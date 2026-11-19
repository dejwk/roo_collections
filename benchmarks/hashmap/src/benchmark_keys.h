#pragma once

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
  const roo::string_view view = value.view();
  return H::combine(std::move(state),
                    absl::string_view(view.data(), view.size()));
}

/// Provides lexicographic ordering for the benchmark's std::map competitor.
template <size_t N>
bool operator<(const SmallString<N>& lhs, const SmallString<N>& rhs) {
  return lhs.view() < rhs.view();
}

}  // namespace roo_collections

namespace std {

/// Supplies std::unordered_map with a string-content hash for this user type.
template <size_t N>
struct hash<roo_collections::SmallString<N>> {
  size_t operator()(const roo_collections::SmallString<N>& value) const {
    return hash<string_view>{}(value.view());
  }
};

}  // namespace std

namespace hashmap_benchmark {

// The 16- and 52-byte strides keep the payload at a word boundary in
// ESP32 query vectors for both hashing and equality.
using SmallString12Key = roo_collections::SmallString<16>;
using SmallString48Key = roo_collections::SmallString<52>;

static_assert(sizeof(SmallString12Key) % 4 == 0);
static_assert(sizeof(SmallString48Key) % 4 == 0);

template <typename Key>
struct IsStringKey : std::is_same<Key, std::string> {};

template <size_t N>
struct IsStringKey<roo_collections::SmallString<N>> : std::true_type {};

}  // namespace hashmap_benchmark
