//  Copyright (c) 2017-present The blackwidow Authors.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.

#include <gtest/gtest.h>
#include <thread>
#include <iostream>

#include "blackwidow/blackwidow.h"

using namespace blackwidow;

static bool elements_match(blackwidow::BlackWidow *const db,
                           const Slice& key,
                           const std::vector<std::string>& expect_elements) {
  std::vector<std::string> elements_out;
  Status s = db->LRange(key, 0, -1, &elements_out);
  if (!s.ok() && !s.IsNotFound()) {
    return false;
  }
  if (elements_out.size() != expect_elements.size()) {
    return false;
  }
  if (s.IsNotFound() && expect_elements.empty()) {
    return true;
  }
  for (uint64_t idx = 0; idx < elements_out.size(); ++idx) {
    if (strcmp(elements_out[idx].c_str(), expect_elements[idx].c_str())) {
      return false;
    }
  }
  return true;
}

static bool elements_match(const std::vector<std::string>& elements_out,
                           const std::vector<std::string>& expect_elements) {
  if (elements_out.size() != expect_elements.size()) {
    return false;
  }
  for (uint64_t idx = 0; idx < elements_out.size(); ++idx) {
    if (strcmp(elements_out[idx].c_str(), expect_elements[idx].c_str())) {
      return false;
    }
  }
  return true;
}

static bool len_match(blackwidow::BlackWidow *const db,
                      const Slice& key,
                      uint64_t expect_len) {
  uint64_t len = 0;
  Status s = db->LLen(key, &len);
  if (!s.ok() && !s.IsNotFound()) {
    return false;
  }
  if (s.IsNotFound() && !expect_len) {
    return true;
  }
  return len == expect_len;
}

