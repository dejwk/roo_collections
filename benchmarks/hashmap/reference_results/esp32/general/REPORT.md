# ESP32 comparison with refreshed Roo results

The source directories and CSVs below are produced by the [regeneration instructions](../README.md#regenerate-without-hardware); raw inputs are distributed as compact bundles.

Roo: 2026-11-20, 3 runs/case; other containers: 2026-11-10, 1 run/case, retained. Separate firmware captures.

This is a composition of separate captures, not a new full-container run. The original competitor observations are retained unchanged. The new firmware retains all competitor instantiations and selects Roo at runtime. Code layout, allocation history and hash seed can differ; before/after ratios do not isolate algorithm changes. No between-run confidence intervals are claimed.

Each Roo point is the median of its run medians; each run has seven timed samples. Lookup/iteration are ns per entry; erase+insert is ns per pair; build/copy include destruction; cache_cycle is a whole 102-call cycle. Heap is retained memory, not peak.

Raw logs, original metadata and coverage reports are preserved under `sources/previous/` and `sources/roo/`. `refresh_manifest.json` identifies the input bundles. `measurements.csv` labels every row's capture source. `summary.csv` contains every case, including tiny maps.

Ratios below are geometric means, with equal weight for every key/size case with at least 16 entries through each fixture limit. Above 1 means Roo is faster.


## default: competitor / current Roo

| Container | Hit | Miss | Iteration | Erase + insert |
|---|---:|---:|---:|---:|
| absl_flat | 2.867x | 3.128x | 1.271x | 2.483x |
| std_unordered | 1.243x | 1.363x | 0.196x | 9.317x |
| absl_node | 2.946x | 3.195x | 1.284x | 9.341x |
| std_map | 2.129x | 2.280x | 1.539x | 8.407x |
| tsl_robin | 0.972x | 0.837x | 1.125x | 0.767x |
| unordered_dense | 4.131x | 4.201x | 0.196x | 2.933x |
| etl | 1.036x | 1.055x | 0.753x | 0.873x |
| boost_flat | 1.719x | 1.470x | 1.055x | 1.324x |
| emhash5 | 0.799x | 0.717x | 0.753x | 0.612x |

## absl_hash: competitor / current Roo

| Container | Hit | Miss | Iteration | Erase + insert |
|---|---:|---:|---:|---:|
| absl_flat | 2.105x | 2.170x | 1.198x | 1.890x |
| std_unordered | 1.500x | 1.483x | 0.180x | 7.573x |
| absl_node | 2.147x | 2.189x | 1.197x | 6.474x |
| std_map | 1.583x | 1.600x | 1.417x | 6.495x |
| tsl_robin | 1.057x | 0.942x | 1.036x | 0.717x |
| unordered_dense | 3.111x | 2.973x | 0.181x | 3.198x |
| etl | 1.006x | 0.965x | 0.680x | 0.864x |
| boost_flat | 1.599x | 1.419x | 0.999x | 1.169x |
| emhash5 | 0.850x | 0.751x | 0.697x | 0.604x |

## Roo before / after speedup

Above 1 means the new capture is faster; unchanged competitors were not remeasured.

| Key | Hash | Hit | Miss | Iteration | Copy | Erase + insert | Build | Presized build | Cache cycle |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| u32_random | default | 1.776x | 1.469x | 0.998x | 1.477x | 1.145x | 1.508x | 1.237x | — |
| u32_random | absl_hash | 1.415x | 1.164x | 1.029x | 1.476x | 1.072x | 1.446x | 1.178x | — |
| u32_sequential | default | 1.874x | 1.512x | 0.999x | 1.477x | 1.232x | 1.579x | 1.378x | — |
| u32_sequential | absl_hash | 1.421x | 1.241x | 1.029x | 1.476x | 1.080x | 1.441x | 1.160x | — |
| u64_random | default | 1.771x | 1.387x | 1.003x | 1.499x | 1.153x | 1.501x | 1.265x | — |
| u64_random | absl_hash | 1.479x | 1.131x | 0.998x | 1.498x | 0.542x | 1.437x | 1.180x | — |
| string12 | default | 1.027x | 1.021x | 1.029x | 1.222x | 1.044x | 1.376x | 1.097x | 1.181x |
| string12 | absl_hash | 1.079x | 1.041x | 0.793x | 1.174x | 1.033x | 1.306x | 1.089x | 1.044x |
| string48 | default | 1.013x | 1.008x | 1.029x | 1.027x | 1.005x | 1.066x | 1.013x | 1.038x |
| string48 | absl_hash | 1.028x | 1.013x | 0.794x | 1.024x | 1.003x | 1.057x | 1.012x | 1.014x |
