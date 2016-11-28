#include "include/peer_thread.h"

namespace floyd {

PeerThread::PeerThread(RaftConsensus* raft_con, NodeInfo* ni)
    : raft_con_(raft_con),
      ni_(ni),
      have_vote_(false),
      vote_done_(false),
      next_index_(1),
      last_agree_index_(0),
      exiting_(false) {
  struct timeval now;
  gettimeofday(&now, NULL);
  next_heartbeat_time_.tv_sec = now.tv_sec;
  next_heartbeat_time_.tv_nsec = now.tv_usec * 1000;
}

PeerThread::~PeerThread() {}

void* PeerThread::ThreadMain() {
  struct timespec when;
  struct timeval now;

  gettimeofday(&now, NULL);
  when.tv_sec = now.tv_sec;
  when.tv_nsec = now.tv_usec * 1000;

  MutexLock l(&raft_con_->mutex_);
  while (!raft_con_->exiting_) {
    gettimeofday(&now, NULL);

    switch (raft_con_->state_) {
      case RaftConsensus::State::FOLLOWER:
        when.tv_sec = std::numeric_limits<time_t>::max();
        when.tv_nsec = 0;
        break;

      case RaftConsensus::State::CANDIDATE:
        if (!vote_done_) {
          if (!RequestVote()) {
            ni_->UpHoldWorkerCliConn(true);
          }
        } else {
          when.tv_sec = std::numeric_limits<time_t>::max();
          when.tv_nsec = 0;
        }
        break;

      case RaftConsensus::State::LEADER:
        bool heartbeat_timeout = false;
        // printf ("PeerThread node(%s:%d) heart(%ld.%09ld) now(%ld.%09ld)\n",
        // ni_->ip.c_str(), ni_->port, next_heartbeat_time_.tv_sec,
        // next_heartbeat_time_.tv_nsec, now.tv_sec, now.tv_usec * 1000);

        if (next_heartbeat_time_.tv_sec < now.tv_sec ||
            (next_heartbeat_time_.tv_sec == now.tv_sec &&
             next_heartbeat_time_.tv_nsec < now.tv_usec * 1000)) {
          heartbeat_timeout = true;
        }
        // printf (" heartbeat_timeout %s\n", heartbeat_timeout ? "true" :
        // "false");

        if (GetLastAgreeIndex() < raft_con_->log_->GetLastLogIndex() ||
            heartbeat_timeout) {
          // appendEntry 往其他发送 Entry 消息
          // 即使没有写入数据也发送类似ping pong 消息
          if (!AppendEntries()) {
            // printf ("PeerThread node(%s:%d) AppendEntries failed\n",
            // ni_->ip.c_str(), ni_->port);
            if (heartbeat_timeout) {
              // printf ("PeerThread node(%s:%d) AppendEntries failed caz
              // heartbeat_timeout\n", ni_->ip.c_str(), ni_->port);
              bool should_delete_user = false;
              {
                // MutexLock l(&Floyd::nodes_mutex);
                if (ni_->ns == NodeStatus::kUp) {
                  ni_->ns = NodeStatus::kDown;
                  should_delete_user = true;
                }
              }

              if (should_delete_user) {
                // printf ("PeerThread node(%s:%d) AppendEntries failed caz
                // timeout\n", ni_->ip.c_str(), ni_->port);
                RaftConsensus::DeleteUserArg* arg =
                    new RaftConsensus::DeleteUserArg(raft_con_, ni_->ip,
                                                     ni_->port);
                raft_con_->bg_thread_.StartIfNeed();
                raft_con_->bg_thread_.Schedule(&HandleDeleteUserWrapper,
                                               static_cast<void*>(arg));
              }
            }

            ni_->UpHoldWorkerCliConn(true);
          }
        }
        when = next_heartbeat_time_;
        break;
    }

    raft_con_->state_changed_.WaitUntil(when);
  }

  return NULL;
}

void PeerThread::set_next_index(uint64_t next_index) {
  next_index_ = next_index;
}

uint64_t PeerThread::get_next_index() { return next_index_; }

bool PeerThread::RequestVote() {
  floyd::Status ret;
  command::Command cmd;
  cmd.set_type(command::Command::RaftVote);
  floyd::raft::RequestVote* rqv = new floyd::raft::RequestVote();
  rqv->set_ip(raft_con_->options_.local_ip);
  rqv->set_port(raft_con_->options_.local_port);
  rqv->set_term(raft_con_->current_term_);
  rqv->set_last_log_term(raft_con_->GetLastLogTerm());
  rqv->set_last_log_index(raft_con_->log_->GetLastLogIndex());
  cmd.set_allocated_rqv(rqv);

  // printf ("request vote to %s,%d\n",ni_->ip.c_str(),ni_->port);
  ret = ni_->dcc->SendMessage(&cmd);
  if (!ret.ok()) {
    return false;
  }

  command::CommandRes cmd_res;
  ret = ni_->dcc->GetResMessage(&cmd_res);
  if (!ret.ok()) {
    return false;
  }

  if (cmd_res.rsv().term() > raft_con_->current_term_) {
    raft_con_->StepDown(cmd_res.rsv().term());
  } else {
    vote_done_ = true;
    raft_con_->state_changed_.SignalAll();

    if (cmd_res.rsv().granted()) {
      have_vote_ = true;
      if (raft_con_->QuorumAll(&PeerThread::HaveVote))
        raft_con_->BecomeLeader();
    } else {
      //@todo: printf denied information
    }
  }

  return true;
}

void PeerThread::BeginRequestVote() {
  vote_done_ = false;
  have_vote_ = false;
}

void PeerThread::BeginLeaderShip() {
  next_index_ = raft_con_->log_->GetLastLogIndex() + 1;
  last_agree_index_ = 0;
}

bool PeerThread::HaveVote() { return have_vote_; }

uint64_t RaftConsensus::PeerThread::GetLastAgreeIndex() {
  return last_agree_index_;
}

bool PeerThread::AppendEntries() {
  uint64_t last_log_index = raft_con_->log_->GetLastLogIndex();
  uint64_t prev_log_index = next_index_ - 1;
  uint64_t prev_log_term;
  struct timeval now;
  if (prev_log_index > last_log_index) {
    return false;
  }
  assert(prev_log_index <= last_log_index);
  if (prev_log_index == 0)
    prev_log_term = 0;
  else
    prev_log_term = raft_con_->log_->GetEntry(prev_log_index).term();

  command::Command cmd;
  floyd::raft::AppendEntriesRequest* aerq =
      new floyd::raft::AppendEntriesRequest();
  cmd.set_type(command::Command::RaftAppendEntries);
  aerq->set_ip(raft_con_->options_.local_ip);
  aerq->set_port(raft_con_->options_.local_port);
  aerq->set_term(raft_con_->current_term_);
  aerq->set_prev_log_index(prev_log_index);
  aerq->set_prev_log_term(prev_log_term);
  uint64_t num_entries = 0;
  for (uint64_t index = next_index_; index <= last_log_index; ++index) {
    Log::Entry& entry = raft_con_->log_->GetEntry(index);
    *aerq->add_entries() = entry;
    uint64_t request_size = aerq->ByteSize();
    if (request_size < raft_con_->options_.append_entries_size_once ||
        num_entries == 0)
      ++num_entries;
    else
      aerq->mutable_entries()->RemoveLast();
  }
  aerq->set_commit_index(
      std::min(raft_con_->commit_index_, prev_log_index + num_entries));
  cmd.set_allocated_aerq(aerq);

  floyd::Status ret = ni_->dcc->SendMessage(&cmd);
  if (!ret.ok()) {
    gettimeofday(&now, NULL);
    next_heartbeat_time_.tv_sec = now.tv_sec + raft_con_->period_.tv_sec;
    next_heartbeat_time_.tv_nsec =
        now.tv_usec * 1000 + raft_con_->period_.tv_nsec;
    ni_->UpHoldWorkerCliConn(true);
    return false;
  }

  command::CommandRes cmd_res;
  ret = ni_->dcc->GetResMessage(&cmd_res);
  if (!ret.ok()) {
    gettimeofday(&now, NULL);
    next_heartbeat_time_.tv_sec = now.tv_sec + raft_con_->period_.tv_sec;
    next_heartbeat_time_.tv_nsec =
        now.tv_usec * 1000 + raft_con_->period_.tv_nsec;
    return false;
  }

  if (cmd_res.aers().term() > raft_con_->current_term_) {
    raft_con_->StepDown(cmd_res.aers().term());
  } else {
    if (cmd_res.aers().status()) {
      last_agree_index_ = prev_log_index + num_entries;
      raft_con_->AdvanceCommitIndex();
      next_index_ = last_agree_index_ + 1;
    } else {
      if (next_index_ > 1) --next_index_;
    }
  }
  gettimeofday(&now, NULL);
  next_heartbeat_time_.tv_sec = now.tv_sec + raft_con_->period_.tv_sec;
  next_heartbeat_time_.tv_nsec =
      now.tv_usec * 1000 + raft_con_->period_.tv_nsec;

  return true;
}

}  // namespace floyd