static bool make_expired(blackwidow::BlackWidow *const db,
                         const Slice& key) {
  std::map<BlackWidow::DataType, rocksdb::Status> type_status;
  int ret = db->Expire(key, 1, &type_status);
  if (!ret || !type_status[BlackWidow::DataType::kLists].ok()) {
    return false;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  return true;
}

class ListsTest : public ::testing::Test {
 public:
  ListsTest() {
    std::string path = "./db/lists";
    if (access(path.c_str(), F_OK)) {
      mkdir(path.c_str(), 0755);
    }
    options.create_if_missing = true;
    s = db.Open(options, path);
  }
  virtual ~ListsTest() { }

  static void SetUpTestCase() { }
  static void TearDownTestCase() { }

  blackwidow::Options options;
  blackwidow::BlackWidow db;
  blackwidow::Status s;
};

// LPush
TEST_F(ListsTest, LPushTest) {
  uint64_t num;
  std::string element;

  // ***************** Group 1 Test *****************
  //  "s" -> "l" -> "a" -> "s" -> "h"
  std::vector<std::string> gp1_nodes {"h", "s", "a", "l", "s"};
  s = db.LPush("GP1_LPUSH_KEY", gp1_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp1_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP1_LPUSH_KEY", gp1_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP1_LPUSH_KEY", {"s", "l", "a", "s", "h"}));


  // ***************** Group 2 Test *****************
  //  "a" -> "x" -> "l"
  std::vector<std::string> gp2_nodes1 {"l", "x", "a"};
  s = db.LPush("GP2_LPUSH_KEY", gp2_nodes1, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp2_nodes1.size(), num);
  ASSERT_TRUE(len_match(&db, "GP2_LPUSH_KEY", gp2_nodes1.size()));
  ASSERT_TRUE(elements_match(&db, "GP2_LPUSH_KEY", {"a", "x", "l"}));

  // "r" -> "o" -> "s" -> "e"
  std::vector<std::string> gp2_nodes2 {"e", "s", "o", "r"};
  ASSERT_TRUE(make_expired(&db, "GP2_LPUSH_KEY"));
  s = db.LPush("GP2_LPUSH_KEY", gp2_nodes2, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp2_nodes2.size(), num);
  ASSERT_TRUE(len_match(&db, "GP2_LPUSH_KEY", gp2_nodes2.size()));
  ASSERT_TRUE(elements_match(&db, "GP2_LPUSH_KEY", {"r", "o", "s", "e"}));


  // ***************** Group 3 Test *****************
  //  "d" -> "a" -> "v" -> "i" -> "d"
  std::vector<std::string> gp3_nodes1 {"d", "i", "v", "a", "d"};
  s = db.LPush("GP3_LPUSH_KEY", gp3_nodes1, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp3_nodes1.size(), num);
  ASSERT_TRUE(len_match(&db, "GP3_LPUSH_KEY", gp3_nodes1.size()));
  ASSERT_TRUE(elements_match(&db, "GP3_LPUSH_KEY", {"d", "a", "v", "i", "d"}));

  // Delete the key
  std::vector<std::string> del_keys = {"GP3_LPUSH_KEY"};
  std::map<BlackWidow::DataType, blackwidow::Status> type_status;
  db.Del(del_keys, &type_status);
  ASSERT_TRUE(type_status[BlackWidow::DataType::kLists].ok());

  // "g" -> "i" -> "l" -> "m" -> "o" -> "u" -> "r"
  std::vector<std::string> gp3_nodes2 {"r", "u", "o", "m", "l", "i", "g"};
  s = db.LPush("GP3_LPUSH_KEY", gp3_nodes2, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp3_nodes2.size(), num);
  ASSERT_TRUE(len_match(&db, "GP3_LPUSH_KEY", gp3_nodes2.size()));
  ASSERT_TRUE(elements_match(&db, "GP3_LPUSH_KEY", {"g", "i", "l", "m", "o", "u", "r"}));


  // ***************** Group 4 Test *****************
  //  "b" -> "l" -> "u" -> "e"
  std::vector<std::string> gp4_nodes1 {"e", "u", "l", "b"};
  s = db.LPush("GP4_LPUSH_KEY", gp4_nodes1, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp4_nodes1.size(), num);
  ASSERT_TRUE(len_match(&db, "GP4_LPUSH_KEY", gp4_nodes1.size()));
  ASSERT_TRUE(elements_match(&db, "GP4_LPUSH_KEY", {"b", "l", "u", "e"}));

  // "t" -> "h" -> "e" -> " " -> "b" -> "l" -> "u" -> "e"
  std::vector<std::string> gp4_nodes2 {" ", "e", "h", "t"};
  s = db.LPush("GP4_LPUSH_KEY", gp4_nodes2, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(8, num);
  ASSERT_TRUE(len_match(&db, "GP4_LPUSH_KEY", 8));
  ASSERT_TRUE(elements_match(&db, "GP4_LPUSH_KEY", {"t", "h", "e", " ", "b", "l", "u", "e"}));
}

// RPush
TEST_F(ListsTest, RPushTest) {
  uint64_t num;
  std::string element;

  // ***************** Group 1 Test *****************
  //  "s" -> "l" -> "a" -> "s" -> "h"
  std::vector<std::string> gp1_nodes {"s", "l", "a", "s", "h"};
  s = db.RPush("GP1_RPUSH_KEY", gp1_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp1_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP1_RPUSH_KEY", gp1_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP1_RPUSH_KEY", {"s", "l", "a", "s", "h"}));


  // ***************** Group 2 Test *****************
  //  "a" -> "x" -> "l"
  std::vector<std::string> gp2_nodes1 {"a", "x", "l"};
  s = db.RPush("GP2_RPUSH_KEY", gp2_nodes1, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp2_nodes1.size(), num);
  ASSERT_TRUE(len_match(&db, "GP2_RPUSH_KEY", gp2_nodes1.size()));
  ASSERT_TRUE(elements_match(&db, "GP2_RPUSH_KEY", {"a", "x", "l"}));

  // "r" -> "o" -> "s" -> "e"
  std::vector<std::string> gp2_nodes2 {"r", "o", "s", "e"};
  ASSERT_TRUE(make_expired(&db, "GP2_RPUSH_KEY"));
  s = db.RPush("GP2_RPUSH_KEY", gp2_nodes2, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp2_nodes2.size(), num);
  ASSERT_TRUE(len_match(&db, "GP2_RPUSH_KEY", gp2_nodes2.size()));
  ASSERT_TRUE(elements_match(&db, "GP2_RPUSH_KEY", {"r", "o", "s", "e"}));


  // ***************** Group 3 Test *****************
  //  "d" -> "a" -> "v" -> "i" -> "d"
  std::vector<std::string> gp3_nodes1 {"d", "a", "v", "i", "d"};
  s = db.RPush("GP3_RPUSH_KEY", gp3_nodes1, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp3_nodes1.size(), num);
  ASSERT_TRUE(len_match(&db, "GP3_RPUSH_KEY", gp3_nodes1.size()));
  ASSERT_TRUE(elements_match(&db, "GP3_RPUSH_KEY", {"d", "a", "v", "i", "d"}));

  // Delete the key
  std::vector<std::string> del_keys = {"GP3_RPUSH_KEY"};
  std::map<BlackWidow::DataType, blackwidow::Status> type_status;
  db.Del(del_keys, &type_status);
  ASSERT_TRUE(type_status[BlackWidow::DataType::kLists].ok());

  // "g" -> "i" -> "l" -> "m" -> "o" -> "u" -> "r"
  std::vector<std::string> gp3_nodes2 {"g", "i", "l", "m", "o", "u", "r"};
  s = db.RPush("GP3_RPUSH_KEY", gp3_nodes2, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp3_nodes2.size(), num);
  ASSERT_TRUE(len_match(&db, "GP3_RPUSH_KEY", gp3_nodes2.size()));
  ASSERT_TRUE(elements_match(&db, "GP3_RPUSH_KEY", {"g", "i", "l", "m", "o", "u", "r"}));


  // ***************** Group 4 Test *****************
  //  "t" -> "h" -> "e" -> " "
  std::vector<std::string> gp4_nodes1 {"t", "h", "e", " "};
  s = db.RPush("GP4_RPUSH_KEY", gp4_nodes1, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp4_nodes1.size(), num);
  ASSERT_TRUE(len_match(&db, "GP4_RPUSH_KEY", gp4_nodes1.size()));
  ASSERT_TRUE(elements_match(&db, "GP4_RPUSH_KEY", {"t", "h", "e", " "}));

  // "t" -> "h" -> "e" -> " " -> "b" -> "l" -> "u" -> "e"
  std::vector<std::string> gp4_nodes2 {"b", "l", "u", "e"};
  s = db.RPush("GP4_RPUSH_KEY", gp4_nodes2, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(8, num);
  ASSERT_TRUE(len_match(&db, "GP4_RPUSH_KEY", 8));
  ASSERT_TRUE(elements_match(&db, "GP4_RPUSH_KEY", {"t", "h", "e", " ", "b", "l", "u", "e"}));
}

// LRange
TEST_F(ListsTest, LRangeTest) {
  uint64_t num;

  // ***************** Group 1 Test *****************
  //  " " -> "a" -> "t" -> " "
  std::vector<std::string> gp1_nodes1 {" ", "a", "t", " "};
  s = db.RPush("GP1_LRANGE_KEY", gp1_nodes1, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp1_nodes1.size(), num);
  ASSERT_TRUE(len_match(&db, "GP1_LRANGE_KEY", gp1_nodes1.size()));
  ASSERT_TRUE(elements_match(&db, "GP1_LRANGE_KEY", {" ", "a", "t", " "}));

  //  "l" -> "i" -> "v" -> "e" -> " " -> "a" -> "t" -> " "
  std::vector<std::string> gp1_nodes2 {"e", "v", "i", "l"};
  s = db.LPush("GP1_LRANGE_KEY", gp1_nodes2, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp1_nodes1.size() + gp1_nodes2.size(), num);
  ASSERT_TRUE(len_match(&db, "GP1_LRANGE_KEY", gp1_nodes1.size() + gp1_nodes2.size()));
  ASSERT_TRUE(elements_match(&db, "GP1_LRANGE_KEY", {"l", "i", "v", "e", " ", "a", "t", " "}));

  //  "l" -> "i" -> "v" -> "e" -> " " -> "a" -> "t" -> " " -> "p" -> "o" -> "m" -> "p" -> "e" -> "i" -> "i"
  //   0      1      2      3      4      5      6      7      8      9      10     11     12     13     14
  //  -15    -14    -13    -12    -11    -10    -9     -8     -7     -6      -5     -4     -3     -2     -1
  std::vector<std::string> gp1_nodes3 {"p", "o", "m", "p", "e", "i", "i"};
  s = db.RPush("GP1_LRANGE_KEY", gp1_nodes3, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp1_nodes1.size() + gp1_nodes2.size() + gp1_nodes3.size(), num);
  ASSERT_TRUE(len_match(&db, "GP1_LRANGE_KEY", gp1_nodes1.size() + gp1_nodes2.size() + gp1_nodes3.size()));
  ASSERT_TRUE(elements_match(&db, "GP1_LRANGE_KEY", {"l", "i", "v", "e", " ", "a", "t", " ", "p", "o", "m", "p", "e", "i", "i"}));

  std::vector<std::string> gp1_range_nodes;
  s = db.LRange("GP1_LRANGE_KEY", 0, -1, &gp1_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp1_range_nodes, {"l", "i", "v", "e", " ", "a", "t", " ", "p", "o", "m", "p", "e", "i", "i"}));

  gp1_range_nodes.clear();
  s = db.LRange("GP1_LRANGE_KEY", 0, 14, &gp1_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp1_range_nodes, {"l", "i", "v", "e", " ", "a", "t", " ", "p", "o", "m", "p", "e", "i", "i"}));

  gp1_range_nodes.clear();
  s = db.LRange("GP1_LRANGE_KEY", -15, -1, &gp1_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp1_range_nodes, {"l", "i", "v", "e", " ", "a", "t", " ", "p", "o", "m", "p", "e", "i", "i"}));

  gp1_range_nodes.clear();
  s = db.LRange("GP1_LRANGE_KEY", 0, 100, &gp1_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp1_range_nodes, {"l", "i", "v", "e", " ", "a", "t", " ", "p", "o", "m", "p", "e", "i", "i"}));

  gp1_range_nodes.clear();
  s = db.LRange("GP1_LRANGE_KEY", -100, -1, &gp1_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp1_range_nodes, {"l", "i", "v", "e", " ", "a", "t", " ", "p", "o", "m", "p", "e", "i", "i"}));

  gp1_range_nodes.clear();
  s = db.LRange("GP1_LRANGE_KEY", 5, 6, &gp1_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp1_range_nodes, {"a", "t"}));

  gp1_range_nodes.clear();
  s = db.LRange("GP1_LRANGE_KEY", -10, -9, &gp1_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp1_range_nodes, {"a", "t"}));

  gp1_range_nodes.clear();
  s = db.LRange("GP1_LRANGE_KEY", -10, 6, &gp1_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp1_range_nodes, {"a", "t"}));

  gp1_range_nodes.clear();
  s = db.LRange("GP1_LRANGE_KEY", -15, 6, &gp1_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp1_range_nodes, {"l", "i", "v", "e", " ", "a", "t"}));

  gp1_range_nodes.clear();
  s = db.LRange("GP1_LRANGE_KEY", -100, 6, &gp1_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp1_range_nodes, {"l", "i", "v", "e", " ", "a", "t"}));

  gp1_range_nodes.clear();
  s = db.LRange("GP1_LRANGE_KEY", -15, -9, &gp1_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp1_range_nodes, {"l", "i", "v", "e", " ", "a", "t"}));

  gp1_range_nodes.clear();
  s = db.LRange("GP1_LRANGE_KEY", 8, 14, &gp1_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp1_range_nodes, {"p", "o", "m", "p", "e", "i", "i"}));

  gp1_range_nodes.clear();
  s = db.LRange("GP1_LRANGE_KEY", -7, 14, &gp1_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp1_range_nodes, {"p", "o", "m", "p", "e", "i", "i"}));

  gp1_range_nodes.clear();
  s = db.LRange("GP1_LRANGE_KEY", -7, -1, &gp1_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp1_range_nodes, {"p", "o", "m", "p", "e", "i", "i"}));

  gp1_range_nodes.clear();
  s = db.LRange("GP1_LRANGE_KEY", 8, 100, &gp1_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp1_range_nodes, {"p", "o", "m", "p", "e", "i", "i"}));

  gp1_range_nodes.clear();
  s = db.LRange("GP1_LRANGE_KEY", -100, -50, &gp1_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp1_range_nodes, {}));

  gp1_range_nodes.clear();
  s = db.LRange("GP1_LRANGE_KEY", -100, 0, &gp1_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp1_range_nodes, {"l"}));

  gp1_range_nodes.clear();
  s = db.LRange("GP1_LRANGE_KEY", -100, -15, &gp1_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp1_range_nodes, {"l"}));

  gp1_range_nodes.clear();
  s = db.LRange("GP1_LRANGE_KEY", 15, 100, &gp1_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp1_range_nodes, {}));

  gp1_range_nodes.clear();
  s = db.LRange("GP1_LRANGE_KEY", 14, 100, &gp1_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp1_range_nodes, {"i"}));

  gp1_range_nodes.clear();
  s = db.LRange("GP1_LRANGE_KEY", -1, 100, &gp1_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp1_range_nodes, {"i"}));


  // ***************** Group 2 Test *****************
  //  "a"
  //   0
  //  -1
  std::vector<std::string> gp2_nodes {"a"};
  s = db.RPush("GP2_LRANGE_KEY", gp2_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp2_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP2_LRANGE_KEY", gp2_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP2_LRANGE_KEY", {"a"}));

  std::vector<std::string> gp2_range_nodes;
  s = db.LRange("GP2_LRANGE_KEY", 0, 0, &gp2_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp2_range_nodes, {"a"}));

  gp2_range_nodes.clear();
  s = db.LRange("GP2_LRANGE_KEY", 0, -1, &gp2_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp2_range_nodes, {"a"}));

  gp2_range_nodes.clear();
  s = db.LRange("GP2_LRANGE_KEY", -1, -1, &gp2_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp2_range_nodes, {"a"}));

  gp2_range_nodes.clear();
  s = db.LRange("GP2_LRANGE_KEY", -100, 0, &gp2_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp2_range_nodes, {"a"}));

  gp2_range_nodes.clear();
  s = db.LRange("GP2_LRANGE_KEY", -100, -1, &gp2_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp2_range_nodes, {"a"}));

  gp2_range_nodes.clear();
  s = db.LRange("GP2_LRANGE_KEY", 0, 100, &gp2_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp2_range_nodes, {"a"}));

  gp2_range_nodes.clear();
  s = db.LRange("GP2_LRANGE_KEY", -1, 100, &gp2_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp2_range_nodes, {"a"}));

  gp2_range_nodes.clear();
  s = db.LRange("GP2_LRANGE_KEY", -100, 100, &gp2_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp2_range_nodes, {"a"}));

  gp2_range_nodes.clear();
  s = db.LRange("GP2_LRANGE_KEY", -10, -2, &gp2_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp2_range_nodes, {}));

  gp2_range_nodes.clear();
  s = db.LRange("GP2_LRANGE_KEY", 1, 2, &gp2_range_nodes);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(gp2_range_nodes, {}));


  // ***************** Group 3 Test *****************
  // LRange not exist key
  std::vector<std::string> gp3_range_nodes;
  s = db.LRange("GP3_LRANGE_KEY", 1, 5, &gp3_range_nodes);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_TRUE(elements_match(gp3_range_nodes, {}));


  // ***************** Group 4 Test *****************
  //  "n" -> "o" -> "w"
  //   0      1      2
  //  -3     -2     -1
  // LRange timeout key
  std::vector<std::string> gp4_nodes {"n", "o", "w"};
  s = db.RPush("GP4_LRANGE_KEY", gp4_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp4_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP4_LRANGE_KEY", gp4_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP4_LRANGE_KEY", {"n", "o", "w"}));
  ASSERT_TRUE(make_expired(&db, "GP4_LRANGE_KEY"));

  std::vector<std::string> gp4_range_nodes;
  s = db.LRange("GP4_LRANGE_KEY", 0, 2, &gp4_range_nodes);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_TRUE(elements_match(gp4_range_nodes, {}));


  // ***************** Group 5 Test *****************
  //  "t" -> "o" -> "u" -> "r"
  //   0      1      2     3
  //  -4     -3     -2    -1
  // LRange has been deleted key
  std::vector<std::string> gp5_nodes {"t", "o", "u", "r"};
  s = db.RPush("GP5_LRANGE_KEY", gp5_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp5_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP5_LRANGE_KEY", gp5_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP5_LRANGE_KEY", {"t", "o", "u", "r"}));
  ASSERT_TRUE(make_expired(&db, "GP5_LRANGE_KEY"));

  // Delete the key
  std::vector<std::string> del_keys = {"GP5_LRANGE_KEY"};
  std::map<BlackWidow::DataType, blackwidow::Status> type_status;
  db.Del(del_keys, &type_status);
  ASSERT_TRUE(type_status[BlackWidow::DataType::kLists].ok());

  std::vector<std::string> gp5_range_nodes;
  s = db.LRange("GP5_LRANGE_KEY", 0, 2, &gp5_range_nodes);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_TRUE(elements_match(gp5_range_nodes, {}));
}

// LTrim
TEST_F(ListsTest, LTrimTest) {
  uint64_t num;
  std::vector<std::string> values;
  for (int32_t i = 0; i < 100; i++) {
    values.push_back("LTRIM_VALUE"+ std::to_string(i));
  }
  s = db.RPush("LTRIM_KEY", values, &num);
  ASSERT_EQ(num, values.size());
  ASSERT_TRUE(s.ok());

  std::vector<std::string> result;
  s = db.LRange("LTRIM_KEY", 0, 100, &result);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(result.size(), 100);
  for (int32_t i = 0; i < 100; i++) {
    ASSERT_STREQ(result[i].c_str(), values[i].c_str());
  }
  result.clear();

  s = db.LTrim("LTRIM_KEY", 0, 50);
  ASSERT_TRUE(s.ok());
  s = db.LRange("LTRIM_KEY", 0, 50, &result);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(result.size(), 51);
  for (int32_t i = 0; i < 51; i++) {
    ASSERT_STREQ(result[i].c_str(), values[i].c_str());
  }
  result.clear();
}

