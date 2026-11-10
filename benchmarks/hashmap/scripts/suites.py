"""Known benchmark suites; absent suite tags retain legacy general semantics."""

GENERAL_KEYS = ("u32_random", "u32_sequential", "u64_random", "string12", "string48")
SMALL_STRING_KEYS = ("small_string12", "small_string48")
SUITES = {"general": GENERAL_KEYS, "small_string": SMALL_STRING_KEYS}


def string_key(key):
    return key.startswith(("string", "small_string"))


def suite_for_keys(keys):
    """Require the exact key families of one known suite."""
    for name, expected in SUITES.items():
        if set(keys) == set(expected):
            return name
    raise ValueError(f"Unknown or mixed benchmark key families: {sorted(keys)}")
