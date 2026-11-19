# ESP32 comparison with refreshed Roo results

The source directories and CSVs below are produced by the [regeneration instructions](../README.md#regenerate-without-hardware); raw inputs are distributed as compact bundles.

Roo: 2026-11-19, 3 runs/case; other containers: 2026-11-10, 1 run/case, retained. Separate firmware captures.
Roo and retained competitors use different SmallString key layouts.

This is a composition of separate captures, not a new full-container run. The original competitor observations are retained unchanged. The new firmware retains all competitor instantiations and selects Roo at runtime. Code layout, allocation history and hash seed can differ; before/after ratios do not isolate algorithm changes. No between-run confidence intervals are claimed.

Each Roo point is the median of its run medians; each run has seven timed samples. Lookup/iteration are ns per entry; erase+insert is ns per pair; build/copy include destruction; cache_cycle is a whole 102-call cycle. Heap is retained memory, not peak.

Raw logs, original metadata and coverage reports are preserved under `sources/previous/` and `sources/roo/`. `refresh_manifest.json` identifies the input bundles. `measurements.csv` labels every row's capture source. `summary.csv` contains every case, including tiny maps.

Ratios below are geometric means, with equal weight for every key/size case with at least 16 entries through each fixture limit. Above 1 means Roo is faster.


## default: competitor / current Roo

| Container | Hit | Miss | Iteration | Erase + insert |
|---|---:|---:|---:|---:|
| absl_flat | 2.827x | 3.102x | 1.282x | 3.255x |
| std_unordered | 1.620x | 1.475x | 0.195x | 4.746x |
| absl_node | 2.828x | 3.114x | 1.271x | 9.345x |
| std_map | 1.778x | 1.033x | 1.478x | 5.248x |
| tsl_robin | 1.579x | 1.360x | 1.147x | 1.846x |
| unordered_dense | 3.018x | 3.316x | 0.195x | 3.306x |
| etl | 1.393x | 1.185x | 0.719x | 1.376x |
| boost_flat | 1.593x | 1.330x | 1.055x | 1.717x |
| emhash5 | 1.508x | 1.305x | 0.767x | 1.404x |

## absl_hash: competitor / current Roo

| Container | Hit | Miss | Iteration | Erase + insert |
|---|---:|---:|---:|---:|
| absl_flat | 1.505x | 1.425x | 1.341x | 1.768x |
| std_unordered | 1.376x | 1.290x | 0.198x | 2.979x |
| absl_node | 1.508x | 1.437x | 1.300x | 5.116x |
| std_map | 0.948x | 0.478x | 1.501x | 2.865x |
| tsl_robin | 1.342x | 1.220x | 1.169x | 1.328x |
| unordered_dense | 1.691x | 1.590x | 0.198x | 1.978x |
| etl | 1.335x | 1.239x | 0.729x | 1.141x |
| boost_flat | 1.389x | 1.272x | 1.062x | 1.271x |
| emhash5 | 1.299x | 1.184x | 0.792x | 1.175x |

## Roo before / after speedup

Above 1 means the new capture is faster; unchanged competitors were not remeasured.

| Key | Hash | Hit | Miss | Iteration | Copy | Erase + insert | Build | Presized build | Cache cycle |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| small_string12 | default | 1.502x | 1.306x | 0.972x | 1.469x | 1.465x | 1.314x | 1.369x | 1.411x |
| small_string12 | absl_hash | 1.355x | 1.271x | 0.997x | 1.473x | 1.365x | 1.304x | 1.342x | 1.343x |
| small_string48 | default | 1.613x | 1.329x | 1.000x | 1.554x | 1.592x | 1.388x | 1.343x | 1.498x |
| small_string48 | absl_hash | 1.393x | 1.250x | 1.028x | 1.554x | 1.011x | 1.281x | 1.304x | 1.370x |
