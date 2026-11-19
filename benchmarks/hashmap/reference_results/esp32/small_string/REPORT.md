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
| absl_flat | 2.827x | 3.106x | 1.282x | 3.238x |
| std_unordered | 1.620x | 1.477x | 0.195x | 4.721x |
| absl_node | 2.828x | 3.118x | 1.271x | 9.295x |
| std_map | 1.778x | 1.034x | 1.478x | 5.220x |
| tsl_robin | 1.579x | 1.361x | 1.147x | 1.836x |
| unordered_dense | 3.018x | 3.320x | 0.195x | 3.288x |
| etl | 1.393x | 1.186x | 0.719x | 1.369x |
| boost_flat | 1.593x | 1.332x | 1.055x | 1.708x |
| emhash5 | 1.508x | 1.307x | 0.767x | 1.397x |

## absl_hash: competitor / current Roo

| Container | Hit | Miss | Iteration | Erase + insert |
|---|---:|---:|---:|---:|
| absl_flat | 1.505x | 1.425x | 1.341x | 2.024x |
| std_unordered | 1.376x | 1.290x | 0.198x | 3.410x |
| absl_node | 1.508x | 1.437x | 1.300x | 5.856x |
| std_map | 0.948x | 0.478x | 1.501x | 3.280x |
| tsl_robin | 1.342x | 1.220x | 1.169x | 1.520x |
| unordered_dense | 1.691x | 1.590x | 0.198x | 2.264x |
| etl | 1.335x | 1.239x | 0.729x | 1.306x |
| boost_flat | 1.389x | 1.272x | 1.062x | 1.455x |
| emhash5 | 1.299x | 1.184x | 0.792x | 1.345x |

## Roo before / after speedup

Above 1 means the new capture is faster; unchanged competitors were not remeasured.

| Key | Hash | Hit | Miss | Iteration | Copy | Erase + insert | Build | Presized build | Cache cycle |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| small_string12 | default | 1.502x | 1.306x | 0.972x | 1.469x | 1.456x | 1.302x | 1.356x | 1.410x |
| small_string12 | absl_hash | 1.355x | 1.271x | 0.997x | 1.474x | 1.360x | 1.304x | 1.343x | 1.343x |
| small_string48 | default | 1.613x | 1.333x | 1.000x | 1.554x | 1.586x | 1.381x | 1.427x | 1.499x |
| small_string48 | absl_hash | 1.394x | 1.250x | 1.028x | 1.553x | 1.354x | 1.283x | 1.304x | 1.370x |
