# ESP32 reference results — ten-container comparison

Fresh captures from 2026-11-10 compare Roo against nine external containers.
The five additions are Tessil robin_map 1.4.1, unordered_dense 5.3.3,
ETL unordered_map 20.49.0, Boost unordered_flat_map 1.90.0 and emhash5 1.3.0.
Roo uses the adopted `word_bitmap8_fingerprint` implementation and a 75% fill
limit. No Roo implementation changes were made for this comparison.

- [General comparison PDF](general/benchmark_charts.pdf): all ten containers;
  **Roo (SmallString)** is an additional series in every string panel.
- [SmallString comparison PDF](small_string/benchmark_charts.pdf): all ten
  containers use SmallString keys.
- [Provenance and checksums](manifest.json), [general raw bundle](general/raw.tar.gz),
  [SmallString raw bundle](small_string/raw.tar.gz).

Each PDF contains 11 pages: overview; lookup, construction/copy,
iteration/mutation and cache charts for both hash policies; retained memory;
and presizing. The dashed/hatched Roo (SmallString) series has 12- and
48-character inline keys. Its 48-character fixture ends at 400 entries.

## Capture and interpretation

| Suite | Device | Complete cases | Samples per case |
|---|---|---:|---:|
| General | ESP32-D0WD-V3 revision 3.1, COM18 (`/dev/ttyUSB1`) | 11,553 | 7 |
| SmallString | ESP32-D0WDQ6 revision 1.1, COM20 (`/dev/ttyUSB0`) | 4,460 | 7 |

One complete capture per board; zero discarded rows. All device correctness,
per-map heap-release and whole-suite heap checks passed. Both original firmware
images were restored and their full 4 MiB flash contents verified afterward.
The measured settings are 240 MHz, internal RAM without PSRAM, C++17, `-O2`,
`-DNDEBUG`, pioarduino 55.03.37, Arduino 3.3.7, GCC 14.2.0 and light heap poisoning.

Native default hashing and explicitly supplied `absl::Hash<Key>` are separate
series. Native hash postprocessing, type traits and load/capacity policies remain
intact. SmallString adapters preserve native string-content hashing and avalanche
traits; they are benchmark-only code. Dependencies are checksum-pinned and fetched
outside Git; see the [benchmark README](../../README.md) for adapter details.

ETL is fixed-capacity: exactly N nodes and N buckets. It has no growing-build
series. Its retained-memory numbers include the full embedded node/bucket
storage, owning adapter and allocator overhead. Lookup, iteration and mutation
call the native size-independent interface; construction/copy include adapter
ownership overhead. A stack/static ETL instance avoids the benchmark's owning
allocation overhead, but still requires its embedded storage.

The shared C++ local-static initialization machinery is warmed before the heap
baseline; its one-time allocation is recorded as `RUNTIME_INIT` in each raw log.
It is not attributed to individual maps. Per-map retained heap is not peak
memory, a count of allocations, or a fragmentation stress test.

Timings are medians of seven warm samples, with no between-run confidence
intervals. The expanded firmware has a different code/cache layout from the
previous five-container binary: changes from older captures do not by themselves
establish a library regression. Both suites use the same new firmware, but run
on different boards; the SmallString overlay does not isolate key representation
as the only changed factor. Inspect the by-size panels before deciding.

## Default-hash performance relative to Roo

Values are geometric means of **competitor time / Roo time**: above 1× favors
Roo, below 1× favors the competitor. Each available key/size case with at least
16 entries gets equal weight, through that key's fixture limit. There are
50 cases per operation in the general table and 17 in the SmallString table.
`std::map` uses ordering. Tiny-map results remain visible in the PDFs.

### General suite

| Container | Hit lookup | Miss lookup | Iteration | Erase + insert |
|---|---:|---:|---:|---:|
| Abseil flat | 1.95× | 2.45× | 1.26× | 2.22× |
| std::unordered_map | 0.84× | 1.07× | 0.19× | 8.32× |
| Abseil node | 2.00× | 2.50× | 1.27× | 8.34× |
| std::map | 1.45× | 1.78× | 1.52× | 7.51× |
| Tessil robin_map | 0.66× | 0.65× | 1.11× | 0.69× |
| unordered_dense | 2.81× | 3.28× | 0.19× | 2.62× |
| ETL (fixed N) | 0.70× | 0.82× | 0.75× | 0.78× |
| Boost flat | 1.17× | 1.15× | 1.04× | 1.18× |
| emhash5 | 0.54× | 0.56× | 0.75× | 0.55× |

### SmallString suite

