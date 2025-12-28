#include <Arduino.h>

#include "roo_collections/flat_small_hash_map.h"
#include "roo_collections/flat_small_hash_set.h"

void setup() { Serial.begin(115200); }

void loop() {
  {
    Serial.println("set of int32_t");
    roo_collections::FlatSmallHashSet<int32_t> my_set;
    my_set.insert(4);
    my_set.insert(45);
    my_set.insert(1000);
    my_set.insert(2);
    my_set.insert(513);
    my_set.insert(26);
    my_set.insert(2);
    Serial.println(my_set.size());
    Serial.println(my_set.erase(45));
    Serial.println(my_set.size());
    Serial.println(my_set.erase(44));
    Serial.println(my_set.size());
    Serial.println(my_set.erase(45));
    Serial.println(my_set.size());
    for (const int32_t& e : my_set) {
      Serial.println(e);
    }
    for (int i = 0; i < 10000; i++) {
      my_set.insert(i);
    }
    Serial.println(my_set.size());
    my_set.clear();
    Serial.println(my_set.size());
  }

  {
    Serial.println("set of std::string");
    roo_collections::FlatSmallHashSet<std::string> my_set;
    my_set.insert("a");
    my_set.insert("b");
    my_set.insert("c");
    my_set.insert("d");
    Serial.println(my_set.size());
    Serial.println(my_set.erase("b"));
    Serial.println(my_set.size());
    Serial.println(my_set.erase("c"));
    for (const std::string& e : my_set) {
      Serial.println(String(e.data(), e.size()));
    }
    my_set.clear();
    Serial.println(my_set.size());
  }

  {
    Serial.println("set of Arduino String");
    roo_collections::FlatSmallHashSet<String> my_set;
    my_set.insert("a");
    my_set.insert("b");
    my_set.insert("c");
    my_set.insert("d");
    Serial.println(my_set.size());
    Serial.println(my_set.erase("b"));
    Serial.println(my_set.size());
    Serial.println(my_set.erase("c"));
    for (const String& e : my_set) {
      Serial.println(e);
    }
    my_set.clear();
    Serial.println(my_set.size());
  }

  {
    Serial.println("Map from std::string to int");
    roo_collections::FlatSmallHashMap<std::string, int> my_map;
    my_map.insert(std::make_pair("a", 1));
    my_map.insert(std::make_pair("b", 2));
    my_map.insert(std::make_pair("c", 3));
    my_map.insert(std::make_pair("d", 4));
    Serial.println(my_map.size());
    Serial.println(my_map.erase("b"));
    Serial.println(my_map.size());
    Serial.println(my_map.erase("b"));
    Serial.println(my_map.contains("a"));
    Serial.println(my_map.contains("b"));
    for (const auto& e : my_map) {
      Serial.printf("%s: %d\n", e.first.data(), e.second);
    }
    Serial.println(my_map["a"]);
    my_map["a"] = 1000;
    Serial.println(my_map["a"]);

    my_map.clear();
    Serial.println(my_map.size());
  }

  {
    Serial.println(
        "Map from std::string to int, accepting heterogeneous key types");
    roo_collections::FlatSmallStringHashMap<int> my_map;
    // This map is similar to FlatSmallHashMap<std::string, int> in that it uses
    // std::string as key. With this map however, you can use not just
    // std::string, but also, roo::string_view, Arduino String, or plain const
    // char* as a lookup key, and the implementation will not create temporary
    // string objects.
    my_map["a"] = 1;
    my_map[std::string("b")] = 2;
    my_map[roo::string_view("c")] = 3;
    my_map.insert(std::make_pair("d", 4));
    Serial.println(my_map.size());
    Serial.println(my_map.erase("b"));
    Serial.println(my_map.size());
    Serial.println(my_map.erase("b"));
    Serial.println(my_map.contains(roo::string_view("a")));
    Serial.println(my_map.contains(String("b")));
    for (const auto& e : my_map) {
      Serial.printf("%s: %d\n", e.first.data(), e.second);
    }
    Serial.println(my_map["a"]);
    my_map["a"] = 1000;
    Serial.println(my_map["a"]);

    my_map.clear();
    Serial.println(my_map.size());
  }

  {
    Serial.println("Map using constant C strings as keys");
    roo_collections::FlatSmallHashMap<roo::string_view, int> my_map;
    // In this map, storage for keys is not allocated; the map simply references
    // PROGMEM. (Each entry uses just the size of string_view, i.e. 8 bytes).
    // (You can also use any strings pre-allocated in RAM and referencable as
    // roo::string_view, as long as they remain constant for as long as they are
    // used in the map).
    my_map["a"] = 1;
    my_map[std::string("b")] = 2;
    my_map[roo::string_view("c")] = 3;
    my_map.insert(std::make_pair("d", 4));
    Serial.println(my_map.size());
    Serial.println(my_map.erase("b"));
    Serial.println(my_map.size());
    Serial.println(my_map.erase("b"));
    Serial.println(my_map.contains(roo::string_view("a")));
    Serial.println(my_map.contains("b"));
    for (const auto& e : my_map) {
      Serial.printf("%s: %d\n", e.first.data(), e.second);
    }
    Serial.println(my_map["a"]);
    my_map["a"] = 1000;
    Serial.println(my_map["a"]);

    my_map.clear();
    Serial.println(my_map.size());
  }

  delay(1000);
}
