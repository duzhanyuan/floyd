#ifndef FLOYD_INCLUDE_FLOYD_H_
#define FLOYD_INCLUDE_FLOYD_H_

#include <string.h>
#include <vector>

#include "third/pink/include/pink_thread.h"
#include "third/slash/include/slash_status.h"
#include "third/pink/include/bg_thread.h"

namespace floyd {

typedef slash::Status Status;

class Binlog;
class Node;

class Floyd {
 public:
  Floyd();
  ~Floyd();
  Status Write(const std::string &key, const std::string &value);
  Status Read(const std::string &key, std::string *value);

  Status RunRaft();

  std::vector<Node> nodes;

 private:
  void TryBeLeader();
  void RequestVote();
  void AppendEntries();

  enum State {
    kFollower = 0,
    kCandidate = 1,
    kLeader = 2,
  };

  State state_;
  BGThread *heart_beat_;
  leveldb::Leveldb *db_;
  Binlog binlog_;

};

}  // namespace floyd

#endif  // FLOYD_INCLUDE_FLOYD_H_
