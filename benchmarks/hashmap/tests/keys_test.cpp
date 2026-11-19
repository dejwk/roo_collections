#include <cassert>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "benchmark_keys.h"

namespace {

struct InspectHash {
  std::string content;

  static InspectHash combine(InspectHash, absl::string_view value) {
    return {std::string(value.data(), value.size())};
  }
};

// Verifies the adapters retain the final payload character, hash content
// without the terminator, and provide consistent ordering and equality.
template <typename Key>
void CheckAdapters(size_t payload_size) {
  std::string first(payload_size, 'a');
  std::string last(first);
  last.back() = 'b';
  Key a(first);
  Key b(last);
  assert(a.length() == payload_size);
  assert(b.length() == payload_size);
  assert(a != b);
  assert(a < b);
  assert(!(b < a));
  assert(AbslHashValue(InspectHash{}, a).content == first);
  assert(AbslHashValue(InspectHash{}, b).content == last);
  std::unordered_map<Key, unsigned> hashed;
  std::map<Key, unsigned> ordered;
  hashed.emplace(a, 1);
  hashed.emplace(b, 2);
  ordered.emplace(b, 2);
  ordered.emplace(a, 1);
  assert(hashed.size() == 2);
  assert(hashed.find(Key(first))->second == 1);
  assert(ordered.begin()->first == a);
}

// Verifies that each query's payload starts on a word boundary.
template <typename Key>
void CheckQueryAlignment() {
  const std::vector<Key> queries(8, Key("key"));
  for (const Key& query : queries) {
    assert(reinterpret_cast<std::uintptr_t>(query.view().data()) % 4 == 0);
  }
}

}  // namespace

int main() {
  CheckAdapters<hashmap_benchmark::SmallString12Key>(12);
  CheckAdapters<hashmap_benchmark::SmallString48Key>(48);
  CheckQueryAlignment<hashmap_benchmark::SmallString12Key>();
  CheckQueryAlignment<hashmap_benchmark::SmallString48Key>();
}
