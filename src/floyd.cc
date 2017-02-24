#include "include/floyd.h"

#include "include/node.h"
#include "third/slash/include/slash_binlog.h"
#include "third/pink/include/bg_thread.h"

namespace floyd {

Floyd::Floyd() 
  : state_(kFollower),
    heart_beat_(new BGThread()),
{
  leveldb::Options options;
  db_ = leveldb::DB::Open(options, "./floyd/db/", &db_);

  // binlog_writer = slash::Binlog::Open("./floyd/binlog/", &binlog_writer);
  // binlog_reader = slash::BinlogReaderImpl();
}

Floyd::~Floyd() {
}

Status Floyd::Write(const std::string &key, const std::string &value) {
}

Status Floyd::Read(const std::string &key, std::string *value) {
}

Status Floyd::RunRaft() {
  heart_beat_->StartThread();
}

void Floyd::TryBeLeader() {
  if (state_ == kLeader) {
    return ;
  } else if (state_ == kFollower) {
    RequestVote();
    return ;
  } else if (state_ == kCandidate) {
  }
}

void Floyd::RequestVote() {
  for (int i = 0; i < n; i++) {
  }
}

void Floyd::AppendEntries() {
}

};
