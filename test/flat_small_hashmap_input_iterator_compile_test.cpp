#include <forward_list>
#include <utility>

#include "roo_collections/flat_small_hash_map.h"

// Verifies FlatSmallHashMap accepts non-random-access iterator ranges.
int main() {
  std::forward_list<std::pair<int, int>> entries = {{1, 10}, {2, 20}};
  roo_collections::FlatSmallHashMap<int, int> map(entries.begin(),
                                                  entries.end());
  return map.size() == 2 ? 0 : 1;
}