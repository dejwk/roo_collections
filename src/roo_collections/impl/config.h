#pragma once

/// @file
/// @brief Language compatibility for shared mutable/immutable table helpers.

// C++11 constexpr functions cannot contain loops or mutate their arguments.
#if __cplusplus >= 201402L
#define ROO_COLLECTIONS_CONSTEXPR14 constexpr
#else
#define ROO_COLLECTIONS_CONSTEXPR14 inline
#endif

// Feature detection can be overridden for toolchains without this extension.
#ifndef ROO_COLLECTIONS_HAS_BUILTIN_CTZ
#if defined(__has_builtin)
#define ROO_COLLECTIONS_HAS_BUILTIN_CTZ __has_builtin(__builtin_ctz)
#elif defined(__GNUC__)
#define ROO_COLLECTIONS_HAS_BUILTIN_CTZ 1
#else
#define ROO_COLLECTIONS_HAS_BUILTIN_CTZ 0
#endif
#endif
