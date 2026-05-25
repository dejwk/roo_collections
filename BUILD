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
    deps = ["@roo_backport"],
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