// LLen
TEST_F(ListsTest, LLenTest) {
  uint64_t num;

  // ***************** Group 1 Test *****************
  // "l" -> "x" -> "a"
  std::vector<std::string> gp1_nodes {"a", "x", "l"};
  s = db.LPush("GP1_LLEN_KEY", gp1_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp1_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP1_LLEN_KEY", gp1_nodes.size()));

  // The key has timeout
  ASSERT_TRUE(make_expired(&db, "GP1_LLEN_KEY"));
  ASSERT_TRUE(len_match(&db, "GP1_LLEN_KEY", 0));


  // ***************** Group 1 Test *****************
  // "p" -> "e" -> "r" -> "g"
  std::vector<std::string> gp2_nodes {"g", "r", "e", "p"};
  s = db.LPush("GP2_LLEN_KEY", gp2_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp2_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP2_LLEN_KEY", gp2_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP2_LLEN_KEY", {"p", "e", "r", "g"}));

  // Delete the key
  std::vector<std::string> del_keys = {"GP2_LLEN_KEY"};
  std::map<BlackWidow::DataType, blackwidow::Status> type_status;
  db.Del(del_keys, &type_status);
  ASSERT_TRUE(type_status[BlackWidow::DataType::kLists].ok());
  ASSERT_TRUE(len_match(&db, "GP2_LLEN_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP2_LLEN_KEY", {}));
}

// LPop
TEST_F(ListsTest, LPopTest) {
  uint64_t num;
  std::string element;

  // ***************** Group 1 Test *****************
  //  "l" -> "x" -> "a"
  std::vector<std::string> gp1_nodes {"a", "x", "l"};
  s = db.LPush("GP1_LPOP_KEY", gp1_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp1_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP1_LPOP_KEY", gp1_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP1_LPOP_KEY", {"l", "x", "a"}));

  // "x" -> "a"
  s = db.LPop("GP1_LPOP_KEY", &element);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(element, "l");
  ASSERT_TRUE(len_match(&db, "GP1_LPOP_KEY", 2));
  ASSERT_TRUE(elements_match(&db, "GP1_LPOP_KEY", {"x", "a"}));

  // after lpop two element, list will be empty
  s = db.LPop("GP1_LPOP_KEY", &element);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(element, "x");
  s = db.LPop("GP1_LPOP_KEY", &element);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(element, "a");
  ASSERT_TRUE(len_match(&db, "GP1_LPOP_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP1_LPOP_KEY", {}));

  // lpop empty list
  s = db.LPop("GP1_LPOP_KEY", &element);
  ASSERT_TRUE(s.IsNotFound());


  // ***************** Group 2 Test *****************
  //  "p" -> "e" -> "r" -> "g"
  std::vector<std::string> gp2_nodes {"g", "r", "e", "p"};
  s = db.LPush("GP2_LPOP_KEY", gp2_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp2_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP2_LPOP_KEY", gp2_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP2_LPOP_KEY", {"p", "e", "r", "g"}));

  ASSERT_TRUE(make_expired(&db, "GP2_LPOP_KEY"));
  s = db.LPop("GP2_LPOP_KEY", &element);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_TRUE(len_match(&db, "GP2_LPOP_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP2_LPOP_KEY", {}));


  // ***************** Group 3 Test *****************
  // "p" -> "o" -> "m" -> "e" -> "i" -> "i"
  std::vector<std::string> gp3_nodes {"i", "i", "e", "m", "o", "p"};
  s = db.LPush("GP3_LPOP_KEY", gp3_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp3_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP3_LPOP_KEY", gp3_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP3_LPOP_KEY", {"p", "o", "m", "e", "i", "i"}));

  // Delete the key, then try lpop
  std::vector<std::string> del_keys = {"GP3_LPOP_KEY"};
  std::map<BlackWidow::DataType, blackwidow::Status> type_status;
  db.Del(del_keys, &type_status);
  ASSERT_TRUE(type_status[BlackWidow::DataType::kLists].ok());
  ASSERT_TRUE(len_match(&db, "GP3_LPOP_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP3_LPOP_KEY", {}));

  s = db.LPop("GP3_LPOP_KEY", &element);
  ASSERT_TRUE(s.IsNotFound());
}

// RPop
TEST_F(ListsTest, RPopTest) {
  uint64_t num;
  std::string element;

  // ***************** Group 1 Test *****************
  //  "a" -> "x" -> "l"
  std::vector<std::string> gp1_nodes {"l", "x", "a"};
  s = db.LPush("GP1_RPOP_KEY", gp1_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp1_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP1_RPOP_KEY", gp1_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP1_RPOP_KEY", {"a", "x", "l"}));

  // "a" -> "x"
  s = db.RPop("GP1_RPOP_KEY", &element);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(element, "l");
  ASSERT_TRUE(len_match(&db, "GP1_RPOP_KEY", 2));
  ASSERT_TRUE(elements_match(&db, "GP1_RPOP_KEY", {"a", "x"}));

  // After rpop two element, list will be empty
  s = db.RPop("GP1_RPOP_KEY", &element);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(element, "x");
  s = db.RPop("GP1_RPOP_KEY", &element);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(element, "a");
  ASSERT_TRUE(len_match(&db, "GP1_RPOP_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP1_RPOP_KEY", {}));

  // lpop empty list
  s = db.LPop("GP1_RPOP_KEY", &element);
  ASSERT_TRUE(s.IsNotFound());


  // ***************** Group 2 Test *****************
  //  "g" -> "r" -> "e" -> "p"
  std::vector<std::string> gp2_nodes {"p", "e", "r", "g"};
  s = db.LPush("GP2_RPOP_KEY", gp2_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp2_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP2_RPOP_KEY", gp2_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP2_RPOP_KEY", {"g", "r", "e", "p"}));

  ASSERT_TRUE(make_expired(&db, "GP2_RPOP_KEY"));
  s = db.LPop("GP2_RPOP_KEY", &element);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_TRUE(len_match(&db, "GP2_RPOP_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP2_RPOP_KEY", {}));


  // ***************** Group 3 Test *****************
  // "p" -> "o" -> "m" -> "e" -> "i" -> "i"
  std::vector<std::string> gp3_nodes {"i", "i", "e", "m", "o", "p"};
  s = db.LPush("GP3_RPOP_KEY", gp3_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp3_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP3_RPOP_KEY", gp3_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP3_RPOP_KEY", {"p", "o", "m", "e", "i", "i"}));

  // Delete the key, then try lpop
  std::vector<std::string> del_keys = {"GP3_RPOP_KEY"};
  std::map<BlackWidow::DataType, blackwidow::Status> type_status;
  db.Del(del_keys, &type_status);
  ASSERT_TRUE(type_status[BlackWidow::DataType::kLists].ok());
  ASSERT_TRUE(len_match(&db, "GP3_RPOP_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP3_RPOP_KEY", {}));

  s = db.RPop("GP3_LPOP_KEY", &element);
  ASSERT_TRUE(s.IsNotFound());
}

// LIndex
TEST_F(ListsTest, RIndexTest) {
  uint64_t num;
  std::string element;

  // ***************** Group 1 Test *****************
  //  "z" -> "e" -> "p" -> "p" -> "l" -> "i" -> "n"
  //   0      1      2      3      4      5      6
  //  -7     -6     -5     -4     -3     -2     -1
  std::vector<std::string> gp1_nodes {"n", "i", "l", "p", "p", "e", "z"};
  s = db.LPush("GP1_LINDEX_KEY", gp1_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp1_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP1_LINDEX_KEY", gp1_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP1_LINDEX_KEY", {"z", "e", "p", "p", "l", "i", "n"}));

  s = db.LIndex("GP1_LINDEX_KEY", 0, &element);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(element, "z");

  s = db.LIndex("GP1_LINDEX_KEY", 4, &element);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(element, "l");

  s = db.LIndex("GP1_LINDEX_KEY", 6, &element);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(element, "n");

  s = db.LIndex("GP1_LINDEX_KEY", 10, &element);
  ASSERT_TRUE(s.IsNotFound());

  s = db.LIndex("GP1_LINDEX_KEY", -1, &element);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(element, "n");

  s = db.LIndex("GP1_LINDEX_KEY", -4, &element);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(element, "p");

  s = db.LIndex("GP1_LINDEX_KEY", -7, &element);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(element, "z");

  s = db.LIndex("GP1_LINDEX_KEY", -10000, &element);
  ASSERT_TRUE(s.IsNotFound());


  // ***************** Group 2 Test *****************
  //  "b" -> "a" -> "t" -> "t" -> "l" -> "e"
  //   0      1      2      3      4      5
  //  -6     -5     -4     -3     -2     -1
  //  LIndex time out list
  std::vector<std::string> gp2_nodes {"b", "a", "t", "t", "l", "e"};
  s = db.RPush("GP2_LINDEX_KEY", gp2_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp2_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP2_LINDEX_KEY", gp2_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP2_LINDEX_KEY", {"b", "a", "t", "t", "l", "e"}));

  ASSERT_TRUE(make_expired(&db, "GP2_LINDEX_KEY"));
  ASSERT_TRUE(len_match(&db, "GP2_LINDEX_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP2_LINDEX_KEY", {}));
  s = db.LIndex("GP2_LINDEX_KEY", 0, &element);
  ASSERT_TRUE(s.IsNotFound());


  // ***************** Group 3 Test *****************
  //  "m" -> "i" -> "s" -> "t" -> "y"
  //   0      1      2      3      4
  //  -5     -4     -3     -2     -1
  //  LIndex the key that has been deleted
  std::vector<std::string> gp3_nodes {"m", "i", "s", "t", "y"};
  s = db.RPush("GP3_LINDEX_KEY", gp3_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp3_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP3_LINDEX_KEY", gp3_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP3_LINDEX_KEY", {"m", "i", "s", "t", "y"}));

  std::vector<std::string> del_keys = {"GP3_LINDEX_KEY"};
  std::map<BlackWidow::DataType, blackwidow::Status> type_status;
  db.Del(del_keys, &type_status);
  ASSERT_TRUE(type_status[BlackWidow::DataType::kLists].ok());
  ASSERT_TRUE(len_match(&db, "GP3_LINDEX_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP3_LINDEX_KEY", {}));

  s = db.LIndex("GP3_LINDEX_KEY", 0, &element);
  ASSERT_TRUE(s.IsNotFound());


  // ***************** Group 4 Test *****************
  //  LIndex not exist key
  s = db.LIndex("GP4_LINDEX_KEY", 0, &element);
  ASSERT_TRUE(s.IsNotFound());
}

