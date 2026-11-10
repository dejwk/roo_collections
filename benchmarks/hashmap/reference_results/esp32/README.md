# ESP32 reference results — 2026-11-10

These are the latest recorded competitor runs for Roo's adopted
`word_bitmap8_fingerprint` implementation at a 75% fill limit. The measured
library revision is `4039bf335f20326628ceb5c535c3fc7218a2ea1b`. Each board ran one complete suite;
each result row summarizes seven timed samples. Publishing this snapshot adds
no new device measurements.

- [General comparison PDF](general/benchmark_charts.pdf): integer and string
  tests, with **Roo (SmallString)** added to every string panel, including
  retained memory. All other string series in this PDF use `std::string`.
- [SmallString comparison PDF](small_string/benchmark_charts.pdf): all five
  competitors use `SmallString<13>` or `<49>` for 12/48-character payloads.
- [Provenance and checksums](manifest.json),
  [general capture bundle](general/raw.tar.gz), and
  [SmallString capture bundle](small_string/raw.tar.gz).

Each PDF contains 11 pages: an overview, lookup, construction/copy,
iteration/mutation and cache charts for both hash policies, then retained-memory
and presizing comparisons. The extra Roo (SmallString) series uses a dashed line
or hatched bars. Its 48-character fixture ends at 400 entries; absent measurements
are not extrapolated.

## Measurements and interpretation

| Suite | Board | Cases | Harness revision |
|---|---|---:|---|
| General | ESP32-D0WD-V3 revision 3.1 (COM18) | 5,553 | `4039bf3` |
| SmallString | ESP32-D0WDQ6 revision 1.1 (COM20) | 2,142 | `2987370` |

Both use 240 MHz, internal RAM without PSRAM, C++17, `-O2`, `-DNDEBUG`,
pioarduino 55.03.37, Arduino 3.3.7, GCC 14.2.0 and Abseil 20250512.1. The full
metadata is preserved unchanged inside each bundle. Historical absolute paths
identify the original inputs; they are not required checkout locations.

The general PDF combines the two captures for side-by-side string comparisons.
The boards and harnesses differ, so timing differences also include code/cache
layout, allocation layout and hash-seed effects. This is not a controlled
same-binary experiment isolating the key representation. Compare individual
sizes in the PDFs; one suite provides no between-run confidence interval.
The older general metadata does not record heap poisoning or the full compiler
option vector; SmallString records light heap poisoning. The available source
hashes, toolchain identities and common flags are retained without filling in
missing historical fields.

## Current competitor comparison

Values are geometric means of **competitor time / Roo time**: above 1× favors
Roo. These tables use each container's default hasher (`std::map` is ordered)
and give equal weight to each available key/size case with at least 16 entries,
through that key's fixture limit. The general table has 50 cases per operation;
the SmallString table has 17. They summarize separate suites and should not be
interpreted as a before/after change caused by SmallString.

### General suite

| Operation | Abseil flat / Roo | std::unordered_map / Roo | Abseil node / Roo | std::map / Roo |
|---|---:|---:|---:|---:|
| Hit lookup | 1.98× | 0.83× | 1.99× | 1.52× |
| Miss lookup | 2.46× | 1.03× | 2.48× | 1.70× |
| Iteration | 1.25× | 0.20× | 1.25× | 1.49× |
| Erase + insert | 2.22× | 6.49× | 7.58× | 7.54× |

### SmallString suite

| Operation | Abseil flat / Roo | std::unordered_map / Roo | Abseil node / Roo | std::map / Roo |
|---|---:|---:|---:|---:|
| Hit lookup | 1.70× | 0.97× | 1.69× | 1.07× |
| Miss lookup | 2.03× | 0.94× | 2.01× | 0.67× |
| Iteration | 1.00× | 0.15× | 1.03× | 1.17× |
| Erase + insert | 2.29× | 3.38× | 7.29× | 3.70× |

### Retained memory at 128 entries

Bytes after growing from the default capacity, with default hashing. All series
except Roo (SmallString) below use `std::string`. Memory includes the map object,
allocator overhead and owned keys; query fixtures are excluded. This measures
retained heap, not peak allocation or fragmentation under arbitrary lifetimes.

| Payload | Roo (std::string) | Roo (SmallString) | Abseil flat | std::unordered_map | Abseil node | std::map |
|---|---:|---:|---:|---:|---:|---:|
| 12 characters | 7,492 | 5,460 | 7,468 | 7,856 | 7,100 | 7,720 |
| 48 characters | 16,200 | 14,676 | 16,172 | 16,520 | 15,712 | 16,424 |

## Regenerate tables and charts without hardware

Run from `benchmarks/hashmap` in a checkout containing this snapshot. Python 3.10+
is sufficient for unpacking, strict capture validation and CSV/report generation:

```sh
python scripts/bundle.py unpack reference_results/esp32/general/raw.tar.gz \
  --output results/reference-general \
  --sha256 b14a0e3b7fd29a1a70c92e01acbecfe81bf8ee8617877501b173f65f78aa4c52
python scripts/bundle.py unpack reference_results/esp32/small_string/raw.tar.gz \
  --output results/reference-small-string \
  --sha256 343d794e095e1c9d341ddfa6a7b204114d6e94a6322784c7b706e4210e78f600
python -m pip install -r scripts/requirements-charts.txt
python scripts/plot.py --results-dir results/reference-general \
  --small-string-results results/reference-small-string
python scripts/plot.py --results-dir results/reference-small-string
```

Unpack refuses to overwrite an existing directory; choose fresh output paths for
a repeat. Generated `summary.csv`, `measurements.csv`, reports, PNG/SVG charts,
HTML galleries and PDFs stay under the ignored `results/` directory. The summary
checksums in [manifest.json](manifest.json) verify numeric regeneration.

These PDFs were generated with tooling revision `ca645656079d4891da8fae3efaf7660e7f80a0c4`, Python 3.13.5
and Matplotlib 3.11.2. Use that revision and Matplotlib version for matching chart
rendering; other supported versions can render fonts or layout differently.
The general chart manifest records both input datasets and their hashes.

## Repeat the device measurements

Follow the current [benchmark setup and board backup/capture instructions](../../README.md)
using the pinned dependencies. Source selection for these historical runs is:

| Run | Library source checkout | Benchmark source checkout | Command |
|---|---|---|---|
| General | `4039bf3` | `4039bf3` | `r` / default capture |
| SmallString | `4039bf3` | `2987370` | `s` / `--suite small_string` |

Use separate checkouts/worktrees; for SmallString set `ROO_COLLECTIONS_SOURCE_DIR`
to the absolute path of the measured library checkout while building the newer
benchmark checkout. Use the current tooling revision's Python requirements for
both, including standalone esptool for backup/restore. The general harness was
originally built under a prototype path; its bytes match the committed benchmark
at the revision above. The SmallString harness and adapters likewise match their
recorded hashes. The downloaded roo_backport 1.2.4 source matches both captures.
Source hashes identify the measured inputs; a rebuild at another path can still
change binary layout and timings.

## Snapshot contents

Each `raw.tar.gz` contains only the original serial transcript, original build
metadata, and a checksum/tooling manifest. [manifest.json](manifest.json) records
checksums for the checked-in PDFs and bundles, exact source revisions and the
additional series in the general PDF. The two PDFs and compact inputs are the
checked-in presentation; the larger galleries and CSVs are regenerated on demand.
