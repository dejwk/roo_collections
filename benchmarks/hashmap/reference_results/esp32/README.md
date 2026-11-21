# ESP32 reference results — ten-container comparison

All ten containers were remeasured on 2026-11-21. The firmware uses one
compiled timing loop per workload for both warm-up and sampling.

- [General comparison PDF](general/benchmark_charts.pdf) includes **Roo (SmallString)** as an additional series in every string panel.
- [SmallString comparison PDF](small_string/benchmark_charts.pdf) uses SmallString keys for all ten containers.
- [Detailed general tables](general/REPORT.md) and [SmallString tables](small_string/REPORT.md).
- Full captures and build metadata are available for [general](general/shared-timer.tar.gz) and [SmallString](small_string/shared-timer.tar.gz).
- [Provenance and checksums](manifest.json).

Each PDF contains 11 pages covering overview, lookup, construction and copy,
iteration and mutation, cache churn, retained memory, and presizing. The
dashed Roo (SmallString) series uses 12- and 48-character inline keys. Its
48-character fixture ends at 256 entries.

## Capture and interpretation

| Suite | Device | Validated cases |
|---|---|---:|
| General | ESP32-D0WD-V3 revision 3.1 | 11,553 |
| SmallString | ESP32-D0WD-V3 revision 3.1 | 4,310 |

Both charts use captures from the same firmware and ESP32-D0WD-V3 board.
The settings include 240 MHz, internal RAM without PSRAM, C++17, `-O2`, `-DNDEBUG`, pioarduino
55.03.37, Arduino 3.3.7, GCC 14.2.0, and light heap poisoning. Each case has
one observation with the median of seven timed samples. All device correctness
and per-map heap checks passed across the charted range. The 400-entry
48-character SmallString fixture needed a 61,440-byte allocation, while
the largest free block was 61,428 bytes. Both PDFs use the complete
48-character range through 256 entries. The SmallString archive includes
the full serial transcript and specifies the size limit for regeneration.
Both boards' original 4 MiB flash images were restored and verified.

Each suite uses identical key fixtures across its containers. The SmallString
fixtures use aligned, length-delimited `SmallString<16>` and `SmallString<52>`.
Charts show default hashing and explicit `absl::Hash<Key>` separately.
Container hash postprocessing, type traits, and load and capacity policies
still differ. Roo uses a 75% fill limit, and `std::map` uses ordering.

ETL is fixed-capacity with exactly N nodes and N buckets. Its retained-memory
numbers include the embedded storage, owning adapter, and allocator overhead.
Construction and copy include adapter ownership overhead. Its native
size-independent interface handles lookup, iteration, and mutation.

The benchmark warms local-static initialization before recording heap
baselines. Each raw log records the one-time allocation in `RUNTIME_INIT`.
Retained heap measures memory held by the map, including its owned keys.

## Shared timing loop

In the pre-fix general Roo capture from 2026-11-21, separate warm-up and
sampling loops had different flash-cache placement on the ESP32. Roo with `absl::Hash<uint64_t>`
reported 24.79 µs per erase+insert pair at one entry, with a recorded batch of
101.5 ms against a 3 ms calibration target. With the shared loop, the same
case measures 0.957 µs per pair and 3.92 ms per batch. The independent Roo-only
sweep reproduced the full run's medians for all 17 sizes at the reported
precision.

## Default-hash performance relative to Roo

Values are geometric means of **competitor time / Roo time**. Values above 1×
favor Roo. Each available key and size case with at least 16 entries gets
equal weight, through that key's fixture limit. There are 50 cases per
operation in the general table and 16 in the SmallString table. The PDFs
also show smaller maps.

### General suite

| Container | Hit lookup | Miss lookup | Iteration | Erase + insert |
|---|---:|---:|---:|---:|
| Abseil flat | 2.67× | 2.62× | 1.06× | 2.52× |
| std::unordered_map | 1.21× | 1.21× | 0.17× | 7.23× |
| Abseil node | 2.69× | 2.62× | 1.08× | 8.50× |
| std::map | 2.38× | 2.17× | 1.39× | 8.49× |
| Tessil robin_map | 0.85× | 0.70× | 1.01× | 0.76× |
| unordered_dense | 4.75× | 3.82× | 0.24× | 2.92× |
| ETL (fixed N) | 1.23× | 1.07× | 0.54× | 0.90× |
| Boost flat | 1.64× | 1.32× | 1.03× | 1.37× |
| emhash5 | 0.76× | 0.60× | 0.70× | 0.62× |