// LInsert
TEST_F(ListsTest, LInsertTest) {
  int64_t ret;
  uint64_t num;

  // ***************** Group 1 Test *****************
  // LInsert not exist key
  s = db.LInsert("GP1_LINSERT_KEY", BlackWidow::Before, "pivot", "value", &ret);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_EQ(ret, 0);


  // ***************** Group 2 Test *****************
  //  "w" -> "e" -> "r" -> "u" -> "n"
  // LInsert not exist pivot value
  std::vector<std::string> gp2_nodes {"w", "e", "r", "u", "n"};
  s = db.RPush("GP2_LINSERT_KEY", gp2_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp2_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP2_LINSERT_KEY", gp2_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP2_LINSERT_KEY", {"w", "e", "r", "u", "n"}));

  s = db.LInsert("GP2_LINSERT_KEY", BlackWidow::Before, "pivot", "value", &ret);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_EQ(ret, -1);


  // ***************** Group 3 Test *****************
  //  "a" -> "p" -> "p" -> "l" -> "e"
  // LInsert expire list
  std::vector<std::string> gp3_nodes {"a", "p", "p", "l", "e"};
  s = db.RPush("GP3_LINSERT_KEY", gp3_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp3_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP3_LINSERT_KEY", gp3_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP3_LINSERT_KEY", {"a", "p", "p", "l", "e"}));
  ASSERT_TRUE(make_expired(&db, "GP3_LINSERT_KEY"));

  s = db.LInsert("GP3_LINSERT_KEY", BlackWidow::Before, "pivot", "value", &ret);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_EQ(ret, 0);

  s = db.LInsert("GP3_LINSERT_KEY", BlackWidow::Before, "a", "value", &ret);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_EQ(ret, 0);


  // ***************** Group 4 Test *****************
  //  "a"
  std::vector<std::string> gp4_nodes {"a"};
  s = db.RPush("GP4_LINSERT_KEY", gp4_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp4_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP4_LINSERT_KEY", gp4_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP4_LINSERT_KEY", {"a"}));

  // "x" -> "a"
  s = db.LInsert("GP4_LINSERT_KEY", BlackWidow::Before, "a", "x", &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 2);
  ASSERT_TRUE(len_match(&db, "GP4_LINSERT_KEY", 2));
  ASSERT_TRUE(elements_match(&db, "GP4_LINSERT_KEY", {"x", "a"}));


  // ***************** Group 5 Test *****************
  //  "a"
  std::vector<std::string> gp5_nodes {"a"};
  s = db.RPush("GP5_LINSERT_KEY", gp5_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp5_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP5_LINSERT_KEY", gp5_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP5_LINSERT_KEY", {"a"}));

  // "a" -> "x"
  s = db.LInsert("GP5_LINSERT_KEY", BlackWidow::After, "a", "x", &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 2);
  ASSERT_TRUE(len_match(&db, "GP5_LINSERT_KEY", 2));
  ASSERT_TRUE(elements_match(&db, "GP5_LINSERT_KEY", {"a", "x"}));


  // ***************** Group 6 Test *****************
  //  "a" -> "b"
  std::vector<std::string> gp6_nodes {"a", "b"};
  s = db.RPush("GP6_LINSERT_KEY", gp6_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp6_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP6_LINSERT_KEY", gp6_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP6_LINSERT_KEY", {"a", "b"}));

  // "x" -> "a" -> "b"
  s = db.LInsert("GP6_LINSERT_KEY", BlackWidow::Before, "a", "x", &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 3);
  ASSERT_TRUE(len_match(&db, "GP6_LINSERT_KEY", 3));
  ASSERT_TRUE(elements_match(&db, "GP6_LINSERT_KEY", {"x", "a", "b"}));


  // ***************** Group 7 Test *****************
  //  "a" -> "b"
  std::vector<std::string> gp7_nodes {"a", "b"};
  s = db.RPush("GP7_LINSERT_KEY", gp7_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp7_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP7_LINSERT_KEY", gp7_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP7_LINSERT_KEY", {"a", "b"}));

  // "a" -> "x" -> "b"
  s = db.LInsert("GP7_LINSERT_KEY", BlackWidow::After, "a", "x", &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 3);
  ASSERT_TRUE(len_match(&db, "GP7_LINSERT_KEY", 3));
  ASSERT_TRUE(elements_match(&db, "GP7_LINSERT_KEY", {"a", "x", "b"}));


  // ***************** Group 8 Test *****************
  //  "a" -> "b"
  std::vector<std::string> gp8_nodes {"a", "b"};
  s = db.RPush("GP8_LINSERT_KEY", gp8_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp8_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP8_LINSERT_KEY", gp8_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP8_LINSERT_KEY", {"a", "b"}));

  // "a" -> "x" -> "b"
  s = db.LInsert("GP8_LINSERT_KEY", BlackWidow::Before, "b", "x", &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 3);
  ASSERT_TRUE(len_match(&db, "GP8_LINSERT_KEY", 3));
  ASSERT_TRUE(elements_match(&db, "GP8_LINSERT_KEY", {"a", "x", "b"}));


  // ***************** Group 9 Test *****************
  //  "a" -> "b"
  std::vector<std::string> gp9_nodes {"a", "b"};
  s = db.RPush("GP9_LINSERT_KEY", gp9_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp9_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP9_LINSERT_KEY", gp9_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP9_LINSERT_KEY", {"a", "b"}));

  // "a" -> "b" -> "x"
  s = db.LInsert("GP9_LINSERT_KEY", BlackWidow::After, "b", "x", &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 3);
  ASSERT_TRUE(len_match(&db, "GP9_LINSERT_KEY", 3));
  ASSERT_TRUE(elements_match(&db, "GP9_LINSERT_KEY", {"a", "b", "x"}));


  // ***************** Group 10 Test *****************
  //  "1" -> "2" -> "3"
  std::vector<std::string> gp10_nodes {"1", "2", "3"};
  s = db.RPush("GP10_LINSERT_KEY", gp10_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp10_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP10_LINSERT_KEY", gp10_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP10_LINSERT_KEY", {"1", "2", "3"}));

  // "1" -> "2" -> "4" -> "3"
  s = db.LInsert("GP10_LINSERT_KEY", BlackWidow::After, "2", "4", &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 4);
  ASSERT_TRUE(len_match(&db, "GP10_LINSERT_KEY", 4));
  ASSERT_TRUE(elements_match(&db, "GP10_LINSERT_KEY", {"1", "2", "4", "3"}));

  // "1" -> "2" -> "4" -> "3" -> "5"
  s = db.LInsert("GP10_LINSERT_KEY", BlackWidow::After, "3", "5", &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 5);
  ASSERT_TRUE(len_match(&db, "GP10_LINSERT_KEY", 5));
  ASSERT_TRUE(elements_match(&db, "GP10_LINSERT_KEY", {"1", "2", "4", "3", "5"}));

  // "1" -> "2" -> "4" -> "3" -> "6" -> "5"
  s = db.LInsert("GP10_LINSERT_KEY", BlackWidow::Before, "5", "6", &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 6);
  ASSERT_TRUE(len_match(&db, "GP10_LINSERT_KEY", 6));
  ASSERT_TRUE(elements_match(&db, "GP10_LINSERT_KEY", {"1", "2", "4", "3", "6", "5"}));

  // "7" -> "1" -> "2" -> "4" -> "3" -> "6" -> "5"
  s = db.LInsert("GP10_LINSERT_KEY", BlackWidow::Before, "1", "7", &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 7);
  ASSERT_TRUE(len_match(&db, "GP10_LINSERT_KEY", 7));
  ASSERT_TRUE(elements_match(&db, "GP10_LINSERT_KEY", {"7", "1", "2", "4", "3", "6", "5"}));

  // "7" -> "1" -> "8" -> "2" -> "4" -> "3" -> "6" -> "5"
  s = db.LInsert("GP10_LINSERT_KEY", BlackWidow::After, "1", "8", &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 8);
  ASSERT_TRUE(len_match(&db, "GP10_LINSERT_KEY", 8));
  ASSERT_TRUE(elements_match(&db, "GP10_LINSERT_KEY", {"7", "1", "8", "2", "4", "3", "6", "5"}));

  // "7" -> "1" -> "8" -> "9" -> "2" -> "4" -> "3" -> "6" -> "5"
  s = db.LInsert("GP10_LINSERT_KEY", BlackWidow::Before, "2", "9", &ret);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(ret, 9);
  ASSERT_TRUE(len_match(&db, "GP10_LINSERT_KEY", 9));
  ASSERT_TRUE(elements_match(&db, "GP10_LINSERT_KEY", {"7", "1", "8", "9", "2", "4", "3", "6", "5"}));

}

