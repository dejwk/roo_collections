#include "roo_collections/flat_small_hash_map.h"

// Verifies const hash maps support operator!= comparisons.
int main() {
  const roo_collections::FlatSmallHashMap<int, int> lhs;
  const roo_collections::FlatSmallHashMap<int, int> rhs;
  return lhs != rhs;
}