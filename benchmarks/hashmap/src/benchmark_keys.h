#pragma once

#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "absl/strings/string_view.h"
#include "roo_collections/small_string.h"

// Benchmark-only interoperability hooks. SmallString itself supplies equality
// and Roo hashing; these additions let all competitors store the same key type.
namespace roo_collections {

/// Hashes SmallString content through Abseil's normal string-view path.
template <typename H, size_t N>
H AbslHashValue(H state, const SmallString<N>& value) {
  return H::combine(std::move(state),
                    absl::string_view(value.c_str(), value.length()));
}

/// Provides lexicographic ordering for the benchmark's std::map competitor.
template <size_t N>
bool operator<(const SmallString<N>& lhs, const SmallString<N>& rhs) {
  return std::strcmp(lhs.c_str(), rhs.c_str()) < 0;
}

}  // namespace roo_collections

namespace std {

/// Supplies std::unordered_map with a string-content hash for this user type.
template <size_t N>
struct hash<roo_collections::SmallString<N>> {
  size_t operator()(const roo_collections::SmallString<N>& value) const {
    return hash<string_view>{}(string_view(value.c_str(), value.length()));
  }
};

}  // namespace std

namespace hashmap_benchmark {

template <typename Key>
struct IsStringKey : std::is_same<Key, std::string> {};

template <size_t N>
struct IsStringKey<roo_collections::SmallString<N>> : std::true_type {};

}  // namespace hashmap_benchmark