// LPushx
TEST_F(ListsTest, LPushxTest) {
  int64_t ret;
  uint64_t num;

  // ***************** Group 1 Test *****************
  //  "o" -> "o" -> "o"
  std::vector<std::string> gp1_nodes1 {"o", "o", "o"};
  s = db.RPush("GP1_LPUSHX_KEY", gp1_nodes1, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp1_nodes1.size(), num);
  ASSERT_TRUE(len_match(&db, "GP1_LPUSHX_KEY", gp1_nodes1.size()));
  ASSERT_TRUE(elements_match(&db, "GP1_LPUSHX_KEY", {"o", "o", "o"}));

  //  "x" -> "o" -> "o" -> "o"
  s = db.LPushx("GP1_LPUSHX_KEY", "x", &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(num, 4);
  ASSERT_TRUE(len_match(&db, "GP1_LPUSHX_KEY", 4));
  ASSERT_TRUE(elements_match(&db, "GP1_LPUSHX_KEY", {"x", "o", "o", "o"}));

  // "o" -> "o" -> "x" -> "o" -> "o" -> "o"
  std::vector<std::string> gp1_nodes2 {"o", "o"};
  s = db.LPush("GP1_LPUSHX_KEY", gp1_nodes2, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(num, 6);
  ASSERT_TRUE(len_match(&db, "GP1_LPUSHX_KEY", 6));
  ASSERT_TRUE(elements_match(&db, "GP1_LPUSHX_KEY", {"o", "o", "x", "o", "o", "o"}));

  // "x" -> "o" -> "o" -> "x" -> "o" -> "o" -> "o"
  s = db.LPushx("GP1_LPUSHX_KEY", "x", &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(num, 7);
  ASSERT_TRUE(len_match(&db, "GP1_LPUSHX_KEY", 7));
  ASSERT_TRUE(elements_match(&db, "GP1_LPUSHX_KEY", {"x", "o", "o", "x", "o", "o", "o"}));


  // ***************** Group 2 Test *****************
  // LPushx not exist key
  s = db.LPushx("GP2_LPUSHX_KEY", "x", &num);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_TRUE(len_match(&db, "GP2_LPUSHX_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP2_LPUSHX_KEY", {}));


  // ***************** Group 3 Test *****************
  //  "o" -> "o" -> "o"
  //  LPushx timeout key
  std::vector<std::string> gp3_nodes {"o", "o", "o"};
  s = db.RPush("GP3_LPUSHX_KEY", gp3_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp3_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP3_LPUSHX_KEY", gp3_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP3_LPUSHX_KEY", {"o", "o", "o"}));
  ASSERT_TRUE(make_expired(&db, "GP3_LPUSHX_KEY"));

  s = db.LPushx("GP3_LPUSHX_KEY", "x", &num);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_TRUE(len_match(&db, "GP3_LPUSHX_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP3_LPUSHX_KEY", {}));


  // ***************** Group 4 Test *****************
  // LPushx has been deleted key
  std::vector<std::string> gp4_nodes {"o", "o", "o"};
  s = db.RPush("GP4_LPUSHX_KEY", gp4_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp4_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP4_LPUSHX_KEY", gp4_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP4_LPUSHX_KEY", {"o", "o", "o"}));

  // Delete the key
  std::vector<std::string> del_keys = {"GP4_LPUSHX_KEY"};
  std::map<BlackWidow::DataType, blackwidow::Status> type_status;
  db.Del(del_keys, &type_status);
  ASSERT_TRUE(type_status[BlackWidow::DataType::kLists].ok());

  s = db.LPushx("GP4_LPUSHX_KEY", "x", &num);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_TRUE(len_match(&db, "GP4_LPUSHX_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP4_LPUSHX_KEY", {}));
}

// RPushx
TEST_F(ListsTest, RPushxTest) {
  int64_t ret;
  uint64_t num;

  // ***************** Group 1 Test *****************
  //  "o" -> "o" -> "o"
  std::vector<std::string> gp1_nodes1 {"o", "o", "o"};
  s = db.LPush("GP1_RPUSHX_KEY", gp1_nodes1, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp1_nodes1.size(), num);
  ASSERT_TRUE(len_match(&db, "GP1_RPUSHX_KEY", gp1_nodes1.size()));
  ASSERT_TRUE(elements_match(&db, "GP1_RPUSHX_KEY", {"o", "o", "o"}));

  //  "o" -> "o" -> "o" -> "x"
  s = db.RPushx("GP1_RPUSHX_KEY", "x", &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(num, 4);
  ASSERT_TRUE(len_match(&db, "GP1_RPUSHX_KEY", 4));
  ASSERT_TRUE(elements_match(&db, "GP1_RPUSHX_KEY", {"o", "o", "o", "x"}));

  // "o" -> "o" -> "o" -> "x" -> "o" -> "o"
  std::vector<std::string> gp1_nodes2 {"o", "o"};
  s = db.RPush("GP1_RPUSHX_KEY", gp1_nodes2, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(num, 6);
  ASSERT_TRUE(len_match(&db, "GP1_RPUSHX_KEY", 6));
  ASSERT_TRUE(elements_match(&db, "GP1_RPUSHX_KEY", {"o", "o", "o", "x", "o", "o"}));

  // "o" -> "o" -> "o" -> "x" -> "o" -> "o" -> "x"
  s = db.RPushx("GP1_RPUSHX_KEY", "x", &num);
  ASSERT_EQ(num, 7);
  ASSERT_TRUE(len_match(&db, "GP1_RPUSHX_KEY", 7));
  ASSERT_TRUE(elements_match(&db, "GP1_RPUSHX_KEY", {"o", "o", "o", "x", "o", "o", "x"}));


  // ***************** Group 2 Test *****************
  // RPushx not exist key
  s = db.RPushx("GP2_RPUSHX_KEY", "x", &num);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_TRUE(len_match(&db, "GP2_RPUSHX_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP2_RPUSHX_KEY", {}));


  // ***************** Group 3 Test *****************
  //  "o" -> "o" -> "o"
  //  RPushx timeout key
  std::vector<std::string> gp3_nodes {"o", "o", "o"};
  s = db.RPush("GP3_RPUSHX_KEY", gp3_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp3_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP3_RPUSHX_KEY", gp3_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP3_RPUSHX_KEY", {"o", "o", "o"}));
  ASSERT_TRUE(make_expired(&db, "GP3_RPUSHX_KEY"));

  s = db.RPushx("GP3_RPUSHX_KEY", "x", &num);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_TRUE(len_match(&db, "GP3_RPUSHX_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP3_RPUSHX_KEY", {}));


  // ***************** Group 4 Test *****************
  // RPushx has been deleted key
  std::vector<std::string> gp4_nodes {"o", "o", "o"};
  s = db.RPush("GP4_RPUSHX_KEY", gp4_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp4_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP4_RPUSHX_KEY", gp4_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP4_RPUSHX_KEY", {"o", "o", "o"}));

  // Delete the key
  std::vector<std::string> del_keys = {"GP4_RPUSHX_KEY"};
  std::map<BlackWidow::DataType, blackwidow::Status> type_status;
  db.Del(del_keys, &type_status);
  ASSERT_TRUE(type_status[BlackWidow::DataType::kLists].ok());

  s = db.RPushx("GP4_RPUSHX_KEY", "x", &num);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_TRUE(len_match(&db, "GP4_RPUSHX_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP4_RPUSHX_KEY", {}));
}

// LSet
TEST_F(ListsTest, LSetTest) {
  int64_t ret;
  uint64_t num;

  // ***************** Group 1 Test *****************
  //  "o" -> "o" -> "o" -> "o" -> "o"
  //   0      1      2      3      4
  //  -5     -4     -3     -2     -1
  std::vector<std::string> gp1_nodes1 {"o", "o", "o", "o", "o"};
  s = db.LPush("GP1_LSET_KEY", gp1_nodes1, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp1_nodes1.size(), num);
  ASSERT_TRUE(len_match(&db, "GP1_LSET_KEY", gp1_nodes1.size()));
  ASSERT_TRUE(elements_match(&db, "GP1_LSET_KEY", {"o", "o", "o", "o", "o"}));

  s = db.LSet("GP1_LSET_KEY", 0, "x");
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(&db, "GP1_LSET_KEY", {"x", "o", "o", "o", "o"}));

  s = db.LSet("GP1_LSET_KEY", -3, "x");
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(&db, "GP1_LSET_KEY", {"x", "o", "x", "o", "o"}));

  s = db.LSet("GP1_LSET_KEY", 5, "x");
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_TRUE(elements_match(&db, "GP1_LSET_KEY", {"x", "o", "x", "o", "o"}));

  s = db.LSet("GP1_LSET_KEY", -100, "x");
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_TRUE(elements_match(&db, "GP1_LSET_KEY", {"x", "o", "x", "o", "o"}));

  s = db.LSet("GP1_LSET_KEY", 0, "o");
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(&db, "GP1_LSET_KEY", {"o", "o", "x", "o", "o"}));

  s = db.LSet("GP1_LSET_KEY", -1, "x");
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(&db, "GP1_LSET_KEY", {"o", "o", "x", "o", "x"}));


  //  "o" -> "o" -> "x" -> "o" -> "x" -> "o" -> "o"
  //   0      1      2      3      4      5      6
  //   -7    -6     -5     -4     -3     -2     -1
  std::vector<std::string> gp1_nodes2 {"o", "o"};
  s = db.RPush("GP1_LSET_KEY", gp1_nodes2, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp1_nodes1.size() + gp1_nodes2.size(), num);
  ASSERT_TRUE(len_match(&db, "GP1_LSET_KEY", gp1_nodes1.size() + gp1_nodes2.size()));
  ASSERT_TRUE(elements_match(&db, "GP1_LSET_KEY", {"o", "o", "x", "o", "x", "o", "o"}));

  s = db.LSet("GP1_LSET_KEY", -2, "x");
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(&db, "GP1_LSET_KEY", {"o", "o", "x", "o", "x", "x", "o"}));

  s = db.LSet("GP1_LSET_KEY", -7, "x");
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(&db, "GP1_LSET_KEY", {"x", "o", "x", "o", "x", "x", "o"}));


  // ***************** Group 2 Test *****************
  // LSet expire key
  std::vector<std::string> gp2_nodes {"o", "o", "o"};
  s = db.LPush("GP2_LSET_KEY", gp2_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp2_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP2_LSET_KEY", gp2_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP2_LSET_KEY", {"o", "o", "o"}));
  ASSERT_TRUE(make_expired(&db, "GP2_LSET_KEY"));

  s = db.LSet("GP2_LSET_KEY", 0, "x");
  ASSERT_TRUE(s.IsNotFound());


  // ***************** Group 3 Test *****************
  // LSet not exist key
  s = db.LSet("GP3_LSET_KEY", 0, "x");
  ASSERT_TRUE(s.IsNotFound());


  // ***************** Group 4 Test *****************
  std::vector<std::string> gp4_nodes {"o"};
  s = db.LPush("GP4_LSET_KEY", gp4_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp4_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP4_LSET_KEY", gp4_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP4_LSET_KEY", {"o"}));

  s = db.LSet("GP4_LSET_KEY", 0, "x");
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(&db, "GP4_LSET_KEY", {"x"}));

  s = db.LSet("GP4_LSET_KEY", -1, "o");
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(elements_match(&db, "GP4_LSET_KEY", {"o"}));

  s = db.LSet("GP4_LSET_KEY", -2, "x");
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_TRUE(elements_match(&db, "GP4_LSET_KEY", {"o"}));
}

