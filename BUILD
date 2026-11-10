# BUILD file for use with https://github.com/dejwk/roo_testing.

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

cc_library(
    name = "roo_collections",
    srcs = glob(
        [
            "src/**/*.cpp",
            "src/**/*.h",
        ],
    ),
    includes = [
        "src",
    ],
    visibility = ["//visibility:public"],
    deps = ["@roo_backport"] + select({
        "@roo_testing//roo_testing/platforms:is_arduino": [
            "@roo_testing//:arduino",
        ],
        "//conditions:default": [],
    }),
)

cc_test(
    name = "flat_hashmap_test",
    size = "small",
    srcs = [
        "test/flat_hashmap_test.cpp",
    ],
    copts = ["-Iexternal/gtest/include"],
    includes = ["src"],
    linkstatic = 1,
    deps = [
        ":roo_collections",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "hash_test",
    size = "small",
    srcs = [
        "test/hash_test.cpp",
    ],
    copts = ["-Iexternal/gtest/include"],
    includes = ["src"],
    linkstatic = 1,
    deps = [
        ":roo_collections",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "flat_small_string_hash_set_compile_test",
    size = "small",
    srcs = [
        "test/flat_small_string_hash_set_compile_test.cpp",
    ],
    includes = ["src"],
    linkstatic = 1,
    deps = [
        ":roo_collections",
    ],
)

cc_test(
    name = "flat_small_hashmap_input_iterator_compile_test",
    size = "small",
    srcs = [
        "test/flat_small_hashmap_input_iterator_compile_test.cpp",
    ],
    includes = ["src"],
    linkstatic = 1,
    deps = [
        ":roo_collections",
    ],
)

cc_test(
    name = "flat_small_hashmap_const_neq_compile_test",
    size = "small",
    srcs = [
        "test/flat_small_hashmap_const_neq_compile_test.cpp",
    ],
    includes = ["src"],
    linkstatic = 1,
    deps = [
        ":roo_collections",
    ],
)

cc_test(
    name = "small_string_asan_test",
    size = "small",
    srcs = [
        "test/small_string_test.cpp",
    ],
    copts = [
        "-Iexternal/gtest/include",
        "-fsanitize=address",
    ],
    includes = ["src"],
    linkopts = ["-fsanitize=address"],
    linkstatic = 1,
    deps = [
        ":roo_collections",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "flat_small_hashtable_primitives_test",
    size = "small",
    srcs = ["test/flat_small_hashtable_primitives_test.cpp"],
    deps = [
        ":roo_collections",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "immutable_flat_small_hashtable_test",
    size = "small",
    srcs = ["test/immutable_flat_small_hashtable_test.cpp"],
    deps = [
        ":roo_collections",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "immutable_flat_small_hash_map_test",
    size = "small",
    srcs = ["test/immutable_flat_small_hash_map_test.cpp"],
    deps = [
        ":roo_collections",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "immutable_flat_small_hash_set_test",
    size = "small",
    srcs = ["test/immutable_flat_small_hash_set_test.cpp"],
    deps = [
        ":roo_collections",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "cpp11_compatibility_test",
    size = "small",
    srcs = ["test/language_version_compile_test.cpp"],
    copts = [
        "-std=c++11",
        "-DROO_COLLECTIONS_TEST_CXX=201103L",
    ],
    deps = [":roo_collections"],
)

cc_test(
    name = "cpp14_compatibility_test",
    size = "small",
    srcs = ["test/language_version_compile_test.cpp"],
    copts = [
        "-std=c++14",
        "-DROO_COLLECTIONS_TEST_CXX=201402L",
    ],
    deps = [":roo_collections"],
)

cc_test(
    name = "cpp17_compatibility_test",
    size = "small",
    srcs = ["test/language_version_compile_test.cpp"],
    copts = [
        "-std=c++17",
        "-DROO_COLLECTIONS_TEST_CXX=201703L",
    ],
    deps = [":roo_collections"],
)

cc_test(
    name = "read_only_flat_small_hashtable_test",
    size = "small",
    srcs = ["test/read_only_flat_small_hashtable_test.cpp"],
    deps = [
        ":roo_collections",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "flat_small_hashtable_capacity_test",
    size = "small",
    srcs = ["test/flat_small_hashtable_capacity_test.cpp"],
    deps = [
        ":roo_collections",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "flat_small_hashtable_churn_test",
    size = "small",
    srcs = ["test/flat_small_hashtable_churn_test.cpp"],
    deps = [
        ":roo_collections",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "flat_small_hashtable_iterator_test",
    size = "small",
    srcs = ["test/flat_small_hashtable_iterator_test.cpp"],
    deps = [
        ":roo_collections",
        "@googletest//:gtest_main",
    ],
)
