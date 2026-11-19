# ESP32 comparison with refreshed Roo results

The source directories and CSVs below are produced by the [regeneration instructions](../README.md#regenerate-without-hardware); raw inputs are distributed as compact bundles.

Roo: 2026-11-19, 3 runs/case; other containers: 2026-11-10, 1 run/case, retained. Separate firmware captures.

This is a composition of separate captures, not a new full-container run. The original competitor observations are retained unchanged. The new firmware retains all competitor instantiations and selects Roo at runtime. Code layout, allocation history and hash seed can differ; before/after ratios do not isolate algorithm changes. No between-run confidence intervals are claimed.

Each Roo point is the median of its run medians; each run has seven timed samples. Lookup/iteration are ns per entry; erase+insert is ns per pair; build/copy include destruction; cache_cycle is a whole 102-call cycle. Heap is retained memory, not peak.

Raw logs, original metadata and coverage reports are preserved under `sources/previous/` and `sources/roo/`. `refresh_manifest.json` identifies the input bundles. `measurements.csv` labels every row's capture source. `summary.csv` contains every case, including tiny maps.

Ratios below are geometric means, with equal weight for every key/size case with at least 16 entries through each fixture limit. Above 1 means Roo is faster.


## default: competitor / current Roo

| Container | Hit | Miss | Iteration | Erase + insert |
|---|---:|---:|---:|---:|
| absl_flat | 2.775x | 2.987x | 1.270x | 2.444x |
| std_unordered | 1.203x | 1.302x | 0.196x | 9.172x |
| absl_node | 2.851x | 3.052x | 1.283x | 9.196x |
| std_map | 2.061x | 2.178x | 1.539x | 8.276x |
| tsl_robin | 0.941x | 0.799x | 1.124x | 0.755x |
| unordered_dense | 3.999x | 4.012x | 0.196x | 2.888x |
| etl | 1.003x | 1.008x | 0.752x | 0.859x |
| boost_flat | 1.664x | 1.404x | 1.055x | 1.303x |
| emhash5 | 0.773x | 0.684x | 0.753x | 0.603x |

## absl_hash: competitor / current Roo

| Container | Hit | Miss | Iteration | Erase + insert |
|---|---:|---:|---:|---:|
| absl_flat | 2.105x | 2.188x | 1.187x | 2.174x |
| std_unordered | 1.500x | 1.495x | 0.179x | 8.708x |
| absl_node | 2.147x | 2.206x | 1.186x | 7.444x |
| std_map | 1.583x | 1.613x | 1.404x | 7.468x |
| tsl_robin | 1.057x | 0.950x | 1.027x | 0.824x |
| unordered_dense | 3.111x | 2.997x | 0.179x | 3.678x |
| etl | 1.006x | 0.973x | 0.673x | 0.993x |
| boost_flat | 1.599x | 1.431x | 0.990x | 1.344x |
| emhash5 | 0.850x | 0.757x | 0.691x | 0.695x |

## Roo before / after speedup

Above 1 means the new capture is faster; unchanged competitors were not remeasured.

| Key | Hash | Hit | Miss | Iteration | Copy | Erase + insert | Build | Presized build | Cache cycle |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| u32_random | default | 1.776x | 1.469x | 0.998x | 1.000x | 1.141x | 1.083x | 1.094x | — |
| u32_random | absl_hash | 1.415x | 1.185x | 1.027x | 0.999x | 1.072x | 1.048x | 1.048x | — |
| u32_sequential | default | 1.874x | 1.512x | 0.999x | 1.000x | 1.227x | 1.133x | 1.205x | — |
| u32_sequential | absl_hash | 1.421x | 1.263x | 1.027x | 0.999x | 1.080x | 1.046x | 1.032x | — |
| u64_random | default | 1.504x | 1.085x | 1.000x | 1.021x | 1.074x | 1.043x | 1.052x | — |
| u64_random | absl_hash | 1.479x | 1.131x | 0.996x | 1.019x | 1.087x | 1.036x | 1.041x | — |
| string12 | default | 1.027x | 1.033x | 1.029x | 1.006x | 1.044x | 0.994x | 1.029x | 1.172x |
| string12 | absl_hash | 1.079x | 1.041x | 0.776x | 1.033x | 1.033x | 0.969x | 1.023x | 1.047x |
| string48 | default | 1.013x | 1.014x | 1.029x | 1.001x | 1.006x | 0.996x | 1.002x | 1.035x |
| string48 | absl_hash | 1.028x | 1.013x | 0.777x | 1.001x | 1.005x | 0.991x | 1.002x | 1.015x |
