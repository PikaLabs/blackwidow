//  Copyright (c) 2017-present The blackwidow Authors.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.

#include <gtest/gtest.h>
#include <thread>
#include <iostream>

#include "blackwidow/blackwidow.h"

using namespace blackwidow;

class SetesTest : public ::testing::Test {
  public:
    SetesTest() {
      std::string path = "./db";
      if (access(path.c_str(), F_OK)) {
        mkdir(path.c_str(), 0755);
      }
      options.create_if_missing = true;
      s = db.Open(options, path);
    }
    virtual ~SetesTest() { }

    static void SetUpTestCase() { }
    static void TearDownTestCase() { }

    blackwidow::Options options;
    blackwidow::BlackWidow db;
    blackwidow::Status s;
};

// SAdd
TEST_F(SetesTest, SAddTest) {
  int32_t ret = 0;
  std::vector<std::string> members1 {"MM1", "MM2", "MM3", "MM2"};
  s = db.SAdd("SADD_KEY", members1, &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 3);
  s = db.SCard("SADD_KEY", &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 3);

  std::vector<std::string> members2 {"MM4", "MM5"};
  s = db.SAdd("SADD_KEY", members2, &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 2);
  s = db.SCard("SADD_KEY", &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 5);

  std::map<BlackWidow::DataType, rocksdb::Status> type_status;
  db.Expire("SADD_KEY", 1, &type_status);
  ASSERT_TRUE(type_status[BlackWidow::DataType::kSetes].ok());

  // The key has timeout
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  s = db.SCard("SADD_KEY", &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 0);

  std::vector<std::string> members3 {"MM7", "MM8"};
  s = db.SAdd("SADD_KEY", members3, &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 2);
  s = db.SCard("SADD_KEY", &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 2);

  // Delete the key
  std::vector<rocksdb::Slice> del_keys = {"SADD_KEY"};
  type_status.clear();
  db.Del(del_keys, &type_status);
  ASSERT_TRUE(type_status[BlackWidow::DataType::kSetes].ok());
  s = db.SCard("SADD_KEY", &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 0);

  std::vector<std::string> members4 {"MM9", "MM10", "MM11"};
  s = db.SAdd("SADD_KEY", members4, &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 3);
  s = db.SCard("SADD_KEY", &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 3);
}

// SCard
TEST_F(SetesTest, SCardTest) {
  int32_t ret = 0;
  std::vector<std::string> members {"MM1", "MM2", "MM3"};
  s = db.SAdd("SCARD_KEY", members, &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 3);
  s = db.SCard("SCARD_KEY", &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 3);
}

// SIsmember
TEST_F(SetesTest, SIsmemberTest) {
  int32_t ret = 0;
  std::vector<std::string> members {"MEMBER"};
  s = db.SAdd("SISMEMBER_KEY", members, &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 1);

  // Not exist set key
  s = db.SIsmember("SISMEMBER_NOT_EXIST_KEY", "MEMBER", &ret);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_EQ(ret, 0);

  // Not exist set member
  s = db.SIsmember("SISMEMBER_KEY", "NOT_EXIST_MEMBER", &ret);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_EQ(ret, 0);

  s = db.SIsmember("SISMEMBER_KEY", "MEMBER", &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 1);

  // Expire set key
  std::map<BlackWidow::DataType, rocksdb::Status> type_status;
  db.Expire("SISMEMBER_KEY", 1, &type_status);
  ASSERT_TRUE(type_status[BlackWidow::DataType::kSetes].ok());
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  s = db.SIsmember("SISMEMBER_KEY", "MEMBER", &ret);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_EQ(ret, 0);
}

// SMembers
TEST_F(SetesTest, SMembersTest) {
  int32_t ret = 0;
  std::vector<std::string> mid_members_in;
  mid_members_in.push_back("MID_MEMBER1");
  mid_members_in.push_back("MID_MEMBER2");
  mid_members_in.push_back("MID_MEMBER3");
  s = db.SAdd("B_SMEMBERS_KEY", mid_members_in, &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 3);

  std::vector<std::string> members_out;
  s = db.SMembers("B_SMEMBERS_KEY", &members_out);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(members_out.size(), 3);
  ASSERT_TRUE(find(members_out.begin(), members_out.end(), "MID_MEMBER1") != members_out.end());
  ASSERT_TRUE(find(members_out.begin(), members_out.end(), "MID_MEMBER2") != members_out.end());
  ASSERT_TRUE(find(members_out.begin(), members_out.end(), "MID_MEMBER3") != members_out.end());

  // Insert some kv who's position above "mid kv"
  std::vector<std::string> pre_members_in;
  pre_members_in.push_back("PRE_MEMBER1");
  pre_members_in.push_back("PRE_MEMBER2");
  pre_members_in.push_back("PRE_MEMBER3");
  s = db.SAdd("A_SMEMBERS_KEY", pre_members_in, &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 3);
  members_out.clear();
  s = db.SMembers("B_SMEMBERS_KEY", &members_out);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(members_out.size(), 3);
  ASSERT_TRUE(find(members_out.begin(), members_out.end(), "MID_MEMBER1") != members_out.end());
  ASSERT_TRUE(find(members_out.begin(), members_out.end(), "MID_MEMBER2") != members_out.end());
  ASSERT_TRUE(find(members_out.begin(), members_out.end(), "MID_MEMBER3") != members_out.end());

  // Insert some kv who's position below "mid kv"
  std::vector<std::string> suf_members_in;
  suf_members_in.push_back("SUF_MEMBER1");
  suf_members_in.push_back("SUF_MEMBER2");
  suf_members_in.push_back("SUF_MEMBER3");
  s = db.SAdd("C_SMEMBERS_KEY", suf_members_in, &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 3);
  members_out.clear();
  s = db.SMembers("B_SMEMBERS_KEY", &members_out);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(members_out.size(), 3);
  ASSERT_TRUE(find(members_out.begin(), members_out.end(), "MID_MEMBER1") != members_out.end());
  ASSERT_TRUE(find(members_out.begin(), members_out.end(), "MID_MEMBER2") != members_out.end());
  ASSERT_TRUE(find(members_out.begin(), members_out.end(), "MID_MEMBER3") != members_out.end());

  // SMembers timeout setes
  members_out.clear();
  std::map<BlackWidow::DataType, rocksdb::Status> type_status;
  db.Expire("B_SMEMBERS_KEY", 1, &type_status);
  ASSERT_TRUE(type_status[BlackWidow::DataType::kSetes].ok());
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  s = db.SMembers("B_SMEMBERS_KEY", &members_out);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_EQ(members_out.size(), 0);

  // SMembers not exist setes
  members_out.clear();
  s = db.SMembers("SMEMBERS_NOT_EXIST_KEY", &members_out);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_EQ(members_out.size(), 0);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
