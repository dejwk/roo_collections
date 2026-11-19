"""Replace Roo rows using complete raw bundles, retaining explicit source provenance."""

import argparse
import csv
import json
from pathlib import Path
import statistics
import tempfile

import analyze
import bundle
from common import HARNESS, digest, normalize_sources
from plot import Results

STRING_ADAPTER_SOURCES = {
    "benchmarks/hashmap/src/benchmark_keys.h",
    "benchmarks/hashmap/src/benchmark_containers.h",
}


def read_rows(path):
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


def write_rows(path, rows):
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def validate_sources(previous, roo, *, allow_string_adapter_changes=False):
    """Require exact coverage, settings, and provenance outside the named adapter files."""
    old, new = Results(previous), Results(roo)
    if old.profile not in ("classic", "extended") or new.profile != "roo_only":
        raise ValueError("Expected a full previous comparison and a Roo-only update")
    if old.suite != new.suite:
        raise ValueError("Benchmark suites differ")
    for result in (old, new):
        if result.data.keys() != analyze.expected_cases(result.suite, result.profile):
            raise ValueError("Incomplete fixture coverage")
    for field in ("board", "flags", "compiler", "arduino", "platform", "cpu_mhz",
                  "psram", "abseil", "fill_ratio", "heap_poisoning",
                  "compiler_options", "dependency_content_sha256", "benchmark_dependencies"):
        if old.build.get(field) != new.build.get(field):
            raise ValueError(f"Configuration changed: {field}")
    def unchanged_sources(build):
        return {name: value for name, value in normalize_sources(build["source_sha256"]).items()
                if not name.startswith("lib/roo_collections/") and name != HARNESS}
    old_sources, new_sources = unchanged_sources(old.build), unchanged_sources(new.build)
    changed = sorted(name for name in old_sources.keys() | new_sources.keys()
                     if old_sources.get(name) != new_sources.get(name))
    if changed and (not allow_string_adapter_changes or
                    not set(changed) <= STRING_ADAPTER_SOURCES):
        raise ValueError("Non-Roo sources changed: " + ", ".join(changed))
    return old, new, changed


def report(previous, roo, merged, note):
    """Produce comparison tables and Roo before/after ratios with consistent case weights."""
    text = ["# ESP32 comparison with refreshed Roo results", "", note, "",
            "This is a composition of separate captures, not a new full-container run. "
            "The original competitor observations are retained unchanged. The new firmware "
            "retains all competitor instantiations and selects Roo at runtime. Code layout, "
            "allocation history and hash seed can differ; before/after ratios do not isolate "
            "algorithm changes. No between-run confidence intervals are claimed.", "",
            "Each Roo point is the median of its run medians; each run has seven timed samples. "
            "Lookup/iteration are ns per entry; erase+insert is ns per pair; build/copy include "
            "destruction; cache_cycle is a whole 102-call cycle. Heap is retained memory, not peak.", "",
            "Raw logs, original metadata and coverage reports are preserved under "
            "`sources/previous/` and `sources/roo/`. `refresh_manifest.json` identifies the "
            "input bundles. `measurements.csv` labels every row's capture source. "
            "`summary.csv` contains every case, including tiny maps.", "",
            "Ratios below are geometric means, with equal weight for every key/size case "
            "with at least 16 entries through each fixture limit. Above 1 means Roo is faster.", ""]
    for hashing in ("default", "absl_hash"):
        text += ["", f"## {hashing}: competitor / current Roo", "",
                 "| Container | Hit | Miss | Iteration | Erase + insert |",
                 "|---|---:|---:|---:|---:|"]
        for name in merged.maps:
            if name == "roo":
                continue
            values = []
            for work in ("find_hit", "find_miss", "iteration", "erase_insert"):
                cases = [case for case in roo.data if case[1] == hashing and case[3] >= 16 and case[4] == work]
                ratios = [merged.get(name, hashing, c[2], c[3], work) / roo.data[c]["median_ns"] for c in cases]
                values.append(f"{statistics.geometric_mean(ratios):.3f}x")
            text.append(f"| {name} | " + " | ".join(values) + " |")
    text += ["", "## Roo before / after speedup", "",
             "Above 1 means the new capture is faster; unchanged competitors were not remeasured.", "",
             "| Key | Hash | Hit | Miss | Iteration | Copy | Erase + insert | Build | Presized build | Cache cycle |",
             "|---|---|---:|---:|---:|---:|---:|---:|---:|---:|"]
    for key in roo.keys:
        for hashing in ("default", "absl_hash"):
            values = []
            for work in analyze.WORKLOADS:
                cases = [c for c in roo.data if c[1] == hashing and c[2] == key and c[3] >= 16 and c[4] == work]
                ratios = [previous.data[c]["median_ns"] / roo.data[c]["median_ns"] for c in cases]
                values.append(f"{statistics.geometric_mean(ratios):.3f}x" if ratios else "—")
            text.append(f"| {key} | {hashing} | " + " | ".join(values) + " |")
    return "\n".join(text) + "\n"