### SmallString suite

| Container | Hit lookup | Miss lookup | Iteration | Erase + insert |
|---|---:|---:|---:|---:|
| Abseil flat | 1.99× | 2.33× | 1.08× | 2.67× |
| std::unordered_map | 1.04× | 1.05× | 0.17× | 4.25× |
| Abseil node | 2.01× | 2.30× | 1.04× | 5.94× |
| std::map | 1.71× | 1.83× | 1.40× | 5.63× |
| Tessil robin_map | 1.05× | 1.12× | 1.03× | 1.12× |
| unordered_dense | 3.93× | 2.60× | 0.24× | 2.65× |
| ETL (fixed N) | 1.12× | 1.25× | 0.53× | 0.85× |
| Boost flat | 1.23× | 1.17× | 1.04× | 1.15× |
| emhash5 | 0.99× | 1.06× | 0.73× | 0.90× |

### General suite with shared Abseil hashing

| Container | Hit lookup | Miss lookup | Iteration | Erase + insert |
|---|---:|---:|---:|---:|
| Abseil flat | 1.93× | 1.95× | 1.06× | 2.19× |
| std::unordered_map | 1.38× | 1.35× | 0.17× | 6.63× |
| Abseil node | 1.95× | 1.94× | 1.08× | 7.46× |
| std::map | 1.73× | 1.63× | 1.39× | 7.45× |
| Tessil robin_map | 0.91× | 0.84× | 1.02× | 0.84× |
| unordered_dense | 2.98× | 3.00× | 0.24× | 2.62× |
| ETL (fixed N) | 0.90× | 0.90× | 0.53× | 0.84× |
| Boost flat | 1.49× | 1.33× | 1.03× | 1.33× |
| emhash5 | 0.81× | 0.67× | 0.71× | 0.70× |

### Retained memory at 128 entries

Bytes with default hashing, after growth for dynamic containers and with
capacity 128 for ETL. The totals include the map object, owned keys, and
allocator overhead. Query fixtures are shared.

| Container | Random uint32 | String, 12 characters | String, 48 characters |
|---|---:|---:|---:|
| Roo | 2,352 | 7,472 | 16,180 |
| Abseil flat | 2,476 | 7,468 | 16,172 |
| std::unordered_map | 4,752 | 7,856 | 16,516 |
| Abseil node | 4,476 | 7,100 | 15,720 |
| std::map | 5,160 | 7,720 | 16,424 |
| Tessil robin_map | 3,264 | 8,768 | 17,520 |
| unordered_dense | 2,684 | 5,436 | 13,952 |
| ETL (fixed N) | 2,724 | 5,668 | 14,116 |
| Boost flat | 2,348 | 7,212 | 15,916 |
| emhash5 | 3,252 | 8,756 | 17,476 |
| Roo (SmallString) | — | 5,680 | 14,896 |

## Regenerate without hardware

From `benchmarks/hashmap`, using Python 3.10 or later:

```sh
python scripts/bundle.py unpack reference_results/esp32/general/shared-timer.tar.gz \
  --output results/reference-general
python scripts/bundle.py unpack reference_results/esp32/small_string/shared-timer.tar.gz \
  --output results/reference-small-string
python -m pip install -r scripts/requirements-charts.txt
python scripts/plot.py --results-dir results/reference-general \
  --small-string-results results/reference-small-string
python scripts/plot.py --results-dir results/reference-small-string
```

Use fresh output paths. Unpacking verifies the archived log and metadata
checksums and validates every case in the charted range before generating tables. The
[manifest](manifest.json) records the complete archive checksums for the
optional `--sha256` argument. It also records chart inputs and tooling hashes.
To repeat on hardware, follow the [build and capture instructions](../../README.md).

The pre-fix full captures for [general](general/raw.tar.gz) and
[SmallString](small_string/raw.tar.gz), and the corresponding Roo-only captures
for [general](general/roo-update.tar.gz) and [SmallString](small_string/roo-update.tar.gz),
remain available as historical measurements.
