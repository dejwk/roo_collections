
#include <assert.h>

#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>

#include "gtest/gtest.h"
#include "roo_collections/flat_small_hash_map.h"

namespace roo_collections {

template <typename K, typename V, typename HashFn>
std::ostream& operator<<(std::ostream& out,
                         const FlatSmallHashMap<K, V, HashFn>& map) {
  bool first = true;
  out << "{";
  for (const auto& e : map) {
    if (first) {
      first = false;
    } else {
      out << ", ";
    }
    out << "(" << e.first << ", " << e.second << ")";
  }
  out << "}";
  return out;
}

TEST(FlatSmallHashMap, DefaultConstructor) {
  FlatSmallHashMap<std::string, int> map;

  EXPECT_EQ(map.size(), 0);
  EXPECT_EQ(map.capacity(), 8);
  EXPECT_TRUE(map.empty());
}

TEST(FlatSmallHashMap, InitialCapacityRespected) {
  {
    FlatSmallHashMap<std::string, int> map(8);
    EXPECT_EQ(map.capacity(), 8);
    EXPECT_TRUE(map.empty());
  }
  {
    FlatSmallHashMap<std::string, int> map(16);
    EXPECT_GE(map.capacity(), 16);
    EXPECT_TRUE(map.empty());
  }
  {
    FlatSmallHashMap<std::string, int> map(32);
    EXPECT_GE(map.capacity(), 32);
    EXPECT_TRUE(map.empty());
  }
  {
    FlatSmallHashMap<std::string, int> map(1400);
    EXPECT_GE(map.capacity(), 1400);
    EXPECT_TRUE(map.empty());
  }
}

TEST(FlatSmallHashMap, At) {
  FlatSmallHashMap<std::string, int> map(
      {{"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}, {"e", 5}});

  EXPECT_EQ(map.at("a"), 1);
  EXPECT_EQ(map.at("b"), 2);
  EXPECT_EQ(map.at("c"), 3);
  EXPECT_EQ(map.at("d"), 4);
  EXPECT_EQ(map.at("e"), 5);

  // Verify that 'at' is assignable.
  map.at("a") = 100;
  EXPECT_EQ(map.at("a"), 100);
  EXPECT_EQ(map.size(), 5);

  // // check non-existing key:
  // try {
  //   map.at("f") = 6;
  //   assert(!" ~~~ function '.at' should not allow search for a non-existent
  //   key ~~~ ");
  // } catch (const std::exception &e) {
  // }

  // try {
  //   map.at("f");
  //   assert(!" ~~~ function '.at' should not allow search for a non-existent
  //   key ~~~ ");
  // } catch (const std::exception &e) {
  // }

  EXPECT_EQ(map.size(), 5);
}

TEST(FlatSmallHashMap, StringView) {
  FlatSmallHashMap<std::string, int, TransparentStringHashFn, std::equal_to<>>
      map({{"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}, {"e", 5}});
  ASSERT_TRUE(map.find(std::string_view("a")) != map.end());
  EXPECT_EQ(1, map["a"]);
  EXPECT_EQ(2, map[std::string_view("b")]);
}

TEST(FlatSmallHashMap, OperatorAssignment) {
  std::vector<std::pair<std::string, int>> entries = {
      {"a", 1}, {"b", 2}, {"c", 3},  {"d", 4},  {"e", 5},  {"f", 6}, {"g", 7},
      {"h", 8}, {"i", 9}, {"j", 10}, {"k", 11}, {"l", 12}, {"m", 13}};

  FlatSmallHashMap<std::string, int> map1(entries.begin(), entries.end());
  FlatSmallHashMap<std::string, int> map2;

  for (const auto& e : entries) {
    EXPECT_EQ(map1[e.first], e.second);
  }

  map1 = map1;

  for (const auto& e : entries) {
    EXPECT_EQ(map1[e.first], e.second);
  }

  map2 = map1;

  EXPECT_EQ(map2.size(), map1.size());
  EXPECT_EQ(map2.capacity(), map1.capacity());

  for (const auto& e : entries) {
    EXPECT_EQ(map2[e.first], e.second);
  }

  map1.at("a") = 100;
  EXPECT_EQ(map1.at("a"), 100);
  EXPECT_EQ(map2.at("a"), 1);

  map2.at("m") = 130;
  EXPECT_EQ(map2.at("m"), 130);
  EXPECT_EQ(map1.at("m"), 13);
}

TEST(FlatSmallHashMap, CopyConstructor) {
  std::vector<std::pair<std::string, int>> entries = {
      {"a", 1}, {"b", 2}, {"c", 3},  {"d", 4},  {"e", 5},  {"f", 6}, {"g", 7},
      {"h", 8}, {"i", 9}, {"j", 10}, {"k", 11}, {"l", 12}, {"m", 13}};

  FlatSmallHashMap<std::string, int> map1(entries.begin(), entries.end());
  FlatSmallHashMap<std::string, int> map2(map1);
  ;

  EXPECT_EQ(map2.size(), map1.size());
  EXPECT_EQ(map2.capacity(), map1.capacity());

  for (const auto& e : entries) {
    EXPECT_EQ(map2[e.first], e.second);
  }

  map1.at("a") = 100;
  EXPECT_EQ(map1.at("a"), 100);
  EXPECT_EQ(map2.at("a"), 1);

  map2.at("m") = 130;
  EXPECT_EQ(map2.at("m"), 130);
  EXPECT_EQ(map1.at("m"), 13);
}

TEST(FlatSmallHashMap, Insert) {
  FlatSmallHashMap<std::string, int> map;

  EXPECT_TRUE(map.empty());

  bool b = map.insert({"a", 10}).second;
  EXPECT_TRUE(b);
  EXPECT_EQ(map.size(), 1);
  EXPECT_FALSE(map.empty());

  b = map.insert({"a", 100}).second;
  EXPECT_FALSE(b);
  EXPECT_EQ(map.size(), 1);
  EXPECT_FALSE(map.empty());

  b = map.insert({"a", 10}).second;
  EXPECT_FALSE(b);
  EXPECT_EQ(map.size(), 1);
  EXPECT_FALSE(map.empty());
}

TEST(FlatSmallHashMap, InsertFromDummyEmpty) {
  FlatSmallHashMap<std::string, int> map(0);

  EXPECT_TRUE(map.empty());
  EXPECT_EQ(map.size(), 0);
  EXPECT_FALSE(map.contains("a"));

  bool b = map.insert({"a", 10}).second;
  EXPECT_TRUE(b);
  EXPECT_EQ(map.size(), 1);
  EXPECT_FALSE(map.empty());

  b = map.insert({"a", 100}).second;
  EXPECT_FALSE(b);
  EXPECT_EQ(map.size(), 1);
  EXPECT_FALSE(map.empty());

  b = map.insert({"a", 10}).second;
  EXPECT_FALSE(b);
  EXPECT_EQ(map.size(), 1);
  EXPECT_FALSE(map.empty());
}

TEST(FlatSmallHashMap, Erase) {
  std::vector<std::pair<int, int>> entries = {{0, 0}, {1, 1}, {2, 2}, {3, 3},
                                              {4, 4}, {5, 5}, {6, 6}, {15, 15}};

  FlatSmallHashMap<int, int> map(entries.begin(), entries.end());

  bool b = map.erase(0);
  EXPECT_TRUE(b);
  EXPECT_EQ(map.size(), 7);

  b = map.erase(4);
  EXPECT_TRUE(b);
  EXPECT_EQ(map.size(), 6);

  b = map.erase(10);
  EXPECT_FALSE(b);
  EXPECT_EQ(map.size(), 6);
}

TEST(FlatSmallHashMap, RepetitiveInsertEraseOneElementDoesNotGrow) {
  FlatSmallHashMap<int, int> map;
  // In this case, we do expect some rehashing, but the storage should remain
  // stationary.
  for (int i = 0; i < 1000; ++i) {
    map.insert({0, i});
    ASSERT_TRUE(map.erase(0));
  }
  EXPECT_TRUE(map.empty());
  EXPECT_EQ(map.capacity(), 8);
}

TEST(FlatSmallHashMap, EraseUsingIterator) {
  FlatSmallHashMap<int, int> map({{0, 0}, {1, 1}, {2, 2}, {3, 3}});
  // Check if erase works for const iterator.
  FlatSmallHashMap<int, int>::const_iterator itr = map.begin();
  ASSERT_NE(itr, map.end());
  EXPECT_EQ(itr->first, 0);
  itr = map.erase(itr);
  ASSERT_NE(itr, map.end());
  EXPECT_EQ(itr->first, 1);
  EXPECT_EQ(map.size(), 3);
  itr = map.erase(itr);
  ASSERT_NE(itr, map.end());
  EXPECT_EQ(itr->first, 2);
  EXPECT_EQ(map.size(), 2);
  itr = map.erase(itr);
  ASSERT_NE(itr, map.end());
  EXPECT_EQ(itr->first, 3);
  EXPECT_EQ(map.size(), 1);
  itr = map.erase(itr);
  ASSERT_EQ(itr, map.end());
  itr = map.erase(itr);
  ASSERT_EQ(itr, map.end());
}

TEST(FlatSmallHashMap, RepetitiveInsertEraseDoesNotGrow) {
  std::vector<std::pair<int, int>> entries = {{0, 0}, {1, 1}, {2, 2}, {3, 3}};
  FlatSmallHashMap<int, int> map;
  for (int i = 0; i < 1000; ++i) {
    map.insert({5, i});
    ASSERT_TRUE(map.erase(5));
  }
  EXPECT_TRUE(map.empty());
  EXPECT_EQ(map.capacity(), 8);
}

TEST(FlatSmallHashMap, Clear) {
  FlatSmallHashMap<int, int> map;

  size_t i = 1;
  for (int n = 0; n <= 8; n++) {
    while (i <= 8 * (1 << n) * 0.75) {
      map.insert({i, i});
      i++;
    }
  }
  i--;

  EXPECT_EQ(map.size(), 1536);
  EXPECT_EQ(map.capacity(), 2986);

  EXPECT_EQ(map.size(), i);
  EXPECT_EQ(map.capacity(), 2986);

  map.clear();

  EXPECT_TRUE(map.empty());
  EXPECT_EQ(map.size(), 0);
  EXPECT_EQ(map.capacity(), 2986);

  map.insert({1, 1});

  EXPECT_EQ(map.size(), 1);
  EXPECT_EQ(map.capacity(), 2986);

  map.compact();
  EXPECT_EQ(map.size(), 1);
  EXPECT_EQ(map.capacity(), 2);

  map.erase(1);

  EXPECT_EQ(map.size(), 0);
  EXPECT_EQ(map.capacity(), 2);
}

TEST(FlatSmallHashMap, OperatorSubscript) {
  std::vector<std::pair<std::string, int>> entries = {
      {"a", 1}, {"b", 2}, {"c", 3}};

  FlatSmallHashMap<std::string, int> map(entries.begin(), entries.end());

  EXPECT_EQ(map.size(), 3);

  int a = map["a"];
  EXPECT_EQ(a, 1);
  a = 10;
  EXPECT_EQ(map["a"], 1);

  int b = map["b"];
  EXPECT_EQ(b, 2);
  b = 20;
  EXPECT_EQ(map["b"], 2);

  int c = map["c"];
  EXPECT_EQ(c, 3);
  c = 30;
  EXPECT_EQ(map["c"], 3);

  EXPECT_EQ(map.size(), 3);

  map["a"] = 111;
  EXPECT_EQ(map["a"], 111);

  map["b"] = 222;
  EXPECT_EQ(map["b"], 222);

  map["c"] = 333;
  EXPECT_EQ(map["c"], 333);

  EXPECT_EQ(map.size(), 3);

  map["d"] = 444;
  EXPECT_EQ(map["d"], 444);

  EXPECT_EQ(map.size(), 4);

  map["e"];

  EXPECT_EQ(map.size(), 5);
}

TEST(FlatSmallHashMap, OperatorSubscriptConst) {
  std::vector<std::pair<std::string, int>> entries = {
      {"a", 1}, {"b", 2}, {"c", 3}};

  const FlatSmallHashMap<std::string, int> constMap(entries.begin(),
                                                    entries.end());

  EXPECT_EQ(constMap.size(), 3);

  int aa = constMap["a"];
  EXPECT_EQ(aa, 1);
  aa = 10;
  EXPECT_EQ(constMap["a"], 1);

  int bb = constMap["b"];
  EXPECT_EQ(bb, 2);

  int cc = constMap["c"];
  EXPECT_EQ(cc, 3);

  EXPECT_EQ(constMap.size(), 3);

  // constMap["d"];  // Causes assertion failure.
}

TEST(FlatSmallHashMap, OperatorEqualsAndNotEquals) {
  FlatSmallHashMap<int, int> emptyMap1;
  FlatSmallHashMap<int, int> emptyMap2;

  std::vector<std::pair<std::string, int>> entries = {
      {"1", 1}, {"2", 2}, {"3", 3}};

  FlatSmallHashMap<std::string, int> mapString1(entries.begin(), entries.end());
  FlatSmallHashMap<std::string, int> mapString2(entries.begin(), entries.end());

  EXPECT_EQ(emptyMap1, emptyMap2);
  EXPECT_FALSE(emptyMap1 != emptyMap2);

  EXPECT_TRUE(mapString1 == mapString2);
  EXPECT_FALSE(mapString1 != mapString2);

  mapString1["4"];

  EXPECT_TRUE(mapString1 != mapString2);
  EXPECT_FALSE(mapString1 == mapString2);

  mapString1["4"] = 4;
  mapString2.insert({"4", 4});

  EXPECT_TRUE(mapString1 == mapString2);
  EXPECT_FALSE(mapString1 != mapString2);

  mapString1.clear();
  mapString2.clear();

  EXPECT_TRUE(mapString1 == mapString2);
  EXPECT_FALSE(mapString1 != mapString2);

  FlatSmallHashMap<int, int> map;

  int i = 1;
  for (int n = 0; n <= 8; n++) {
    while (i <= 8 * (1 << n) * 0.75) {
      map.insert({i, i});
      i++;
    }
  }

  EXPECT_TRUE(map != emptyMap1);
  EXPECT_FALSE(map == emptyMap1);

  map.clear();
  EXPECT_FALSE(map != emptyMap1);
  EXPECT_TRUE(map == emptyMap1);

  FlatSmallHashMap<std::string, int> smallMap(8);
  FlatSmallHashMap<std::string, int> largeMap(2000);
  for (const auto& e : entries) {
    smallMap.insert(e);
    largeMap.insert(e);
  }
  EXPECT_EQ(smallMap, largeMap);
}

TEST(FlatSmallHashMap, ContainsKey) {
  std::vector<std::pair<std::string, int>> entries = {
      {"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}, {"e", 5},
      {"f", 6}, {"g", 7}, {"h", 8}, {"i", 9}};

  FlatSmallHashMap<std::string, int> map(entries.begin(), entries.end());

  for (const auto& e : entries) {
    EXPECT_TRUE(map.contains(e.first));
  }

  EXPECT_TRUE(!map.contains("x"));
}

TEST(FlatSmallHashMap, IteratorsEmpty) {
  FlatSmallHashMap<std::string, int> emptyMap;

  auto iterBegin = emptyMap.begin();
  auto iterEnd = emptyMap.end();

  EXPECT_EQ(iterBegin, iterBegin);
  EXPECT_EQ(iterEnd, iterEnd);
  EXPECT_EQ(iterBegin, iterEnd);
}

TEST(FlatSmallHashMap, Regression1) {
  FlatSmallHashMap<int16_t, int16_t> map;
  map.insert({58, -47});
  map.insert({-40, 40});
  map.insert({81, 124});
  map.insert({-56, -80});
  map.insert({1, -16});
  map.erase(1);

  FlatSmallHashMap<int16_t, int16_t> expected(
      {{58, -47}, {-40, 40}, {81, 124}, {-56, -80}});
  EXPECT_EQ(map, expected);
}

TEST(FlatSmallHashMap, Stress) {
  FlatSmallHashMap<int16_t, int16_t> test;
  std::map<int16_t, int16_t> reference;
  // srand(3);
  for (int i = 0; i < 500; ++i) {
    for (int j = 0; j < 50; ++j) {
      int16_t k = rand();
      int16_t v = rand();
      test.insert({k, v});
      reference.insert({k, v});
    }
    for (int j = 0; j < 50; ++j) {
      int16_t k = rand();
      test.erase(k);
      reference.erase(k);
    }
    std::map<int16_t, int16_t> copy(test.begin(), test.end());
    ASSERT_EQ(copy, reference);
  }
}

}  // namespace roo_collections
