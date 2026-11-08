"""Compare complete benchmark summaries while retaining unchanged-map controls."""
import argparse
import csv
import json
import math
from pathlib import Path
import statistics

GROUP = ("map", "hash", "key", "n", "workload")
KEYS = ("u32_random", "u32_sequential", "u64_random", "string12", "string48")
WORKS = ("find_hit", "find_miss", "iteration", "copy_ctor", "erase_insert",
         "build_grow", "build_presized")
SIZES = (16, 32, 54, 64, 100, 128, 256, 400, 512)


def load(directory):
    with (directory / "summary.csv").open() as stream:
        rows = list(csv.DictReader(stream))
    data = {tuple(row[field] for field in GROUP): row for row in rows}
    if len(data) != len(rows):
        raise ValueError("Duplicate summary cases")
    return data, json.loads((directory / "build_metadata.json").read_text())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--updated", type=Path, required=True)
    args = parser.parse_args()
    before, old_build = load(args.baseline)
    after, new_build = load(args.updated)
    if before.keys() != after.keys():
        raise ValueError("The summaries cover different cases")
    for field in ("flags", "compiler", "arduino", "platform", "cpu_mhz", "psram", "abseil"):
        if old_build[field] != new_build[field]:
            raise ValueError(f"Configuration changed: {field}")
    from common import normalize_sources
    old_build["source_sha256"] = normalize_sources(old_build["source_sha256"])
    new_build["source_sha256"] = normalize_sources(new_build["source_sha256"])
    for field in ("dependency_content_sha256", "heap_poisoning", "compiler_options"):
        if field in old_build and field in new_build and old_build[field] != new_build[field]:
            raise ValueError(f"Configuration changed: {field}")
    harness = "benchmarks/hashmap/src/main.cpp"
    if old_build["source_sha256"][harness] != new_build["source_sha256"][harness]:
        raise ValueError("Benchmark firmware source changed")
    for name, digest in old_build["source_sha256"].items():
        if not name.startswith("lib/roo_collections/") and new_build["source_sha256"].get(name) != digest:
            raise ValueError(f"Non-Roo source changed: {name}")

    comparisons = {}
    for case in sorted(before):
        old = float(before[case]["median_ns"])
        new = float(after[case]["median_ns"])
        row = dict(zip(GROUP, case))
        row.update(baseline_ns=old, updated_ns=new, change_percent=100 * (new / old - 1),
                   speedup=old / new, baseline_heap_bytes=float(before[case]["heap_bytes"]),
                   updated_heap_bytes=float(after[case]["heap_bytes"]),
                   baseline_runs=int(before[case]["runs"]), updated_runs=int(after[case]["runs"]))
        comparisons[case] = row
    with (args.updated / "comparison.csv").open("w") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(next(iter(comparisons.values()))))
        writer.writeheader()
        writer.writerows(comparisons.values())

    def case(name, hashing, key, n, work):
        return comparisons[(name, hashing, key, str(n), work)]

    def delta(name, hashing, keys, work):
        ratios = [case(name, hashing, key, n, work)["updated_ns"] /
                  case(name, hashing, key, n, work)["baseline_ns"]
                  for key in keys for n in SIZES]
        return 100 * (math.exp(statistics.mean(math.log(r) for r in ratios)) - 1)

    def percent(change):
        return "0.0%" if abs(change) < 0.05 else f"{change:+.1f}%"

    old_commit = old_build["roo_collections_commit"]
    new_commit = new_build["roo_collections_commit"]
    old_runs = min(int(r["runs"]) for r in before.values())
    new_runs = min(int(r["runs"]) for r in after.values())
    text = ["# Updated Roo benchmark versus the previous baseline", "",
            f"Baseline `{old_commit}` versus updated `{new_commit}`, {len(before)} matching cases. "
            f"Minimum per-case observation counts are {old_runs} and {new_runs}, respectively. "
            "The benchmark C++ source, non-Roo recorded sources, optimization flags, "
            "SDK, clock, key fixtures and workload coverage match. Source hashes identify the measured code.", "",
            "This is the existing mutable-map suite: lookup calls non-const find. "
            "It does not instantiate immutable owners or exercise references to "
            "the common read-only interface. Changes in mutable-map internals are "
            "included in the measurement. A dirty source status in build_metadata.json "
            "means the recorded commit is the snapshot's HEAD base; hashes identify "
            "the exact measured sources.", "",
            "All times compare the median of each suite's seven-sample median. "
            "Negative change means faster; speedup above 1 means faster. These "
            "use the recorded device configurations and one deterministic data seed. "
            "The SDK enables light heap poisoning. Unchanged-map controls below "
            "show layout/hash-seed/run variation; changes are not automatically "
            "attributable to source algorithms.", "",
            "Transport exclusions and per-suite coverage are documented in REPORT.md "
            "and excluded_records.json. Each case's actual observation count is "
            "retained in summary.csv.", "",
            "For Roo's default hash across sizes 16–512, random uint32_t hit/miss "
            f"time changes are {percent(delta('roo', 'default', ('u32_random',), 'find_hit'))} / "
            f"{percent(delta('roo', 'default', ('u32_random',), 'find_miss'))}; "
            "sequential uint32_t changes are "
            f"{percent(delta('roo', 'default', ('u32_sequential',), 'find_hit'))} / "
            f"{percent(delta('roo', 'default', ('u32_sequential',), 'find_miss'))}; "
            "random uint64_t changes are "
            f"{percent(delta('roo', 'default', ('u64_random',), 'find_hit'))} / "
            f"{percent(delta('roo', 'default', ('u64_random',), 'find_miss'))}. "
            "These are geometric means across the listed sizes, rather than "
            "a mix of hits and misses from an application.", "",
            "For Roo's default hash across sizes 16–512, 12-byte string hit/miss "
            f"time changes are {percent(delta('roo', 'default', ('string12',), 'find_hit'))} / "
            f"{percent(delta('roo', 'default', ('string12',), 'find_miss'))}; "
            "48-byte string changes are "
            f"{percent(delta('roo', 'default', ('string48',), 'find_hit'))} / "
            f"{percent(delta('roo', 'default', ('string48',), 'find_miss'))}. "
            "Use the shared-hash rows and unchanged-map controls to assess how "
            "much of the difference is specific to the default string hasher.", "",
            "## Selected Roo cases", "",
            "| Key | Hash | N | Operation | Before us | After us | Change | Speedup |",
            "|---|---|---:|---|---:|---:|---:|---:|"]
    if old_runs == 1 or new_runs == 1:
        text[4:4] = ["One measurement uses a single suite, with seven timed samples "
                     "per case. Small differences need caution because its "
                     "between-suite variation is not measured.", ""]
    selected = [(key, "default", 128, work) for key in KEYS
                for work in ("find_hit", "find_miss")]
    selected = [(key, "default", n, "erase_insert") for key in KEYS for n in (16, 32)] + selected
    selected = [(key, "default", n, "cache_cycle") for key in ("string12", "string48")
                for n in (16, 32)] + selected
    selected += [(key, hashing, 100, work) for key in ("string12", "string48")
                 for hashing in ("default", "absl_hash")
                 for work in ("erase_insert", "build_grow")]
    selected += [("u32_sequential", hashing, 512, work)
                 for hashing in ("default", "absl_hash")
                 for work in ("find_hit", "find_miss")]
    selected += [(key, hashing, 100, "cache_cycle")
                 for key in ("string12", "string48") for hashing in ("default", "absl_hash")]
    for key, hashing, n, work in selected:
        r = case("roo", hashing, key, n, work)
        text.append(f"| {key} | {hashing} | {n} | {work} | "
                    f"{r['baseline_ns']/1000:.3f} | {r['updated_ns']/1000:.3f} | "
                    f"{percent(r['change_percent'])} | {r['speedup']:.3f}x |")
    text += ["", "A cache_cycle is one whole cycle of 90 hit lookups, 10 miss "
             "lookups, one eviction and one insertion. Other per-operation units "
             "are defined in ../README.md."]
    text += ["", "## Roo changes across 16–512 entries", "",
             "Geometric mean of updated/baseline time ratios, equal weight for every "
             "listed size. Negative is faster. Each row uses one key dataset and hash.", "",
             "| Key | Hash | Hit | Miss | Iterate | Copy | Erase/insert | Build | Presized build |",
             "|---|---|---:|---:|---:|---:|---:|---:|---:|"]
    for key in KEYS:
        for hashing in ("default", "absl_hash"):
            values = [delta("roo", hashing, (key,), work) for work in WORKS]
            text.append(f"| {key} | {hashing} | " +
                        " | ".join(percent(v) for v in values) + " |")
    text += ["", "## Unchanged-map controls", "",
             "The same geometric mean over all five key datasets and sizes 16–512. "
             "These containers' source did not change, but allocations, hash seeds "
             "and generated-code layout can change between binaries.", "",
             "| Map | Hash | Hit | Miss | Iterate | Copy | Erase/insert | Build | Presized build |",
             "|---|---|---:|---:|---:|---:|---:|---:|---:|"]
    for name in ("absl_flat", "std_unordered", "absl_node", "std_map"):
        for hashing in (("ordered",) if name == "std_map" else ("default", "absl_hash")):
            values = [delta(name, hashing, KEYS, work) for work in WORKS]
            text.append(f"| {name} | {hashing} | " +
                        " | ".join(percent(v) for v in values) + " |")
    changed_heaps = [r for r in comparisons.values() if r["map"] == "roo" and
                     r["baseline_heap_bytes"] != r["updated_heap_bytes"]]
    text += ["", "## Retained heap", "",
             f"{len(changed_heaps)} Roo cases have a different measured median heap "
             "footprint. Per-case values are retained in comparison.csv; small "
             "differences can reflect allocator rounding/placement.", "",
             "Default Roo hashing, measured retained bytes. Grown maps are the initial "
             "lookup fixture; presized maps use the size-hint constructor. This is "
             "retained heap including the map object and allocator overhead, not peak memory.", "",
             "| Key | N | Grown before B | Grown after B | Presized before B | Presized after B | Presized saving |",
             "|---|---:|---:|---:|---:|---:|---:|"]
    for key in KEYS:
        for n in (100, 128, 512):
            grown = case("roo", "default", key, n, "find_hit")
            presized = case("roo", "default", key, n, "build_presized")
            saving = 100 * (1 - presized["updated_heap_bytes"] / presized["baseline_heap_bytes"])
            text.append(f"| {key} | {n} | {grown['baseline_heap_bytes']:.0f} | "
                        f"{grown['updated_heap_bytes']:.0f} | {presized['baseline_heap_bytes']:.0f} | "
                        f"{presized['updated_heap_bytes']:.0f} | {saving:.1f}% |")
    (args.updated / "COMPARISON.md").write_text("\n".join(text))
    print(f"Compared {len(comparisons)} cases; wrote {args.updated}/COMPARISON.md")


if __name__ == "__main__":
    main()