def refresh(previous_bundle, roo_bundle, output, previous_sha256=None, roo_sha256=None,
            *, allow_string_adapter_changes=False):
    """Reanalyze both immutable bundles before atomically publishing a composed result."""
    if output.exists():
        raise ValueError("Refusing to overwrite an existing results directory")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="refresh-", dir=output.parent) as directory:
        temp = Path(directory)
        previous_dir, roo_dir = temp / "sources/previous", temp / "sources/roo"
        bundle.unpack(previous_bundle, previous_dir, previous_sha256)
        bundle.unpack(roo_bundle, roo_dir, roo_sha256)
        old, new, changed = validate_sources(
            previous_dir, roo_dir, allow_string_adapter_changes=allow_string_adapter_changes)
        old_date = old.build["recorded_at"][:10]
        new_date = new.build["recorded_at"][:10]
        note = (f"Roo: {new_date}, {new.minimum_runs} runs/case; other containers: "
                f"{old_date}, {old.minimum_runs} {'run' if old.minimum_runs == 1 else 'runs'}/case, retained. Separate firmware captures.")
        if changed and new.suite == "small_string":
            note += "\nRoo and retained competitors use different SmallString key layouts."
        metadata = dict(new.build)
        metadata.update(comparison_note=note, metadata_scope="Roo refresh; retained peers have separate metadata",
                        comparison_sources={"previous": "sources/previous/build_metadata.json",
                                            "roo": "sources/roo/build_metadata.json"})
        (temp / "build_metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
        for filename in ("summary.csv", "measurements.csv"):
            rows = []
            for source, directory, select_roo in (("previous", previous_dir, False), ("roo", roo_dir, True)):
                for row in read_rows(directory / filename):
                    if (row["map"] == "roo") == select_roo:
                        if filename == "measurements.csv":
                            row["capture_source"] = source
                        rows.append(row)
            rows.sort(key=lambda row: tuple(int(row[k]) if k == "n" else row[k] for k in analyze.GROUP))
            write_rows(temp / filename, rows)
        merged = Results(temp)
        (temp / "REPORT.md").write_text(report(old, new, merged, note))
        manifest = dict(schema_version=1, suite=old.suite, map_profile=old.profile,
                        note=note, replaced_map="roo", previous_cases=len(old.data), refreshed_cases=len(new.data),
                        changed_string_adapter_sources=changed,
                        input_bundles_sha256={"previous": digest(previous_bundle), "roo": digest(roo_bundle)},
                        generated_sha256={name: digest(temp / name) for name in
                                          ("summary.csv", "measurements.csv", "build_metadata.json", "REPORT.md")},
                        refresh_script_sha256=digest(Path(__file__)))
        (temp / "refresh_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        temp.rename(output)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--previous", type=Path, required=True, help="Original full-comparison raw bundle")
    parser.add_argument("--roo", type=Path, required=True, help="Fresh Roo-only raw bundle")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--previous-sha256")
    parser.add_argument("--roo-sha256")
    parser.add_argument("--allow-string-adapter-changes", action="store_true",
                        help="Allow source changes in benchmark_keys.h and benchmark_containers.h. "
                             "The output records the changed sources.")
    args = parser.parse_args()
    result = refresh(args.previous, args.roo, args.output, args.previous_sha256, args.roo_sha256,
                     allow_string_adapter_changes=args.allow_string_adapter_changes)
    print(f"Replaced {result['refreshed_cases']} Roo cases; retained all other rows")


if __name__ == "__main__":
    main()
