#pragma once

/// @file
/// @brief Language compatibility for shared mutable/immutable table helpers.

// C++11 constexpr functions cannot contain loops or mutate their arguments.
#if __cplusplus >= 201402L
#define ROO_COLLECTIONS_CONSTEXPR14 constexpr
#else
#define ROO_COLLECTIONS_CONSTEXPR14 inline
#endif
