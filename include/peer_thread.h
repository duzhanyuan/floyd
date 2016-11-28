#ifndef FLOYD_INCLUDE_PEER_THREAD_H_
#define FLOYD_INCLUDE_PEER_THREAD_H_

#include "pink/include/pink_thread.h"

namespace floyd {

class PeerThread : public pink::Thread {
 public:
  PeerThread(RaftConsensus* raft_con, NodeInfo* ni);
  ~PeerThread();

  virtual void* ThreadMain();
  bool HaveVote();
  uint64_t GetLastAgreeIndex();
  void BeginRequestVote();
  void BeginLeaderShip();
  void set_next_index(uint64_t next_index);
  uint64_t get_next_index();

 private:
  RaftConsensus* raft_con_;
  NodeInfo* ni_;
  bool have_vote_;
  bool vote_done_;
  uint64_t next_index_;
  uint64_t last_agree_index_;
  struct timespec next_heartbeat_time_;
  bool exiting_;

  bool RequestVote();
  bool AppendEntries();
};

}  // namespace floyd

#endif  // FLOYD_INCLUDE_PEER_THREAD_H_