// LRem
TEST_F(ListsTest, LRemTest) {
  int64_t ret;
  uint64_t num;

  // ***************** Group 1 Test *****************
  //  "o"
  //   0
  //  -1
  std::vector<std::string> gp1_nodes {"o"};
  s = db.RPush("GP1_LREM_KEY", gp1_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp1_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP1_LREM_KEY", gp1_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP1_LREM_KEY", {"o"}));

  s = db.LRem("GP1_LREM_KEY", 0, "x", &num);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_EQ(num, 0);
  ASSERT_TRUE(len_match(&db, "GP1_LREM_KEY", 1));
  ASSERT_TRUE(elements_match(&db, "GP1_LREM_KEY", {"o"}));

  s = db.LRem("GP1_LREM_KEY", 1, "x", &num);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_EQ(num, 0);
  ASSERT_TRUE(len_match(&db, "GP1_LREM_KEY", 1));
  ASSERT_TRUE(elements_match(&db, "GP1_LREM_KEY", {"o"}));

  s = db.LRem("GP1_LREM_KEY", -1, "x", &num);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_EQ(num, 0);
  ASSERT_TRUE(len_match(&db, "GP1_LREM_KEY", 1));
  ASSERT_TRUE(elements_match(&db, "GP1_LREM_KEY", {"o"}));

  s = db.LRem("GP1_LREM_KEY", 1, "o", &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(num, 1);
  ASSERT_TRUE(len_match(&db, "GP1_LREM_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP1_LREM_KEY", {}));


  // ***************** Group 2 Test *****************
  //  "o"
  //   0
  //  -1
  std::vector<std::string> gp2_nodes {"o"};
  s = db.RPush("GP2_LREM_KEY", gp2_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp2_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP2_LREM_KEY", gp2_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP2_LREM_KEY", {"o"}));

  s = db.LRem("GP2_LREM_KEY", -1, "o", &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(num, 1);
  ASSERT_TRUE(len_match(&db, "GP2_LREM_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP2_LREM_KEY", {}));


  // ***************** Group 3 Test *****************
  //  "o"
  //   0
  //  -1
  std::vector<std::string> gp3_nodes {"o"};
  s = db.RPush("GP3_LREM_KEY", gp3_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp3_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP3_LREM_KEY", gp3_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP3_LREM_KEY", {"o"}));

  s = db.LRem("GP3_LREM_KEY", 0, "o", &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(num, 1);
  ASSERT_TRUE(len_match(&db, "GP3_LREM_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP3_LREM_KEY", {}));


  // ***************** Group 4 Test *****************
  //  "o" -> "x"
  //   0      1
  //  -2     -1
  std::vector<std::string> gp4_nodes {"o", "x"};
  s = db.RPush("GP4_LREM_KEY", gp4_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp4_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP4_LREM_KEY", gp4_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP4_LREM_KEY", {"o", "x"}));

  s = db.LRem("GP4_LREM_KEY", 0, "x", &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(num, 1);
  ASSERT_TRUE(len_match(&db, "GP4_LREM_KEY", 1));
  ASSERT_TRUE(elements_match(&db, "GP4_LREM_KEY", {"o"}));


  // ***************** Group 5 Test *****************
  //  "o" -> "x"
  //   0      1
  //  -2     -1
  std::vector<std::string> gp5_nodes {"o", "x"};
  s = db.RPush("GP5_LREM_KEY", gp5_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp5_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP5_LREM_KEY", gp5_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP5_LREM_KEY", {"o", "x"}));

  s = db.LRem("GP5_LREM_KEY", 1, "x", &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(num, 1);
  ASSERT_TRUE(len_match(&db, "GP5_LREM_KEY", 1));
  ASSERT_TRUE(elements_match(&db, "GP5_LREM_KEY", {"o"}));


  // ***************** Group 6 Test *****************
  //  "o" -> "x"
  //   0      1
  //  -2     -1
  std::vector<std::string> gp6_nodes {"o", "x"};
  s = db.RPush("GP6_LREM_KEY", gp6_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp6_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP6_LREM_KEY", gp6_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP6_LREM_KEY", {"o", "x"}));

  s = db.LRem("GP6_LREM_KEY", 0, "x", &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(num, 1);
  ASSERT_TRUE(len_match(&db, "GP6_LREM_KEY", 1));
  ASSERT_TRUE(elements_match(&db, "GP6_LREM_KEY", {"o"}));


  // ***************** Group 7 Test *****************
  //  "o" -> "x" -> "o" -> "o" -> "x" -> "o" -> "x" -> "o" -> "o" -> "x"
  //   0      1      2      3      4      5      6      7      8      9
  //  -1     -2     -3     -4     -5     -6     -7     -8     -9     -10
  std::vector<std::string> gp7_nodes {"o", "x", "o", "o", "x", "o", "x", "o", "o", "x"};
  s = db.RPush("GP7_LREM_KEY", gp7_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp7_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP7_LREM_KEY", gp7_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP7_LREM_KEY", {"o", "x", "o", "o", "x", "o", "x", "o", "o", "x"}));

  s = db.LRem("GP7_LREM_KEY", 0, "x", &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(num, 4);
  ASSERT_TRUE(len_match(&db, "GP7_LREM_KEY", 6));
  ASSERT_TRUE(elements_match(&db, "GP7_LREM_KEY", {"o", "o", "o", "o", "o", "o"}));


  // ***************** Group 8 Test *****************
  //  "o" -> "x" -> "o" -> "o" -> "x" -> "o" -> "x" -> "o" -> "o" -> "x"
  //   0      1      2      3      4      5      6      7      8      9
  //  -1     -2     -3     -4     -5     -6     -7     -8     -9     -10
  std::vector<std::string> gp8_nodes {"o", "x", "o", "o", "x", "o", "x", "o", "o", "x"};
  s = db.RPush("GP8_LREM_KEY", gp8_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp8_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP8_LREM_KEY", gp8_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP8_LREM_KEY", {"o", "x", "o", "o", "x", "o", "x", "o", "o", "x"}));

  s = db.LRem("GP8_LREM_KEY", -10, "x", &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(num, 4);
  ASSERT_TRUE(len_match(&db, "GP8_LREM_KEY", 6));
  ASSERT_TRUE(elements_match(&db, "GP8_LREM_KEY", {"o", "o", "o", "o", "o", "o"}));


  // ***************** Group 9 Test *****************
  //  "o" -> "x" -> "o" -> "o" -> "x" -> "o" -> "x" -> "o" -> "o" -> "x"
  //   0      1      2      3      4      5      6      7      8      9
  //  -1     -2     -3     -4     -5     -6     -7     -8     -9     -10
  std::vector<std::string> gp9_nodes {"o", "x", "o", "o", "x", "o", "x", "o", "o", "x"};
  s = db.RPush("GP9_LREM_KEY", gp9_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp9_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP9_LREM_KEY", gp9_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP9_LREM_KEY", {"o", "x", "o", "o", "x", "o", "x", "o", "o", "x"}));

  s = db.LRem("GP9_LREM_KEY", 10, "x", &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(num, 4);
  ASSERT_TRUE(len_match(&db, "GP9_LREM_KEY", 6));
  ASSERT_TRUE(elements_match(&db, "GP9_LREM_KEY", {"o", "o", "o", "o", "o", "o"}));


  // ***************** Group 10 Test *****************
  //  "o" -> "x" -> "o" -> "o" -> "x" -> "o" -> "x" -> "o" -> "o" -> "x"
  //   0      1      2      3      4      5      6      7      8      9
  //  -1     -2     -3     -4     -5     -6     -7     -8     -9     -10
  std::vector<std::string> gp10_nodes {"o", "x", "o", "o", "x", "o", "x", "o", "o", "x"};
  s = db.RPush("GP10_LREM_KEY", gp10_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp10_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP10_LREM_KEY", gp10_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP10_LREM_KEY", {"o", "x", "o", "o", "x", "o", "x", "o", "o", "x"}));

  s = db.LRem("GP10_LREM_KEY", 1, "x", &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(num, 1);
  ASSERT_TRUE(len_match(&db, "GP10_LREM_KEY", 9));
  ASSERT_TRUE(elements_match(&db, "GP10_LREM_KEY", {"o", "o", "o", "x", "o", "x", "o", "o", "x"}));


  // ***************** Group 11 Test *****************
  //  "o" -> "x" -> "o" -> "o" -> "x" -> "o" -> "x" -> "o" -> "o" -> "x"
  //   0      1      2      3      4      5      6      7      8      9
  //  -1     -2     -3     -4     -5     -6     -7     -8     -9     -10
  std::vector<std::string> gp11_nodes {"o", "x", "o", "o", "x", "o", "x", "o", "o", "x"};
  s = db.RPush("GP11_LREM_KEY", gp11_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp11_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP11_LREM_KEY", gp11_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP11_LREM_KEY", {"o", "x", "o", "o", "x", "o", "x", "o", "o", "x"}));

  s = db.LRem("GP11_LREM_KEY", 3, "x", &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(num, 3);
  ASSERT_TRUE(len_match(&db, "GP11_LREM_KEY", 7));
  ASSERT_TRUE(elements_match(&db, "GP11_LREM_KEY", {"o", "o", "o", "o", "o", "o", "x"}));


  // ***************** Group 12 Test *****************
  //  "o" -> "x" -> "o" -> "o" -> "x" -> "o" -> "x" -> "o" -> "o" -> "x"
  //   0      1      2      3      4      5      6      7      8      9
  //  -1     -2     -3     -4     -5     -6     -7     -8     -9     -10
  std::vector<std::string> gp12_nodes {"o", "x", "o", "o", "x", "o", "x", "o", "o", "x"};
  s = db.RPush("GP12_LREM_KEY", gp12_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp12_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP12_LREM_KEY", gp12_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP12_LREM_KEY", {"o", "x", "o", "o", "x", "o", "x", "o", "o", "x"}));

  s = db.LRem("GP12_LREM_KEY", 4, "x", &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(num, 4);
  ASSERT_TRUE(len_match(&db, "GP12_LREM_KEY", 6));
  ASSERT_TRUE(elements_match(&db, "GP12_LREM_KEY", {"o", "o", "o", "o", "o", "o"}));


  // ***************** Group 13 Test *****************
  //  "o" -> "x" -> "o" -> "o" -> "x" -> "o" -> "x" -> "o" -> "o" -> "x"
  //   0      1      2      3      4      5      6      7      8      9
  //  -1     -2     -3     -4     -5     -6     -7     -8     -9     -10
  std::vector<std::string> gp13_nodes {"o", "x", "o", "o", "x", "o", "x", "o", "o", "x"};
  s = db.RPush("GP13_LREM_KEY", gp13_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp13_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP13_LREM_KEY", gp13_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP13_LREM_KEY", {"o", "x", "o", "o", "x", "o", "x", "o", "o", "x"}));

  s = db.LRem("GP13_LREM_KEY", -1, "x", &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(num, 1);
  ASSERT_TRUE(len_match(&db, "GP13_LREM_KEY", 9));
  ASSERT_TRUE(elements_match(&db, "GP13_LREM_KEY", {"o", "x", "o", "o", "x", "o", "x", "o", "o"}));


  // ***************** Group 14 Test *****************
  //  "o" -> "x" -> "o" -> "o" -> "x" -> "o" -> "x" -> "o" -> "o" -> "x"
  //   0      1      2      3      4      5      6      7      8      9
  //  -1     -2     -3     -4     -5     -6     -7     -8     -9     -10
  std::vector<std::string> gp14_nodes {"o", "x", "o", "o", "x", "o", "x", "o", "o", "x"};
  s = db.RPush("GP14_LREM_KEY", gp14_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp14_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP14_LREM_KEY", gp14_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP14_LREM_KEY", {"o", "x", "o", "o", "x", "o", "x", "o", "o", "x"}));

  s = db.LRem("GP14_LREM_KEY", -2, "x", &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(num, 2);
  ASSERT_TRUE(len_match(&db, "GP14_LREM_KEY", 8));
  ASSERT_TRUE(elements_match(&db, "GP14_LREM_KEY", {"o", "x", "o", "o", "x", "o", "o", "o"}));


  // ***************** Group 15 Test *****************
  //  "o" -> "x" -> "o" -> "o" -> "x" -> "o" -> "x" -> "o" -> "o" -> "x"
  //   0      1      2      3      4      5      6      7      8      9
  //  -1     -2     -3     -4     -5     -6     -7     -8     -9     -10
  std::vector<std::string> gp15_nodes {"o", "x", "o", "o", "x", "o", "x", "o", "o", "x"};
  s = db.RPush("GP15_LREM_KEY", gp15_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp15_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP15_LREM_KEY", gp14_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP15_LREM_KEY", {"o", "x", "o", "o", "x", "o", "x", "o", "o", "x"}));

  s = db.LRem("GP15_LREM_KEY", -3, "x", &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(num, 3);
  ASSERT_TRUE(len_match(&db, "GP15_LREM_KEY", 7));
  ASSERT_TRUE(elements_match(&db, "GP15_LREM_KEY", {"o", "x", "o", "o", "o", "o", "o"}));


  // ***************** Group 16 Test *****************
  //  "o" -> "x" -> "x" -> "x" -> "x" -> "o"
  //   0      1      2      3      4      5
  //  -6     -5     -4     -3     -2     -1
  std::vector<std::string> gp16_nodes {"o", "x", "x", "x", "x", "o"};
  s = db.RPush("GP16_LREM_KEY", gp16_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp16_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP16_LREM_KEY", gp16_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP16_LREM_KEY", {"o", "x", "x", "x", "x", "o"}));

  s = db.LRem("GP16_LREM_KEY", -2, "x", &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(num, 2);
  ASSERT_TRUE(len_match(&db, "GP16_LREM_KEY", 4));
  ASSERT_TRUE(elements_match(&db, "GP16_LREM_KEY", {"o", "x", "x", "o"}));


  // ***************** Group 17 Test *****************
  //  "o" -> "x" -> "x" -> "x" -> "x" -> "o"
  //   0      1      2      3      4      5
  //  -6     -5     -4     -3     -2     -1
  std::vector<std::string> gp17_nodes {"o", "x", "x", "x", "x", "o"};
  s = db.RPush("GP17_LREM_KEY", gp17_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp17_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP17_LREM_KEY", gp17_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP17_LREM_KEY", {"o", "x", "x", "x", "x", "o"}));

  s = db.LRem("GP17_LREM_KEY", 2, "x", &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(num, 2);
  ASSERT_TRUE(len_match(&db, "GP17_LREM_KEY", 4));
  ASSERT_TRUE(elements_match(&db, "GP17_LREM_KEY", {"o", "x", "x", "o"}));


  // ***************** Group 18 Test *****************
  //  "o" -> "x" -> "x" -> "x" -> "x" -> "o"
  //   0      1      2      3      4      5
  //  -6     -5     -4     -3     -2     -1
  std::vector<std::string> gp18_nodes {"o", "x", "x", "x", "x", "o"};
  s = db.RPush("GP18_LREM_KEY", gp18_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp18_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP18_LREM_KEY", gp18_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP18_LREM_KEY", {"o", "x", "x", "x", "x", "o"}));

  s = db.LRem("GP18_LREM_KEY", 3, "x", &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(num, 3);
  ASSERT_TRUE(len_match(&db, "GP18_LREM_KEY", 3));
  ASSERT_TRUE(elements_match(&db, "GP18_LREM_KEY", {"o", "x", "o"}));


  // ***************** Group 19 Test *****************
  //  "o" -> "x" -> "x" -> "x" -> "x" -> "o"
  //   0      1      2      3      4      5
  //  -6     -5     -4     -3     -2     -1
  std::vector<std::string> gp19_nodes {"o", "x", "x", "x", "x", "o"};
  s = db.RPush("GP19_LREM_KEY", gp19_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp19_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP19_LREM_KEY", gp19_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP19_LREM_KEY", {"o", "x", "x", "x", "x", "o"}));

  s = db.LRem("GP19_LREM_KEY", 0, "x", &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(num, 4);
  ASSERT_TRUE(len_match(&db, "GP19_LREM_KEY", 2));
  ASSERT_TRUE(elements_match(&db, "GP19_LREM_KEY", {"o", "o"}));


  // ***************** Group 20 Test *****************
  //  "o" -> "x" -> "o"
  //  LRem timeout key
  std::vector<std::string> gp20_nodes {"o", "o", "o"};
  s = db.RPush("GP20_LREM_KEY", gp20_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp20_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP20_LREM_KEY", gp20_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP20_LREM_KEY", {"o", "o", "o"}));
  ASSERT_TRUE(make_expired(&db, "GP20_LREM_KEY"));

  s = db.LRem("GP20_LREM_KEY", 0, "x", &num);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_EQ(0, num);
  ASSERT_TRUE(len_match(&db, "GP20_LREM_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP20_LREM_KEY", {}));


  // ***************** Group 21 Test *****************
  //  LRem not exist key
  s = db.LRem("GP21_LREM_KEY", 0, "x", &num);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_EQ(0, num);
  ASSERT_TRUE(len_match(&db, "GP21_LREM_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP21_LREM_KEY", {}));
}