| Container | Hit lookup | Miss lookup | Iteration | Erase + insert |
|---|---:|---:|---:|---:|
| Abseil flat | 1.82× | 2.36× | 1.30× | 2.14× |
| std::unordered_map | 1.04× | 1.12× | 0.20× | 3.11× |
| Abseil node | 1.82× | 2.36× | 1.29× | 6.13× |
| std::map | 1.14× | 0.78× | 1.50× | 3.44× |
| Tessil robin_map | 1.02× | 1.03× | 1.16× | 1.21× |
| unordered_dense | 1.94× | 2.52× | 0.20× | 2.17× |
| ETL (fixed N) | 0.90× | 0.90× | 0.73× | 0.90× |
| Boost flat | 1.03× | 1.01× | 1.07× | 1.13× |
| emhash5 | 0.97× | 0.99× | 0.78× | 0.92× |

### General suite with shared Abseil hashing

The same 50 cases, now supplying `absl::Hash<Key>` to every hash map. Container
postprocessing and capacity policies still differ; std::map remains ordered.

| Container | Hit lookup | Miss lookup | Iteration | Erase + insert |
|---|---:|---:|---:|---:|
| Abseil flat | 1.64× | 1.93× | 1.29× | 2.06× |
| std::unordered_map | 1.17× | 1.32× | 0.19× | 8.24× |
| Abseil node | 1.67× | 1.95× | 1.29× | 7.04× |
| std::map | 1.23× | 1.43× | 1.52× | 7.06× |
| Tessil robin_map | 0.82× | 0.84× | 1.11× | 0.78× |
| unordered_dense | 2.42× | 2.65× | 0.19× | 3.48× |
| ETL (fixed N) | 0.78× | 0.86× | 0.73× | 0.94× |
| Boost flat | 1.24× | 1.27× | 1.07× | 1.27× |
| emhash5 | 0.66× | 0.67× | 0.75× | 0.66× |

### Retained memory at 128 entries

Bytes with default hashing, after growth for dynamic containers and with capacity
128 for ETL. Includes the map object, owned keys and allocator overhead; excludes
the shared query fixtures. All string rows except Roo (SmallString) use std::string.

| Container | Random uint32 | String, 12 characters | String, 48 characters |
|---|---:|---:|---:|
| Roo | 2,388 | 7,508 | 16,196 |
| Abseil flat | 2,476 | 7,468 | 16,172 |
| std::unordered_map | 4,752 | 7,856 | 16,516 |
| Abseil node | 4,476 | 7,100 | 15,720 |
| std::map | 5,160 | 7,720 | 16,424 |
| Tessil robin_map | 3,264 | 8,768 | 17,520 |
| unordered_dense | 2,684 | 5,436 | 13,952 |
| ETL (fixed N) | 2,724 | 5,668 | 14,116 |
| Boost flat | 2,348 | 7,212 | 15,916 |
| emhash5 | 3,252 | 8,756 | 17,476 |
| Roo (SmallString) | — | 5,460 | 14,676 |

## Regenerate without hardware

From `benchmarks/hashmap`, using Python 3.10+:

```sh
python scripts/bundle.py unpack reference_results/esp32/general/raw.tar.gz \
  --output results/reference-general \
  --sha256 e001a68f64a3071d7625756a6b7d788e0f6c17b571ef866d355418777513d0c0
python scripts/bundle.py unpack reference_results/esp32/small_string/raw.tar.gz \
  --output results/reference-small-string \
  --sha256 4949e9a406dd33bba370bfd33059320c3e4b825082b5b0c59f736b16768977c0
python -m pip install -r scripts/requirements-charts.txt
python scripts/plot.py --results-dir results/reference-general \
  --small-string-results results/reference-small-string
python scripts/plot.py --results-dir results/reference-small-string
```

Use fresh output paths: unpack refuses to overwrite existing results. Generated
CSVs, reports, HTML galleries, PNG/SVG figures and PDFs stay in ignored `results/`.
The manifest records the exact numeric-summary and plotting-source checksums.
These PDFs use Python 3.13.5 and Matplotlib 3.11.2; rendering can vary with versions.

The raw metadata is preserved unchanged. Its base commit is
`6e232ae6de0a46a7cecfe7448e699ed4a68e74fc` plus the benchmark source
changes identified by exact SHA-256 hashes. Only benchmark code changed; the
measured Roo library sources are identified separately in the bundled metadata.
Historical absolute paths identify the build inputs, not required checkout paths.
To repeat on hardware, follow the [build/capture instructions](../../README.md).
Full flash backups, dependency trees and firmware binaries are not published.
