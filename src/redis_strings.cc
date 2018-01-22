//  Copyright (c) 2017-present The blackwidow Authors.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.

#include "src/redis_strings.h"

#include <memory>
#include <climits>

#include "src/strings_filter.h"
#include "src/scope_record_lock.h"

namespace blackwidow {

Status RedisStrings::Open(const rocksdb::Options& options,
    const std::string& db_path) {
  rocksdb::Options ops(options);
  ops.compaction_filter_factory = std::make_shared<StringsFilterFactory>();
  return rocksdb::DB::Open(ops, db_path, &db_);
}

Status RedisStrings::Set(const Slice& key, const Slice& value) {
  InternalStringsValue internal_value(value);
  ScopeRecordLock l(lock_mgr_, key);
  return db_->Put(default_write_options_, key, internal_value.Encode());
}

Status RedisStrings::Get(const Slice& key, std::string* value) {
  Status s = db_->Get(default_read_options_, key, value);
  if (s.ok()) {
    ParsedInternalStringsValue internal_value(value);
    if (internal_value.IsStale()) {
      value->clear();
      return Status::NotFound("Stale");
    } else {
      internal_value.StripSuffix();
    }
  }
  return s;
}

Status RedisStrings::Setnx(const Slice& key, const Slice& value, int32_t* ret) {
  *ret = 0;
  std::string old_value;
  ScopeRecordLock l(lock_mgr_, key);
  Status s = db_->Get(default_read_options_, key, &old_value);
  if (s.ok()) {
    ParsedInternalStringsValue internal_value(&old_value);
    if (internal_value.IsStale()) {
      InternalStringsValue new_value(value);
      s = db_->Put(default_write_options_, key, new_value.Encode());
      if (s.ok()) {
        *ret = 1;
      }
    }
  } else if (s.IsNotFound()) {
    InternalStringsValue new_value(value);
    s = db_->Put(default_write_options_, key, new_value.Encode());
    if (s.ok()) {
      *ret = 1;
    }
  }
  return s;
}

Status RedisStrings::Setrange(const Slice& key, int32_t offset, const Slice& value, int32_t* ret) {
  std::string old_value;
  std::string new_value;
  if (offset < 0) {
    return Status::Corruption("offset < 0");
  }

  if (value.size() + offset > (1<<29)) {
    return Status::Corruption("too big");
  }
  ScopeRecordLock l(lock_mgr_, key);
  Status s = db_->Get(default_read_options_, key, &old_value);
  if (s.ok()) {
    ParsedInternalStringsValue internal_value(&old_value);
    internal_value.StripSuffix();
    if (internal_value.IsStale()) {
      std::string tmp(offset, '\0');
      new_value = tmp.append(value.data());
      *ret = new_value.length();
    } else {
      if (static_cast<size_t>(offset) > old_value.length()) {
        old_value.resize(offset);
        new_value = old_value.append(value.data());
      } else {
        std::string head = old_value.substr(0, offset);
        std::string tail;
        if (offset + value.size() - 1 < old_value.length() - 1) {
          tail = old_value.substr(offset + value.size());
        }
        new_value = head + value.data() + tail;
      }
    }
    *ret = new_value.length();
    InternalStringsValue val(new_value);
    return db_->Put(default_write_options_, key, val.Encode());
  } else if (s.IsNotFound()) {
    std::string tmp(offset, '\0');
    new_value = tmp.append(value.data());
    *ret = new_value.length();
    InternalStringsValue val(new_value);
    return db_->Put(default_write_options_, key, val.Encode());
  }
  return s;
}

Status RedisStrings::Append(const Slice& key, const Slice& value, int32_t* ret) {
  std::string old_value;
  *ret = 0;
  ScopeRecordLock l(lock_mgr_, key);
  Status s = db_->Get(default_read_options_, key, &old_value);
  if (s.ok()) {
    ParsedInternalStringsValue internal_value(&old_value);
    if (internal_value.IsStale()) {
      *ret = value.size();
      InternalStringsValue new_value(value);
      return db_->Put(default_write_options_, key, new_value.Encode());
    } else {
      internal_value.StripSuffix();
      *ret = old_value.size() + value.size();
      old_value += value.data();
      InternalStringsValue new_value(old_value);
      return db_->Put(default_write_options_, key, new_value.Encode());
    }
  } else if (s.IsNotFound()) {
    *ret = value.size();
    InternalStringsValue internal_value(value);
    return db_->Put(default_write_options_, key, internal_value.Encode());
  }
  return s;
}

int GetBitCount(const unsigned char* value, int64_t bytes) {
  int bit_num = 0;
  static const unsigned char bitsinbyte[256] = {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8};
  for (int i = 0; i < bytes; i++) {
    bit_num += bitsinbyte[static_cast<unsigned int>(value[i])];
  }
  return bit_num;
}

Status RedisStrings::BitCount(const Slice& key, int32_t start_offset, int32_t end_offset, int32_t* ret, bool have_range) {
  *ret = 0;
  std::string value;
  Status s = db_->Get(default_read_options_, key, &value);
  if (s.ok()) {
    ParsedInternalStringsValue internal_value(&value);
    if (internal_value.IsStale()) {
      return Status::NotFound("Stale");
    } else {
      const unsigned char* bit_value = reinterpret_cast<const unsigned char*>(value.data());
      int64_t value_length = value.length();
      if (have_range) {
        if (start_offset < 0) {
          start_offset = start_offset + value_length;
        }
        if (end_offset < 0) {
          end_offset = end_offset + value_length;
        }
        if (start_offset < 0) {
          start_offset = 0;
        }
        if (end_offset < 0) {
          end_offset = 0;
        }

        if (end_offset >= value_length) {
          end_offset = value_length -1;
        }
        if (start_offset > end_offset) {
          return Status::OK();
        }
      } else {
        start_offset = 0;
        end_offset = std::max(value_length - 1, (int64_t)0);
      }
      *ret = GetBitCount(bit_value + start_offset, end_offset - start_offset + 1);
    }
  } else {
    return s;
  }
  return Status::OK();
}

Status RedisStrings::Decrby(const Slice& key, int64_t value, int64_t* ret) {
  std::string old_value;
  std::string new_value;
  ScopeRecordLock l(lock_mgr_, key);
  Status s = db_->Get(default_read_options_, key, &old_value);
  if (s.ok()) {
    ParsedInternalStringsValue parsed(&old_value);
    if (parsed.IsStale()) {
      *ret = -value;
      new_value = std::to_string(*ret);
      InternalStringsValue internal_value(new_value);
      return db_->Put(default_write_options_, key, internal_value.Encode());
    } else {
      parsed.StripSuffix();
      char* end = nullptr;
      int64_t ival = strtoll(old_value.c_str(), &end, 10);
      if (*end != 0) {
        return Status::Corruption("value is not a integer");
      }
      if ((value >= 0 && LLONG_MIN + value > ival) || (value < 0 && LLONG_MAX + value < ival)) {
        return Status::InvalidArgument("Overflow");
      }
      *ret = ival - value;
      new_value = std::to_string(*ret);
      InternalStringsValue internal_value(new_value);
      return db_->Put(default_write_options_, key, internal_value.Encode());
    }
  } else if (s.IsNotFound()) {
    *ret = -value;
    new_value = std::to_string(*ret);
    InternalStringsValue internal_value(new_value);
    return db_->Put(default_write_options_, key, internal_value.Encode());
  } else {
    return s;
  }
}

Status RedisStrings::Expire(const Slice& key, int32_t ttl) {
  std::string value;
  ScopeRecordLock l(lock_mgr_, key);
  Status s = db_->Get(default_read_options_, key, &value);
  if (s.ok()) {
    ParsedInternalStringsValue parsed(&value);
    if (parsed.IsStale()) {
      return Status::NotFound("Stale");
    }

    if (ttl > 0) {
      parsed.SetRelativeTimestamp(ttl);
      return db_->Put(default_write_options_, key, value);
    } else {
      return db_->Delete(default_write_options_, key);
    }
  }
  return s;
}

Status RedisStrings::CompactRange(const rocksdb::Slice* begin,
    const rocksdb::Slice* end) {
  return db_->CompactRange(default_compact_range_options_, begin, end);
}

}  //  namespace blackwidow
