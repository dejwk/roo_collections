# ESP32 hash map results

[Full shared-timer capture and metadata](shared-timer.tar.gz) · [Chart regeneration instructions](../README.md#regenerate-without-hardware)

Benchmark suite: `small_string`; map profile: `extended`.

Validated 4310 rows in the requested size range. The 48-character SmallString fixture ends at 256 entries in these results. The raw log includes the subsequent 400-entry attempt. Each observation is the median of seven timed samples. Between-suite variation is unavailable with one suite.

Measured Roo Collections source: commit `240cff048f5c2cdfe6a70f2987b4c0ed4bb86c2f`. Source hashes in build_metadata.json identify the measured working tree.

## Measurement

Mutable maps; warm repeated access; deterministic keys and visit order. Lookup/iteration are ns per entry, erase+insert is ns per pair, copy/build include destruction, and cache churn is ns per 90 hits + 10 misses + one eviction + one insertion. Heap is retained internal 8-bit memory including the map object and owned keys, not peak memory.

Build metadata: `build_metadata.json`. Raw logs are preserved; summary.csv contains medians and ranges, and measurements.csv contains each validated observation. Default policies are not normalized.

Transport separators recovered: 0; excluded records: 0. Payload fields are never repaired. See excluded_records.json for coverage and exclusions.

## Device records

```text
esp32.log: BEGIN,1,chip=ESP32-D0WD-V3,revision=301,cpu_mhz=240,core=1,heap=324876,psram=0,compiler=14.2.0,absl=20250512.1,group_width=8,suite=small_string,profile=extended
```

## small_string12, 100 entries, default hashing

| Map | Hit | Miss | Iterate | Copy | Erase+insert | Build | Presized build | Heap B |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| roo | 1031.3 | 728.3 | 194.4 | 614.3 | 3241.4 | 4069.6 | 1769.8 | 5680 |
| absl_flat | 2052.6 | 1944.1 | 135.4 | 3166.8 | 8421.4 | 7620.9 | 5005.9 | 2860 |
| std_unordered | 1153.7 | 882.2 | 29.7 | 15260.7 | 15115.0 | 15773.4 | 15355.9 | 4992 |
| absl_node | 2042.0 | 1821.4 | 138.8 | 17757.6 | 20298.2 | 21474.2 | 19003.0 | 4324 |
| std_map | 2385.7 | 2367.3 | 250.8 | 15310.5 | 21580.5 | 17777.7 | 17777.7 | 5240 |
| tsl_robin | 1080.7 | 909.0 | 201.9 | 482.7 | 3048.5 | 4957.5 | 2363.5 | 6464 |
| unordered_dense | 2689.0 | 2328.0 | 42.2 | 766.8 | 9014.9 | 6552.6 | 4122.2 | 3516 |
| etl | 1129.6 | 941.2 | 94.6 | 1617.5 | 2826.2 | — | 2115.3 | 3364 |
| boost_flat | 1378.5 | 1001.5 | 183.8 | 454.3 | 10503.7 | 3550.2 | 2123.9 | 2604 |
| emhash5 | 1117.6 | 1028.7 | 107.7 | 499.8 | 3009.1 | 4032.3 | 2125.3 | 3252 |

## small_string48, 100 entries, default hashing

| Map | Hit | Miss | Iterate | Copy | Erase+insert | Build | Presized build | Heap B |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| roo | 2398.1 | 1797.6 | 198.4 | 847.8 | 5277.7 | 6507.3 | 2872.7 | 14896 |
| absl_flat | 4954.5 | 4609.1 | 134.6 | 8656.3 | 16975.7 | 16339.9 | 10870.6 | 7468 |
| std_unordered | 2328.6 | 1752.7 | 29.7 | 32223.4 | 20638.6 | 20437.4 | 19321.0 | 8732 |
| absl_node | 5026.6 | 4470.6 | 132.1 | 40654.5 | 31735.5 | 31967.5 | 26515.1 | 8076 |
| std_map | 2988.8 | 2375.5 | 250.8 | 31964.0 | 25577.4 | 21118.9 | 21118.9 | 8976 |
| tsl_robin | 2272.9 | 1780.2 | 207.2 | 716.1 | 5358.8 | 6929.3 | 3078.4 | 15936 |
| unordered_dense | 16610.9 | 4065.6 | 42.2 | 976.0 | 15202.3 | 9863.0 | 6141.6 | 8252 |
| etl | 2791.5 | 2359.3 | 95.0 | 2599.3 | 4792.8 | — | 3247.1 | 6948 |
| boost_flat | 2766.4 | 2037.9 | 183.4 | 696.9 | 9046.5 | 4789.6 | 2751.9 | 6956 |
| emhash5 | 2284.9 | 1901.9 | 111.2 | 778.6 | 5684.2 | 7164.0 | 3719.5 | 7988 |

## Lookup speed ratios, 16 entries through each fixture limit (at most 512)

Geometric mean of competitor/Roo time. Above 1 means Roo is faster; this is a fixed fixture, not a confidence interval or universal ranking.

| Hash | Key | Workload | absl_flat / Roo | std_unordered / Roo | absl_node / Roo | tsl_robin / Roo | unordered_dense / Roo | etl / Roo | boost_flat / Roo | emhash5 / Roo |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| default | small_string12 | find_hit | 1.953x | 1.107x | 1.965x | 1.111x | 2.545x | 1.086x | 1.300x | 1.030x |
| default | small_string12 | find_miss | 2.297x | 1.141x | 2.254x | 1.224x | 2.975x | 1.235x | 1.223x | 1.153x |
| default | small_string48 | find_hit | 2.044x | 0.962x | 2.063x | 0.982x | 6.872x | 1.154x | 1.141x | 0.939x |
| default | small_string48 | find_miss | 2.363x | 0.945x | 2.365x | 0.997x | 2.190x | 1.267x | 1.098x | 0.960x |
| absl_hash | small_string12 | find_hit | 1.221x | 1.069x | 1.224x | 1.053x | 1.568x | 1.045x | 1.105x | 1.003x |
| absl_hash | small_string12 | find_miss | 1.189x | 1.032x | 1.192x | 1.036x | 1.565x | 1.073x | 1.005x | 1.005x |
| absl_hash | small_string48 | find_hit | 1.075x | 1.021x | 1.084x | 1.013x | 1.203x | 1.013x | 1.036x | 0.997x |
| absl_hash | small_string48 | find_miss | 1.070x | 1.011x | 1.062x | 1.021x | 1.196x | 1.025x | 0.998x | 1.000x |

ETL uses exact fixed capacity N with N buckets. Its embedded storage and adapter are included in heap/object bytes. Dynamic growth is unsupported and omitted; presized construction includes allocating the entire fixed-capacity object.

Only the recorded board, toolchain and one deterministic seed are represented. The suite does not measure immutable owners, read-only base interfaces, heterogeneous lookup, latency tails, fragmentation after arbitrary lifetimes, peak heap or isolated firmware size. Maximum fixture sizes are constrained by contiguous memory needed for copies.
