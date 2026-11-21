# ESP32 hash map results

[Full shared-timer capture and metadata](shared-timer.tar.gz) · [Chart regeneration instructions](../README.md#regenerate-without-hardware)

Benchmark suite: `general`; map profile: `extended`.

Validated one complete suite, 11553 rows and 1–1 observations per case. Each observation is the median of seven timed samples. Between-suite variation is unavailable with one suite.

Measured Roo Collections source: commit `240cff048f5c2cdfe6a70f2987b4c0ed4bb86c2f`. Source hashes in build_metadata.json identify the measured working tree.

## Measurement

Mutable maps; warm repeated access; deterministic keys and visit order. Lookup/iteration are ns per entry, erase+insert is ns per pair, copy/build include destruction, and cache churn is ns per 90 hits + 10 misses + one eviction + one insertion. Heap is retained internal 8-bit memory including the map object and owned keys, not peak memory.

Build metadata: `build_metadata.json`. Raw logs are preserved; summary.csv contains medians and ranges, and measurements.csv contains each validated observation. Default policies are not normalized.

Transport separators recovered: 0; excluded records: 0. Payload fields are never repaired. See excluded_records.json for coverage and exclusions.

## Device records

```text
esp32.log: BEGIN,1,chip=ESP32-D0WD-V3,revision=301,cpu_mhz=240,core=1,heap=324876,psram=0,compiler=14.2.0,absl=20250512.1,group_width=8,suite=general,profile=extended
esp32.log: END,1,elapsed_ms=813125,sink=720,heap=324876
```

## u32_random, 100 entries, default hashing

| Map | Hit | Miss | Iterate | Copy | Erase+insert | Build | Presized build | Heap B |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| roo | 221.3 | 199.3 | 194.6 | 302.1 | 1163.5 | 1981.2 | 772.2 | 2352 |
| absl_flat | 718.5 | 819.2 | 129.9 | 956.5 | 4213.4 | 3461.8 | 2136.8 | 1260 |
| std_unordered | 320.8 | 320.3 | 29.7 | 14291.5 | 11513.9 | 13163.0 | 12955.8 | 3416 |
| absl_node | 722.0 | 828.0 | 120.2 | 15433.8 | 14389.0 | 14862.0 | 14549.7 | 3132 |
| std_map | 615.1 | 578.2 | 252.3 | 14282.9 | 14061.2 | 13918.0 | 13918.0 | 4036 |
| tsl_robin | 170.3 | 130.6 | 202.2 | 270.6 | 743.7 | 2144.0 | 961.5 | 3264 |
| unordered_dense | 1245.5 | 1221.5 | 42.2 | 513.0 | 4956.2 | 4122.4 | 2569.9 | 1884 |
| etl | 207.9 | 187.8 | 93.1 | 790.6 | 896.0 | — | 892.4 | 2212 |
| boost_flat | 456.0 | 383.2 | 184.3 | 186.1 | 3241.4 | 2271.4 | 1375.1 | 1196 |
| emhash5 | 158.3 | 128.8 | 107.7 | 156.7 | 688.0 | 1771.5 | 834.8 | 1652 |

## u32_sequential, 100 entries, default hashing

| Map | Hit | Miss | Iterate | Copy | Erase+insert | Build | Presized build | Heap B |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| roo | 193.6 | 218.1 | 196.2 | 302.1 | 1037.7 | 1898.0 | 645.1 | 2352 |
| absl_flat | 703.4 | 799.7 | 133.1 | 939.3 | 3970.1 | 3433.1 | 2129.6 | 1260 |
| std_unordered | 239.3 | 267.0 | 29.7 | 14253.1 | 11260.1 | 13059.5 | 12901.9 | 3416 |
| absl_node | 721.2 | 867.3 | 131.7 | 15412.2 | 14150.0 | 14850.3 | 14547.2 | 3132 |
| std_map | 612.8 | 619.5 | 251.4 | 14286.9 | 14036.1 | 13886.0 | 13886.0 | 4036 |
| tsl_robin | 147.2 | 99.4 | 202.2 | 270.6 | 611.4 | 2093.6 | 934.8 | 3264 |
| unordered_dense | 1186.2 | 1160.7 | 42.2 | 513.0 | 4599.1 | 4064.4 | 2515.7 | 1884 |
| etl | 138.9 | 168.5 | 109.0 | 703.3 | 646.1 | — | 803.0 | 2212 |
| boost_flat | 444.4 | 363.3 | 182.4 | 186.1 | 1657.1 | 2264.9 | 1363.4 | 1196 |
| emhash5 | 138.9 | 108.0 | 107.4 | 156.7 | 565.0 | 1701.8 | 758.7 | 1652 |

## u64_random, 100 entries, default hashing

| Map | Hit | Miss | Iterate | Copy | Erase+insert | Build | Presized build | Heap B |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| roo | 271.2 | 240.1 | 198.7 | 376.5 | 1334.0 | 2251.6 | 844.7 | 4400 |
| absl_flat | 713.2 | 770.7 | 126.0 | 1033.5 | 4060.6 | 4587.7 | 2149.4 | 2348 |
| std_unordered | 337.2 | 330.7 | 29.7 | 14421.6 | 13224.1 | 14764.9 | 14387.3 | 4624 |
| absl_node | 748.8 | 815.3 | 137.7 | 15461.0 | 15987.6 | 16476.4 | 16009.3 | 3940 |
| std_map | 599.0 | 520.3 | 252.3 | 14301.8 | 15301.3 | 14891.6 | 14891.6 | 4840 |
| tsl_robin | 191.2 | 160.7 | 200.9 | 287.8 | 856.6 | 2318.2 | 833.2 | 6464 |
| unordered_dense | 1346.7 | 1309.7 | 42.2 | 595.8 | 4983.4 | 4394.5 | 2747.0 | 3004 |
| etl | 995.2 | 555.1 | 95.0 | 1174.5 | 1633.4 | — | 1284.8 | 3364 |
| boost_flat | 500.7 | 427.3 | 183.3 | 211.8 | 2402.1 | 2499.5 | 1524.2 | 2220 |
| emhash5 | 163.6 | 124.2 | 102.8 | 203.3 | 718.3 | 1851.2 | 908.4 | 2228 |

## string12, 100 entries, default hashing

| Map | Hit | Miss | Iterate | Copy | Erase+insert | Build | Presized build | Heap B |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| roo | 1028.2 | 782.5 | 194.4 | 1077.0 | 3638.2 | 5032.9 | 2157.4 | 7472 |
| absl_flat | 2103.8 | 1773.1 | 150.7 | 3522.4 | 8741.2 | 8292.3 | 5554.8 | 3756 |
| std_unordered | 1157.8 | 886.5 | 29.7 | 17624.0 | 16253.2 | 17547.3 | 17023.8 | 5792 |
| absl_node | 2129.9 | 1968.8 | 137.9 | 20020.0 | 21975.4 | 21898.1 | 19675.0 | 5132 |
| std_map | 2483.5 | 2473.0 | 250.8 | 17272.4 | 22233.1 | 18606.1 | 18606.1 | 6032 |
| tsl_robin | 1079.6 | 891.7 | 201.9 | 937.9 | 3514.8 | 5242.5 | 2482.4 | 8768 |
| unordered_dense | 5170.4 | 2322.0 | 42.2 | 956.9 | 9321.0 | 7132.1 | 4463.1 | 4636 |
| etl | 1149.6 | 950.4 | 94.6 | 1964.4 | 3483.7 | — | 2736.7 | 4132 |
| boost_flat | 1324.6 | 944.1 | 183.8 | 724.3 | 11353.1 | 3983.1 | 2927.2 | 3628 |
| emhash5 | 1112.8 | 1028.3 | 103.6 | 722.7 | 3360.0 | 4572.9 | 2482.0 | 4660 |

## string48, 100 entries, default hashing

| Map | Hit | Miss | Iterate | Copy | Erase+insert | Build | Presized build | Heap B |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| roo | 2378.4 | 1834.6 | 194.2 | 33647.5 | 36817.9 | 39782.3 | 35440.5 | 14272 |
| absl_flat | 5044.7 | 4736.6 | 134.6 | 41655.0 | 48814.9 | 49987.6 | 43820.9 | 10556 |
| std_unordered | 2328.7 | 1752.7 | 29.7 | 64829.8 | 67847.9 | 68652.7 | 67424.2 | 12632 |
| absl_node | 5034.8 | 4498.8 | 137.9 | 73038.5 | 79666.2 | 80629.3 | 74963.0 | 11920 |
| std_map | 3086.6 | 2473.2 | 250.8 | 64418.5 | 72299.4 | 68172.2 | 68172.2 | 12836 |
| tsl_robin | 2260.8 | 1745.9 | 198.9 | 33584.2 | 36582.3 | 40299.9 | 35639.8 | 15608 |
| unordered_dense | 7178.4 | 4060.6 | 42.2 | 33952.2 | 46230.3 | 43466.5 | 38894.7 | 11312 |
| etl | 2794.5 | 2351.3 | 95.0 | 35455.1 | 65421.8 | — | 64591.8 | 10936 |
| boost_flat | 2508.5 | 1770.3 | 183.4 | 33524.5 | 39543.4 | 37967.8 | 36119.2 | 10436 |
| emhash5 | 2260.9 | 1887.1 | 102.8 | 33556.0 | 36827.0 | 40213.5 | 36278.5 | 11228 |

## Lookup speed ratios, 16 entries through each fixture limit (at most 512)

Geometric mean of competitor/Roo time. Above 1 means Roo is faster; this is a fixed fixture, not a confidence interval or universal ranking.

| Hash | Key | Workload | absl_flat / Roo | std_unordered / Roo | absl_node / Roo | tsl_robin / Roo | unordered_dense / Roo | etl / Roo | boost_flat / Roo | emhash5 / Roo |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| default | u32_random | find_hit | 3.239x | 1.459x | 3.275x | 0.805x | 5.445x | 0.925x | 2.044x | 0.696x |
| default | u32_random | find_miss | 3.055x | 1.376x | 3.057x | 0.597x | 4.991x | 0.760x | 1.561x | 0.487x |
| default | u32_sequential | find_hit | 3.591x | 1.209x | 3.599x | 0.746x | 5.980x | 0.704x | 2.249x | 0.704x |
| default | u32_sequential | find_miss | 3.334x | 1.231x | 3.221x | 0.561x | 5.367x | 0.777x | 1.672x | 0.479x |
| default | u64_random | find_hit | 2.563x | 1.180x | 2.611x | 0.724x | 4.877x | 3.615x | 1.812x | 0.585x |
| default | u64_random | find_miss | 2.417x | 1.134x | 2.543x | 0.551x | 4.597x | 1.920x | 1.496x | 0.392x |
| default | string12 | find_hit | 2.011x | 1.222x | 2.036x | 1.115x | 4.884x | 1.108x | 1.252x | 1.026x |
| default | string12 | find_miss | 2.154x | 1.292x | 2.160x | 1.122x | 2.767x | 1.163x | 1.069x | 1.073x |
| default | string48 | find_hit | 2.091x | 0.999x | 2.096x | 0.984x | 2.959x | 1.168x | 1.045x | 0.939x |
| default | string48 | find_miss | 2.364x | 1.042x | 2.336x | 0.957x | 2.154x | 1.245x | 0.939x | 0.940x |
| absl_hash | u32_random | find_hit | 2.608x | 1.608x | 2.608x | 0.827x | 4.612x | 0.814x | 1.737x | 0.696x |
| absl_hash | u32_random | find_miss | 2.735x | 1.551x | 2.666x | 0.758x | 4.659x | 0.786x | 1.560x | 0.537x |
| absl_hash | u32_sequential | find_hit | 2.551x | 1.577x | 2.574x | 0.778x | 4.578x | 0.807x | 1.727x | 0.690x |
| absl_hash | u32_sequential | find_miss | 2.705x | 1.630x | 2.675x | 0.732x | 4.765x | 0.829x | 1.600x | 0.546x |
| absl_hash | u64_random | find_hit | 2.553x | 1.695x | 2.592x | 0.915x | 4.694x | 0.934x | 1.875x | 0.788x |
| absl_hash | u64_random | find_miss | 2.590x | 1.596x | 2.626x | 0.762x | 4.591x | 0.869x | 1.552x | 0.521x |
| absl_hash | string12 | find_hit | 1.249x | 1.032x | 1.273x | 1.063x | 1.562x | 1.029x | 1.126x | 1.003x |
| absl_hash | string12 | find_miss | 1.193x | 0.977x | 1.197x | 1.007x | 1.531x | 1.067x | 0.964x | 0.977x |
| absl_hash | string48 | find_hit | 1.089x | 1.009x | 1.093x | 1.015x | 1.200x | 1.005x | 1.043x | 0.998x |
| absl_hash | string48 | find_miss | 1.081x | 0.995x | 1.073x | 1.007x | 1.189x | 1.025x | 0.988x | 0.993x |

ETL uses exact fixed capacity N with N buckets. Its embedded storage and adapter are included in heap/object bytes. Dynamic growth is unsupported and omitted; presized construction includes allocating the entire fixed-capacity object.

Only the recorded board, toolchain and one deterministic seed are represented. The suite does not measure immutable owners, read-only base interfaces, heterogeneous lookup, latency tails, fragmentation after arbitrary lifetimes, peak heap or isolated firmware size. Maximum fixture sizes are constrained by contiguous memory needed for copies.
