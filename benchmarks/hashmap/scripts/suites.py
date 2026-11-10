"""Known benchmark suites; absent suite tags retain legacy general semantics."""

GENERAL_KEYS = ("u32_random", "u32_sequential", "u64_random", "string12", "string48")
SMALL_STRING_KEYS = ("small_string12", "small_string48")
SUITES = {"general": GENERAL_KEYS, "small_string": SMALL_STRING_KEYS}
CLASSIC_MAPS = ("roo", "absl_flat", "std_unordered", "absl_node", "std_map")
EXTRA_MAPS = ("tsl_robin", "unordered_dense", "etl", "boost_flat", "emhash5")
ALL_MAPS = CLASSIC_MAPS + EXTRA_MAPS
MAP_PROFILES = {"classic": CLASSIC_MAPS, "extended": ALL_MAPS}


def profile_for_maps(maps):
    """Reject incomplete rosters rather than silently dropping a competitor."""
    for name, expected in MAP_PROFILES.items():
        if set(maps) == set(expected):
            return name
    raise ValueError(f"Unknown or incomplete map roster: {sorted(maps)}")


def supports_workload(name, workload):
    return not (name == "etl" and workload == "build_grow")


def string_key(key):
    return key.startswith(("string", "small_string"))


def suite_for_keys(keys):
    """Require the exact key families of one known suite."""
    for name, expected in SUITES.items():
        if set(keys) == set(expected):
            return name
    raise ValueError(f"Unknown or mixed benchmark key families: {sorted(keys)}")
