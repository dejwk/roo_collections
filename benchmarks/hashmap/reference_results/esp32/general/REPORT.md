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
| absl_flat | 2.455x | 2.666x | 1.271x | 2.314x |
| std_unordered | 1.065x | 1.162x | 0.196x | 8.685x |
| absl_node | 2.523x | 2.723x | 1.284x | 8.708x |
| std_map | 1.824x | 1.943x | 1.540x | 7.837x |
| tsl_robin | 0.833x | 0.713x | 1.125x | 0.715x |
| unordered_dense | 3.539x | 3.580x | 0.196x | 2.734x |
| etl | 0.888x | 0.899x | 0.753x | 0.814x |
| boost_flat | 1.472x | 1.253x | 1.055x | 1.234x |
| emhash5 | 0.684x | 0.611x | 0.753x | 0.571x |

## absl_hash: competitor / current Roo

| Container | Hit | Miss | Iteration | Erase + insert |
|---|---:|---:|---:|---:|
| absl_flat | 2.113x | 2.184x | 1.198x | 2.157x |
| std_unordered | 1.505x | 1.493x | 0.180x | 8.642x |
| absl_node | 2.155x | 2.202x | 1.197x | 7.388x |
| std_map | 1.589x | 1.610x | 1.417x | 7.412x |
| tsl_robin | 1.061x | 0.948x | 1.036x | 0.818x |
| unordered_dense | 3.122x | 2.992x | 0.181x | 3.650x |
| etl | 1.010x | 0.971x | 0.680x | 0.985x |
| boost_flat | 1.605x | 1.428x | 0.999x | 1.334x |
| emhash5 | 0.853x | 0.756x | 0.697x | 0.690x |

## Roo before / after speedup

Above 1 means the new capture is faster; unchanged competitors were not remeasured.

| Key | Hash | Hit | Miss | Iteration | Copy | Erase + insert | Build | Presized build | Cache cycle |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| u32_random | default | 1.381x | 1.151x | 1.000x | 1.000x | 1.050x | 1.049x | 1.062x | — |
| u32_random | absl_hash | 1.415x | 1.185x | 1.029x | 0.999x | 1.059x | 1.051x | 1.053x | — |
| u32_sequential | default | 1.382x | 1.151x | 1.000x | 1.000x | 1.048x | 1.048x | 1.064x | — |
| u32_sequential | absl_hash | 1.421x | 1.263x | 1.029x | 0.999x | 1.067x | 1.049x | 1.037x | — |
| u64_random | default | 1.504x | 1.085x | 1.000x | 1.020x | 1.068x | 1.041x | 1.050x | — |
| u64_random | absl_hash | 1.479x | 1.131x | 0.998x | 1.018x | 1.079x | 1.034x | 1.039x | — |
| string12 | default | 1.027x | 1.033x | 1.029x | 1.017x | 1.040x | 1.022x | 1.028x | 1.173x |
| string12 | absl_hash | 1.096x | 1.033x | 0.793x | 1.032x | 1.029x | 1.013x | 1.013x | 1.049x |
| string48 | default | 1.013x | 1.014x | 1.029x | 1.001x | 1.006x | 1.003x | 1.002x | 1.036x |
| string48 | absl_hash | 1.033x | 1.010x | 0.794x | 1.002x | 1.004x | 1.001x | 1.000x | 1.017x |
