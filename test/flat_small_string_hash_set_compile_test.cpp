#include <string>

#include "roo_collections/flat_small_hash_set.h"

// Verifies FlatSmallStringHashSet can be instantiated and used with string
// inserts.
int main() {
  roo_collections::FlatSmallStringHashSet set;
  set.insert(std::string("alpha"));
  return set.contains("alpha") ? 0 : 1;
}