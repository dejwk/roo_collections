#include <cassert>
#include <map>
#include <string>
#include <unordered_map>

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
template <size_t N>
void CheckAdapters() {
  using Key = roo_collections::SmallString<N>;
  std::string first(N - 1, 'a');
  std::string last(first);
  last.back() = 'b';
  Key a(first);
  Key b(last);
  assert(a.length() == N - 1);
  assert(b.length() == N - 1);
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

}  // namespace

int main() {
  CheckAdapters<13>();
  CheckAdapters<49>();
}