// RPoplpush
TEST_F(ListsTest, RPoplpushTest) {
  int64_t ret;
  uint64_t num;
  std::string target;

  // ***************** Group 1 Test *****************
  // source        "o"
  // destination
  // ----------------after rpoplpush-----------------
  // source
  // destination   "o"
  //
  std::vector<std::string> gp1_nodes {"o"};
  s = db.RPush("GP1_RPOPLPUSH_SOURCE_KEY", gp1_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp1_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP1_RPOPLPUSH_SOURCE_KEY", gp1_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP1_RPOPLPUSH_SOURCE_KEY", {"o"}));

  s = db.RPoplpush("GP1_RPOPLPUSH_SOURCE_KEY", "GP1_RPOPLPUSH_DESTINATION_KEY", &target);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(target, "o");
  ASSERT_TRUE(len_match(&db, "GP1_RPOPLPUSH_SOURCE_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP1_RPOPLPUSH_SOURCE_KEY", {}));
  ASSERT_TRUE(len_match(&db, "GP1_RPOPLPUSH_DESTINATION_KEY", 1));
  ASSERT_TRUE(elements_match(&db, "GP1_RPOPLPUSH_DESTINATION_KEY", {"o"}));


  // ***************** Group 2 Test *****************
  // source
  // destination   "o"
  // ----------------after rpoplpush-----------------
  // source
  // destination   "o"
  //
  std::vector<std::string> gp2_nodes {"o"};
  s = db.RPush("GP2_RPOPLPUSH_DESTINATION_KEY", gp2_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp2_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP2_RPOPLPUSH_DESTINATION_KEY", gp2_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP2_RPOPLPUSH_DESTINATION_KEY", {"o"}));

  s = db.RPoplpush("GP2_RPOPLPUSH_SOURCE_KEY", "GP2_RPOPLPUSH_DESTINATION_KEY", &target);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_EQ(target, "");
  ASSERT_TRUE(len_match(&db, "GP2_RPOPLPUSH_SOURCE_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP2_RPOPLPUSH_SOURCE_KEY", {}));
  ASSERT_TRUE(len_match(&db, "GP2_RPOPLPUSH_DESTINATION_KEY", 1));
  ASSERT_TRUE(elements_match(&db, "GP2_RPOPLPUSH_DESTINATION_KEY", {"o"}));


  // ***************** Group 3 Test *****************
  // source        "a" -> "b" -> "c" -> "o"
  // destination   "a" -> "b" -> "c"
  // ----------------after rpoplpush-----------------
  // source        "a" -> "b" -> "c"
  // destination   "o" -> "a" -> "b" -> "c"
  // ----------------after rpoplpush-----------------
  // source        "a" -> "b"
  // destination   "c" -> "o" -> "a" -> "b" -> "c"
  // ----------------after rpoplpush-----------------
  // source        "a"
  // destination   "b" -> "c" -> "o" -> "a" -> "b" -> "c"
  // ----------------after rpoplpush-----------------
  // source
  // destination   "a" -> "b" -> "c" -> "o" -> "a" -> "b" -> "c"
  //
  std::vector<std::string> gp3_nodes1 {"a", "b", "c", "o"};
  std::vector<std::string> gp3_nodes2 {"a", "b", "c"};
  s = db.RPush("GP3_RPOPLPUSH_SOURCE_KEY", gp3_nodes1, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp3_nodes1.size(), num);
  ASSERT_TRUE(len_match(&db, "GP3_RPOPLPUSH_SOURCE_KEY", gp3_nodes1.size()));
  ASSERT_TRUE(elements_match(&db, "GP3_RPOPLPUSH_SOURCE_KEY", {"a", "b","c", "o"}));

  s = db.RPush("GP3_RPOPLPUSH_DESTINATION_KEY", gp3_nodes2, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp3_nodes2.size(), num);
  ASSERT_TRUE(len_match(&db, "GP3_RPOPLPUSH_DESTINATION_KEY", gp3_nodes2.size()));
  ASSERT_TRUE(elements_match(&db, "GP3_RPOPLPUSH_DESTINATION_KEY", {"a", "b", "c"}));

  s = db.RPoplpush("GP3_RPOPLPUSH_SOURCE_KEY", "GP3_RPOPLPUSH_DESTINATION_KEY", &target);
  ASSERT_EQ(target, "o");
  ASSERT_TRUE(len_match(&db, "GP3_RPOPLPUSH_SOURCE_KEY", 3));
  ASSERT_TRUE(elements_match(&db, "GP3_RPOPLPUSH_SOURCE_KEY", {"a", "b", "c"}));
  ASSERT_TRUE(len_match(&db, "GP3_RPOPLPUSH_DESTINATION_KEY", 4));
  ASSERT_TRUE(elements_match(&db, "GP3_RPOPLPUSH_DESTINATION_KEY", {"o", "a", "b", "c"}));

  s = db.RPoplpush("GP3_RPOPLPUSH_SOURCE_KEY", "GP3_RPOPLPUSH_DESTINATION_KEY", &target);
  ASSERT_EQ(target, "c");
  ASSERT_TRUE(len_match(&db, "GP3_RPOPLPUSH_SOURCE_KEY", 2));
  ASSERT_TRUE(elements_match(&db, "GP3_RPOPLPUSH_SOURCE_KEY", {"a", "b"}));
  ASSERT_TRUE(len_match(&db, "GP3_RPOPLPUSH_DESTINATION_KEY", 5));
  ASSERT_TRUE(elements_match(&db, "GP3_RPOPLPUSH_DESTINATION_KEY", {"c", "o", "a", "b", "c"}));

  s = db.RPoplpush("GP3_RPOPLPUSH_SOURCE_KEY", "GP3_RPOPLPUSH_DESTINATION_KEY", &target);
  ASSERT_EQ(target, "b");
  ASSERT_TRUE(len_match(&db, "GP3_RPOPLPUSH_SOURCE_KEY", 1));
  ASSERT_TRUE(elements_match(&db, "GP3_RPOPLPUSH_SOURCE_KEY", {"a"}));
  ASSERT_TRUE(len_match(&db, "GP3_RPOPLPUSH_DESTINATION_KEY", 6));
  ASSERT_TRUE(elements_match(&db, "GP3_RPOPLPUSH_DESTINATION_KEY", {"b", "c", "o", "a", "b", "c"}));

  s = db.RPoplpush("GP3_RPOPLPUSH_SOURCE_KEY", "GP3_RPOPLPUSH_DESTINATION_KEY", &target);
  ASSERT_EQ(target, "a");
  ASSERT_TRUE(len_match(&db, "GP3_RPOPLPUSH_SOURCE_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP3_RPOPLPUSH_SOURCE_KEY", {}));
  ASSERT_TRUE(len_match(&db, "GP3_RPOPLPUSH_DESTINATION_KEY", 7));
  ASSERT_TRUE(elements_match(&db, "GP3_RPOPLPUSH_DESTINATION_KEY", {"a", "b", "c", "o", "a", "b", "c"}));

  s = db.RPoplpush("GP3_RPOPLPUSH_SOURCE_KEY", "GP3_RPOPLPUSH_DESTINATION_KEY", &target);
  ASSERT_EQ(target, "");
  ASSERT_TRUE(len_match(&db, "GP3_RPOPLPUSH_SOURCE_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP3_RPOPLPUSH_SOURCE_KEY", {}));
  ASSERT_TRUE(len_match(&db, "GP3_RPOPLPUSH_DESTINATION_KEY", 7));
  ASSERT_TRUE(elements_match(&db, "GP3_RPOPLPUSH_DESTINATION_KEY", {"a", "b", "c", "o", "a", "b", "c"}));


  // ***************** Group 4 Test *****************
  // source                            (empty list);
  // destination   "a" -> "b" -> "c"
  // ----------------after rpoplpush-----------------
  // source                            (empty list);
  // destination   "a" -> "b" -> "c"
  //
  std::vector<std::string> gp4_nodes1 {"o"};
  std::vector<std::string> gp4_nodes2 {"a", "b", "c"};
  s = db.RPush("GP4_RPOPLPUSH_SOURCE_KEY", gp4_nodes1, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp4_nodes1.size(), num);
  ASSERT_TRUE(len_match(&db, "GP4_RPOPLPUSH_SOURCE_KEY", gp4_nodes1.size()));
  ASSERT_TRUE(elements_match(&db, "GP4_RPOPLPUSH_SOURCE_KEY", {"o"}));
  s = db.RPop("GP4_RPOPLPUSH_SOURCE_KEY", &target);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(target, "o");
  ASSERT_TRUE(len_match(&db, "GP4_RPOPLPUSH_SOURCE_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP4_RPOPLPUSH_SOURCE_KEY", {}));

  s = db.RPush("GP4_RPOPLPUSH_DESTINATION_KEY", gp4_nodes2, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp4_nodes2.size(), num);
  ASSERT_TRUE(len_match(&db, "GP4_RPOPLPUSH_DESTINATION_KEY", gp4_nodes2.size()));
  ASSERT_TRUE(elements_match(&db, "GP4_RPOPLPUSH_DESTINATION_KEY", {"a", "b", "c"}));

  s = db.RPoplpush("GP4_RPOPLPUSH_SOURCE_KEY", "GP4_RPOPLPUSH_DESTINATION_KEY", &target);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_EQ(target, "");
  ASSERT_TRUE(len_match(&db, "GP4_RPOPLPUSH_SOURCE_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP4_RPOPLPUSH_SOURCE_KEY", {}));
  ASSERT_TRUE(len_match(&db, "GP4_RPOPLPUSH_DESTINATION_KEY", 3));
  ASSERT_TRUE(elements_match(&db, "GP4_RPOPLPUSH_DESTINATION_KEY", {"a", "b", "c"}));


  // ***************** Group 5 Test *****************
  // source        "a" -> "b" -> "c"
  // destination                       (empty list);
  // ----------------after rpoplpush-----------------
  // source        "a" -> "b"
  // destination   "c"
  //
  std::vector<std::string> gp5_nodes1 {"a", "b", "c"};
  std::vector<std::string> gp5_nodes2 {"o"};
  s = db.RPush("GP5_RPOPLPUSH_SOURCE_KEY", gp5_nodes1, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp5_nodes1.size(), num);
  ASSERT_TRUE(len_match(&db, "GP5_RPOPLPUSH_SOURCE_KEY", gp5_nodes1.size()));
  ASSERT_TRUE(elements_match(&db, "GP5_RPOPLPUSH_SOURCE_KEY", {"a", "b", "c"}));

  s = db.RPush("GP5_RPOPLPUSH_DESTINATION_KEY", gp5_nodes2, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp5_nodes2.size(), num);
  ASSERT_TRUE(len_match(&db, "GP5_RPOPLPUSH_DESTINATION_KEY", gp5_nodes2.size()));
  ASSERT_TRUE(elements_match(&db, "GP5_RPOPLPUSH_DESTINATION_KEY", {"o"}));
  s = db.RPop("GP5_RPOPLPUSH_DESTINATION_KEY", &target);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(target, "o");
  ASSERT_TRUE(len_match(&db, "GP5_RPOPLPUSH_DESTINATION_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP5_RPOPLPUSH_DESTINATION_KEY", {}));

  s = db.RPoplpush("GP5_RPOPLPUSH_SOURCE_KEY", "GP5_RPOPLPUSH_DESTINATION_KEY", &target);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(target, "c");
  ASSERT_TRUE(len_match(&db, "GP5_RPOPLPUSH_SOURCE_KEY", 2));
  ASSERT_TRUE(elements_match(&db, "GP5_RPOPLPUSH_SOURCE_KEY", {"a", "b"}));
  ASSERT_TRUE(len_match(&db, "GP5_RPOPLPUSH_DESTINATION_KEY", 1));
  ASSERT_TRUE(elements_match(&db, "GP5_RPOPLPUSH_DESTINATION_KEY", {"c"}));


  // ***************** Group 6 Test *****************
  // source        "a" -> "b" -> "c"   (timeout list);
  // destination   "x" -> "y" -> "z"
  // ----------------after rpoplpush-----------------
  // source        "a" -> "b" -> "c"   (timeout list);
  // destination   "x" -> "y" -> "z"
  //
  std::vector<std::string> gp6_nodes1 {"a", "b", "c"};
  std::vector<std::string> gp6_nodes2 {"x", "y", "z"};
  s = db.RPush("GP6_RPOPLPUSH_SOURCE_KEY", gp6_nodes1, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp6_nodes1.size(), num);
  ASSERT_TRUE(len_match(&db, "GP6_RPOPLPUSH_SOURCE_KEY", gp6_nodes1.size()));
  ASSERT_TRUE(elements_match(&db, "GP6_RPOPLPUSH_SOURCE_KEY", {"a", "b", "c"}));
  ASSERT_TRUE(make_expired(&db, "GP6_RPOPLPUSH_SOURCE_KEY"));

  s = db.RPush("GP6_RPOPLPUSH_DESTINATION_KEY", gp6_nodes2, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp6_nodes2.size(), num);
  ASSERT_TRUE(len_match(&db, "GP6_RPOPLPUSH_DESTINATION_KEY", gp6_nodes2.size()));
  ASSERT_TRUE(elements_match(&db, "GP6_RPOPLPUSH_DESTINATION_KEY", {"x", "y", "z"}));

  s = db.RPoplpush("GP6_RPOPLPUSH_SOURCE_KEY", "GP6_RPOPLPUSH_DESTINATION_KEY", &target);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_EQ(target, "");
  ASSERT_TRUE(len_match(&db, "GP6_RPOPLPUSH_SOURCE_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP6_RPOPLPUSH_SOURCE_KEY", {}));
  ASSERT_TRUE(len_match(&db, "GP6_RPOPLPUSH_DESTINATION_KEY", 3));
  ASSERT_TRUE(elements_match(&db, "GP6_RPOPLPUSH_DESTINATION_KEY", {"x", "y", "z"}));


  // ***************** Group 7 Test *****************
  // source        "a" -> "b" -> "c"
  // destination   "x" -> "y" -> "z"   (timeout list);
  // ----------------after rpoplpush-----------------
  // source        "a" -> "b"
  // destination   "c"
  //
  std::vector<std::string> gp7_nodes1 {"a", "b", "c"};
  std::vector<std::string> gp7_nodes2 {"x", "y", "z"};
  s = db.RPush("GP7_RPOPLPUSH_SOURCE_KEY", gp7_nodes1, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp7_nodes1.size(), num);
  ASSERT_TRUE(len_match(&db, "GP7_RPOPLPUSH_SOURCE_KEY", gp7_nodes1.size()));
  ASSERT_TRUE(elements_match(&db, "GP7_RPOPLPUSH_SOURCE_KEY", {"a", "b", "c"}));

  s = db.RPush("GP7_RPOPLPUSH_DESTINATION_KEY", gp7_nodes2, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp7_nodes2.size(), num);
  ASSERT_TRUE(len_match(&db, "GP7_RPOPLPUSH_DESTINATION_KEY", gp7_nodes2.size()));
  ASSERT_TRUE(elements_match(&db, "GP7_RPOPLPUSH_DESTINATION_KEY", {"x", "y", "z"}));
  ASSERT_TRUE(make_expired(&db, "GP7_RPOPLPUSH_DESTINATION_KEY"));

  s = db.RPoplpush("GP7_RPOPLPUSH_SOURCE_KEY", "GP7_RPOPLPUSH_DESTINATION_KEY", &target);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(target, "c");
  ASSERT_TRUE(len_match(&db, "GP7_RPOPLPUSH_SOURCE_KEY", 2));
  ASSERT_TRUE(elements_match(&db, "GP7_RPOPLPUSH_SOURCE_KEY", {"a", "b"}));
  ASSERT_TRUE(len_match(&db, "GP7_RPOPLPUSH_DESTINATION_KEY", 1));
  ASSERT_TRUE(elements_match(&db, "GP7_RPOPLPUSH_DESTINATION_KEY", {"c"}));


  // ***************** Group 8 Test *****************
  // source        "a" -> "b" -> "c"
  // ----------------after rpoplpush-----------------
  // source        "c" -> "a" -> "b"
  //
  std::vector<std::string> gp8_nodes {"a", "b", "c"};
  s = db.RPush("GP8_RPOPLPUSH_SOURCE_KEY", gp8_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp8_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP8_RPOPLPUSH_SOURCE_KEY", gp8_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP8_RPOPLPUSH_SOURCE_KEY", {"a", "b", "c"}));

  s = db.RPoplpush("GP8_RPOPLPUSH_SOURCE_KEY", "GP8_RPOPLPUSH_SOURCE_KEY", &target);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(target, "c");
  ASSERT_TRUE(len_match(&db, "GP8_RPOPLPUSH_SOURCE_KEY", 3));
  ASSERT_TRUE(elements_match(&db, "GP8_RPOPLPUSH_SOURCE_KEY", {"c", "a", "b"}));


  // ***************** Group 9 Test *****************
  // source        "a" -> "b" -> "c"    (timeout list)
  // ----------------after rpoplpush-----------------
  // source        "a" -> "b" -> "c"    (timeout list)
  //
  std::vector<std::string> gp9_nodes {"a", "b", "c"};
  s = db.RPush("GP9_RPOPLPUSH_SOURCE_KEY", gp9_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp9_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP9_RPOPLPUSH_SOURCE_KEY", gp9_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP9_RPOPLPUSH_SOURCE_KEY", {"a", "b", "c"}));
  ASSERT_TRUE(make_expired(&db, "GP9_RPOPLPUSH_SOURCE_KEY"));
  ASSERT_TRUE(len_match(&db, "GP9_RPOPLPUSH_SOURCE_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP9_RPOPLPUSH_SOURCE_KEY", {}));

  s = db.RPoplpush("GP9_RPOPLPUSH_SOURCE_KEY", "GP9_RPOPLPUSH_SOURCE_KEY", &target);
  ASSERT_TRUE(s.IsNotFound());
  ASSERT_EQ(target, "");
  ASSERT_TRUE(len_match(&db, "GP9_RPOPLPUSH_SOURCE_KEY", 0));
  ASSERT_TRUE(elements_match(&db, "GP9_RPOPLPUSH_SOURCE_KEY", {}));


  // ***************** Group 10 Test *****************
  // source        "o"
  // ----------------after rpoplpush-----------------
  // source        "o"
  //
  std::vector<std::string> gp10_nodes {"o"};
  s = db.RPush("GP10_RPOPLPUSH_SOURCE_KEY", gp10_nodes, &num);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(gp10_nodes.size(), num);
  ASSERT_TRUE(len_match(&db, "GP10_RPOPLPUSH_SOURCE_KEY", gp10_nodes.size()));
  ASSERT_TRUE(elements_match(&db, "GP10_RPOPLPUSH_SOURCE_KEY", {"o"}));

  s = db.RPoplpush("GP10_RPOPLPUSH_SOURCE_KEY", "GP10_RPOPLPUSH_SOURCE_KEY", &target);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(target, "o");
  ASSERT_TRUE(len_match(&db, "GP10_RPOPLPUSH_SOURCE_KEY", 1));
  ASSERT_TRUE(elements_match(&db, "GP10_RPOPLPUSH_SOURCE_KEY", {"o"}));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
