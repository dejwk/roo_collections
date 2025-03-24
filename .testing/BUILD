cc_library(
    name = "roo_collections",
    visibility = ["//visibility:public"],
    srcs = glob(
        [
            "src/**/*.cpp",
            "src/**/*.h",
        ],
    ),
    includes = ["src"],
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
        "@roo_backport//:roo_backport",
    ],
)
