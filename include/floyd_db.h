#ifndef __FLOYD_DB_H__
#define __FLOYD_DB_H__

#include <iostream>
#include <string>

#include <leveldb/db.h>
#include <leveldb/write_batch.h>

#include "status.h"

namespace floyd {

class LeveldbBackend {
 public:
  LeveldbBackend(std::string path) { dbPath = path; }

  virtual ~LeveldbBackend() { delete db; }

  Status Open();

  Status Get(const std::string& key, std::string& value);

  Status GetAll(std::map<std::string, std::string>& kvMap);

  Status Set(const std::string& key, const std::string& value);

  Status Delete(const std::string& key);

  virtual int LockIsAvailable(std::string& user, std::string& key);

  virtual Status LockKey(const std::string& user, const std::string& key);

  virtual Status UnLockKey(const std::string& user, const std::string& key);

  virtual Status DeleteUserLocks(const std::string& user);

 private:
  leveldb::DB* db;
  std::string dbPath;
};

}  // namespace floyd
#endif
