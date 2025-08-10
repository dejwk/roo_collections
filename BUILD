# BUILD file for use with https://github.com/dejwk/roo_testing.

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
    deps = ["@roo_backport"],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "flat_hashmap_test",
    srcs = [
        "test/flat_hashmap_test.cpp",
    ],
    includes = ["src"],
    copts = ["-Iexternal/gtest/include"],
    linkstatic = 1,
    deps = [
        ":roo_collections",
        "@googletest//:gtest_main",
    ],
    size = "small"
)
