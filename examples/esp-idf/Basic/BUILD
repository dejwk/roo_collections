load("@roo_testing//roo_testing/emulation:esp_idf.bzl", "roo_esp_idf_example")

roo_esp_idf_example(
    name = "Basic",
    srcs = ["main.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//:roo_collections",
        "@roo_testing//roo_testing/frameworks/esp-idf",
    ],
)
