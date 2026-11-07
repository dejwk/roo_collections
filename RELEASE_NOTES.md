# roo_collections 1.4.9

- Upgrade the Bazel `roo_testing` dependency from 2.3.0 to 2.5.0.
- Add emulator test profiles for ESP32-S3 and ESP32-C3 with Arduino and ESP-IDF, plus RP2040 with Pico SDK, Arduino, and Arduino FreeRTOS.
- Expand `test_all_profiles` to run all nine supported profiles sequentially, and update the Bazel launcher to recognize Pico profiles.

---

# roo_collections 1.4.8

- Upgrade the Bazel `roo_testing` dependency from 2.1.2 to 2.3.0.
- Add a test helper that runs Arduino ESP32 and ESP-IDF ESP32 profiles sequentially.
- Refresh contributor and agent guidance, and standardize pointer and reference formatting.
- No changes to collection APIs or runtime behavior.

---

# roo_collections 1.4.7

- Updated Bazel dependencies: `rules_cc` to 0.2.25 and `googletest` to 1.18.0.bcr.1.
- Updated the shared `roo_testing` CI workflow to 2.1.2.
- Added consolidated historical release notes in `RELEASE_NOTES.md`.

---

# [roo_collections 1.4.6](https://github.com/dejwk/roo_collections/releases/tag/1.4.6)

Published 2026-08-29.

This release improves embedded example coverage and modernizes the project’s build and CI tooling.

### Added

- Emulator-runnable Arduino example support.
- Emulator-runnable ESP-IDF example support.
- Shared ESP32 Bazel configurations for Arduino and ESP-IDF.
- Centralized AddressSanitizer configuration.

### Changed

- Updated CI to use the shared `roo_testing` workflow, with pull-request and manual-run support.
- Updated Bazel dependencies:
  - `roo_testing` to 2.1.0
  - `roo_backport` to 1.2.3
- The main library target now links Arduino support automatically when built for Arduino platforms.

### Compatibility

- Public collection APIs are unchanged.
- PlatformIO users now require `roo_backport` 1.2.3 or newer.

---

# [roo_collections 1.4.5](https://github.com/dejwk/roo_collections/releases/tag/1.4.5)

Published 2026-08-07.

 Re-formatted the sources using clang-format.

**Full Changelog**: https://github.com/dejwk/roo_collections/compare/1.4.4...1.4.5

---

# [roo_collections 1.4.4](https://github.com/dejwk/roo_collections/releases/tag/1.4.4)

Published 2026-06-04.

* fixed bug in SmallString, ensuring that the string gets zero-terminated when truncated.
* fixed a couple of compilation issues.
* fixing Murmur3 to behave as the actual Murmur3.

**Full Changelog**: https://github.com/dejwk/roo_collections/compare/1.4.3...1.4.4

---

# [roo_collections 1.4.3](https://github.com/dejwk/roo_collections/releases/tag/1.4.3)

Published 2026-02-25.

* Added doxygen-style public documentation.

**Full Changelog**: https://github.com/dejwk/roo_collections/compare/1.4.2...1.4.3

---

# [roo_collections 1.4.2](https://github.com/dejwk/roo_collections/releases/tag/1.4.2)

Published 2026-02-24.

Updated dependencies.

**Full Changelog**: https://github.com/dejwk/roo_collections/compare/1.4.1...1.4.2

---

# [roo_collections 1.4.1](https://github.com/dejwk/roo_collections/releases/tag/1.4.1)

Published 2026-01-26.

Fixing unit test setup, after the change of behavior in Bazel.

**Full Changelog**: https://github.com/dejwk/roo_collections/compare/1.4.0...1.4.1

---

# [roo_collections 1.4.0](https://github.com/dejwk/roo_collections/releases/tag/1.4.0)

Published 2026-01-06.

* Made work with esp-idf, and Raspberry Pi Pico.

**Full Changelog**: https://github.com/dejwk/roo_collections/compare/1.3.2...1.4.0

---

# [roo_collections 1.3.2](https://github.com/dejwk/roo_collections/releases/tag/1.3.2)

Published 2025-10-30.

Picking up updated dependencies; better CI, .gitignore.

---

# [roo_collections 1.3.1](https://github.com/dejwk/roo_collections/releases/tag/1.3.1)

Published 2025-09-19.

Bugfix: memory leak when hashtable grows.

Added address sanitizer tests to the CI/CD pipeline to avoid regressions.

**Full Changelog**: https://github.com/dejwk/roo_collections/compare/1.3.0...1.3.1

---

# [roo_collections 1.3.0](https://github.com/dejwk/roo_collections/releases/tag/1.3.0)

Published 2025-08-10.

Turning roo_collections into a Bazel module, to allow depending libraries to use it in unit tests.

**Full Changelog**: https://github.com/dejwk/roo_collections/compare/1.2.1...1.3.0

---

# [roo_collections 1.2.1](https://github.com/dejwk/roo_collections/releases/tag/1.2.1)

Published 2025-07-31.

Bugfix: making it possible to store SmallString in hashtables.

---

# [roo_collections 1.2.0](https://github.com/dejwk/roo_collections/releases/tag/1.2.0)

Published 2025-07-04.

New features:
* 'transparent' collections: you can now search strings in string maps without making string copies.
* better compatibility with std::map.
* added SmallString, for capped size strings that don't allocate data on the heap.
* added support for custom key comparators.
* added a murmur hash implementation.
* added the ability to erase elements while iterating.

Fixes:
* you can now store non-copyable entries.
* compiles under Arduino IDE.
* fixed crash when insert from dummy empty hashtable.
* better compilation error messages.

Performance improvements:
* fast-path erase in case there was just one element.
* making it cheaper to insert, with in-place rehashing.
* using a quick-check (using 7-bit hash) to reduce the cost of equality comparison.


---

# [roo_collections 1.1.6](https://github.com/dejwk/roo_collections/releases/tag/1.1.6)

Published 2024-01-03.

Reducing memory footprint for tiny hashtables. In particular, it is now possible to have a zero-capacity hashtable, which will not allocate any memory dynamically. This is useful when hashtable inserts happen in rare, exceptional circumstances.

---

# [roo_collections 1.1.5](https://github.com/dejwk/roo_collections/releases/tag/1.1.5)

Published 2023-12-23.

Adding an empty header file, to enable Arduino IDE to accept:

#include <roo_collections.h>
#include <roo_collections/flat_small_hashmap.h>


---

# [roo_collections 1.1.4](https://github.com/dejwk/roo_collections/releases/tag/1.1.4)

Published 2023-12-04.

Flattening header file structure to make this library compatible with Arduino IDE, which seems to be unhappy about relative nested header references.

---

# [roo_collections 1.1.3](https://github.com/dejwk/roo_collections/releases/tag/1.1.3)

Published 2023-12-03.

Fixing Arduino IDE compatibility: allowing dependent libs to pick up individual headers.

---

# [roo_collections 1.1.2](https://github.com/dejwk/roo_collections/releases/tag/1.1.2)

Published 2023-11-26.

Made compatible with Arduino IDE (and the Arduino library manager).

---

# [roo_collections 1.1](https://github.com/dejwk/roo_collections/releases/tag/1.1)

Published 2023-11-11.

Bug fixes and new method implementations, to align the hash map more closely with the standard C++ library.

---

# [roo_collections 1.0](https://github.com/dejwk/roo_collections/releases/tag/1.0)

Published 2023-11-11.

Initial release.

---

