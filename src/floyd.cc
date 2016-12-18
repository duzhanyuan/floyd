#include "include/floyd.h"

namespace floyd {

Floyd::Floyd() {
}

Floyd::~Floyd() {
}

Status Floyd::Write(const std::string &key, const std::string &value) {
}

Status Floyd::Read(const std::string &key, std::string *value) {
}

Status Floyd::RunRaft() {
  heart_beat_->Start();
}

void Floyd::TryBeLeader() {
  if (state_ == kLeader) {
    return ;
  } else if (state_ == kFollower) {
    return ;
  } else {
    RequestVote();
  }
}

void Floyd::RequestVote() {
}

void Floyd::AppendEntries() {
}

};
