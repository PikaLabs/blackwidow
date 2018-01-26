//  Copyright (c) 2017-present The blackwidow Authors.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.

#include "blackwidow/blackwidow.h"

#include "src/redis_strings.h"
#include "src/redis_hashes.h"

namespace blackwidow {

BlackWidow::BlackWidow() :
    strings_db_(nullptr),
    hashes_db_(nullptr) {
}

BlackWidow::~BlackWidow() {
  delete strings_db_;
  delete hashes_db_;
}

Status BlackWidow::Compact() {
  Status s = strings_db_->CompactRange(NULL, NULL);
  if (!s.ok()) {
    return s;
  }
  return hashes_db_->CompactRange(NULL, NULL);
}

static std::string AppendSubDirectory(const std::string& db_path,
    const std::string& sub_db) {
  if (db_path.back() == '/') {
    return db_path + sub_db;
  } else {
    return db_path + "/" + sub_db;
  }
}

Status BlackWidow::Open(const rocksdb::Options& options,
    const std::string& db_path) {
  strings_db_ = new RedisStrings();
  Status s = strings_db_->Open(options, AppendSubDirectory(db_path, "strings"));
  hashes_db_ = new RedisHashes();
  s = hashes_db_->Open(options, AppendSubDirectory(db_path, "hashes"));
  return s;
}

// Strings Commands
Status BlackWidow::Set(const Slice& key, const Slice& value) {
  return strings_db_->Set(key, value);
}

Status BlackWidow::Get(const Slice& key, std::string* value) {
  return strings_db_->Get(key, value);
}

Status BlackWidow::MSet(const std::vector<BlackWidow::KeyValue>& kvs) {
  return strings_db_->MSet(kvs);
}

Status BlackWidow::MGet(const std::vector<Slice>& keys,
                        std::vector<std::string>* values) {
  return strings_db_->MGet(keys, values);
}

Status BlackWidow::Setnx(const Slice& key, const Slice& value, int32_t* ret) {
  return strings_db_->Setnx(key, value, ret);
}

Status BlackWidow::Setrange(const Slice& key, int32_t offset,
                            const Slice& value, int32_t* ret) {
  return strings_db_->Setrange(key, offset, value, ret);
}

Status BlackWidow::Append(const Slice& key, const Slice& value, int32_t* ret) {
  return strings_db_->Append(key, value, ret);
}

Status BlackWidow::BitCount(const Slice& key, int32_t start_offset,
                            int32_t end_offset, int32_t *ret, bool have_range) {
  return strings_db_->BitCount(key, start_offset, end_offset, ret, have_range);
}

Status BlackWidow::Decrby(const Slice& key, int64_t value, int64_t* ret) {
  return strings_db_->Decrby(key, value, ret);
}

Status BlackWidow::Setex(const Slice& key, const Slice& value, int32_t ttl) {
  return strings_db_->Setex(key, value, ttl);
}

Status BlackWidow::Strlen(const Slice& key, int32_t* len) {
  return strings_db_->Strlen(key, len);
}

// Hashes Commands
Status BlackWidow::HSet(const Slice& key, const Slice& field,
    const Slice& value, int32_t* res) {
  return hashes_db_->HSet(key, field, value, res);
}

Status BlackWidow::HGet(const Slice& key, const Slice& field,
    std::string* value) {
  return hashes_db_->HGet(key, field, value);
}

Status BlackWidow::HExists(const Slice& key, const Slice& field) {
  return hashes_db_->HExists(key, field);
}

// Keys Commands
Status BlackWidow::Expire(const Slice& key, int32_t ttl) {
  Status s = strings_db_->Expire(key, ttl);
  if (!s.ok() && !s.IsNotFound()) {
    return s;
  }
  s = hashes_db_->Expire(key, ttl);
  return s;
}

Status BlackWidow::Delete(const Slice& key) {
  Status s = strings_db_->Delete(key);
  if (!s.ok() && !s.IsNotFound()) {
    return s;
  }
  s = hashes_db_->Delete(key);
  return s;
}

}  //  namespace blackwidow
