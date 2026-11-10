"""Validate complete serial suites and produce CSV plus comparison tables."""
import argparse
from collections import defaultdict
import csv
import json
import math
from pathlib import Path
import re
import statistics
import subprocess
import sys

from suites import SUITES, string_key

FIELDS = "run map hash key n workload min_ns median_ns max_ns batches heap_bytes object_bytes".split()
GROUP = "map hash key n workload".split()
MAPS = ("roo", "absl_flat", "std_unordered", "absl_node", "std_map")
TYPES = ("u32_random", "u32_sequential", "u64_random", "string12", "string48")
WORKLOADS = ("find_hit", "find_miss", "iteration", "copy_ctor", "erase_insert",
             "build_grow", "build_presized", "cache_cycle")
SIZES = (1, 2, 4, 7, 8, 10, 15, 16, 32, 54, 64, 100, 128, 256, 400, 512, 1024, 2048)


def expected_cases(suite="general"):
    result = set()
    for key in SUITES[suite]:
        string = string_key(key)
        for n in SIZES:
            if key == "small_string48" and n > 400:
                continue
            if string and n > 512:
                continue
            if key == "u64_random" and n > 1024:
                continue
            for name in MAPS:
                hashes = ("ordered",) if name == "std_map" else ("default", "absl_hash")
                for hashing in hashes:
                    for work in WORKLOADS:
                        if work == "cache_cycle" and (not string or n < 10):
                            continue
                        result.add((name, hashing, key, n, work))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logs", type=Path, nargs="+")
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--charts", action="store_true",
                        help="Generate current-comparison PNG/SVG charts, PDF and gallery "
                             "using scripts/requirements-charts.txt")
    parser.add_argument("--min-observations", type=int,
                        help="Allow missing cases in ended suites and discard malformed ROW "
                             "records; require this many intact observations of every case")
    parser.add_argument("--allow-partial-suites", action="store_true",
                        help="With --min-observations, retain only completed fixtures "
                             "followed by another fixture in an interrupted suite")
    args = parser.parse_args()
    if args.min_observations is not None and args.min_observations < 1:
        parser.error("--min-observations must be positive")
    if args.allow_partial_suites and args.min_observations is None:
        parser.error("--allow-partial-suites requires --min-observations")
    rows = []
    runs = {}
    expected = None
    suite = None
    metadata = []
    recovered_separators = 0
    excluded = []
    row_records = {}
    for path in args.logs:
        # USBIP occasionally drops only the newline separating two otherwise
        # intact records. Recover explicit protocol boundaries, never payloads.
        raw = path.read_text()
        recovered_separators += len(re.findall(r"[^\r\n](?=(?:BEGIN|HEADER|ROW|END),)", raw))
        records = re.sub(r"(?=(?:BEGIN|HEADER|ROW|END),)", "\n", raw)
        for line in records.splitlines():
            fields = next(csv.reader([line]))
            if not fields:
                continue
            if fields[0] == "BEGIN":
                run = f"{path.name}:{fields[1]}"
                if run in runs:
                    raise ValueError(f"Duplicate BEGIN for {run}")
                start = dict(part.split("=", 1) for part in fields[2:])
                if start.get("cpu_mhz") != "240" or start.get("psram") != "0":
                    raise ValueError(f"Unexpected device configuration: {line}")
                current_suite = start.get("suite", "general")
                if current_suite not in SUITES:
                    raise ValueError(f"Unknown benchmark suite: {current_suite}")
                if suite is not None and suite != current_suite:
                    raise ValueError("Cannot combine different benchmark suites")
                suite = current_suite
                expected = expected_cases(suite)
                runs[run] = {"end": False, "cases": set(), "start": start}
                metadata.append(f"{path.name}: {line}")
            elif fields[0] == "END":
                run = f"{path.name}:{fields[1]}"
                end = dict(part.split("=", 1) for part in fields[2:])
                if runs[run]["end"] or end.get("heap") != runs[run]["start"].get("heap"):
                    raise ValueError(f"Duplicate END or suite heap changed: {line}")
                runs[run]["end"] = True
                metadata.append(f"{path.name}: {line}")
            elif fields[0] == "ROW":
                if len(fields) != len(FIELDS) + 1:
                    if args.min_observations is not None:
                        excluded.append({"log": path.name, "record": line,
                                         "reason": "Wrong ROW field count; entire record discarded"})
                        continue
                    raise ValueError(f"Malformed row: {line}")
                row = dict(zip(FIELDS, fields[1:]))
                row["run"] = f"{path.name}:{row['run']}"
                for field in ("n", "batches", "heap_bytes", "object_bytes"):
                    row[field] = int(row[field])
                if row["batches"] < 1 or row["heap_bytes"] < 0 or row["object_bytes"] < 1:
                    raise ValueError(f"Invalid counts or memory: {line}")
                for field in ("min_ns", "median_ns", "max_ns"):
                    row[field] = float(row[field])
                    if not math.isfinite(row[field]) or row[field] <= 0:
                        raise ValueError(f"Invalid timing: {line}")
                if not row["min_ns"] <= row["median_ns"] <= row["max_ns"]:
                    raise ValueError(f"Unordered samples: {line}")
                case = tuple(row[k] for k in GROUP)
                if case not in expected:
                    raise ValueError(f"Unexpected case: {case}")
                seen = runs[row["run"]]["cases"]
                if case in seen:
                    raise ValueError(f"Duplicate case: {case}")
                seen.add(case)
                rows.append(row)
                row_records[id(row)] = (path.name, line)
            elif any(word in line for word in ("FAIL,", "Guru Meditation", "CORRUPT HEAP", "abort()")):
                raise ValueError(f"Board failure: {line}")
    if not runs:
        raise ValueError("No benchmark suites")
    if args.allow_partial_suites:
        discarded = set()
        for run, state in runs.items():
            if state["end"]:
                continue
            run_rows = [row for row in rows if row["run"] == run]
            if not run_rows:
                raise ValueError(f"Interrupted suite has no intact records: {run}")
            fixtures = defaultdict(list)
            for row in run_rows:
                fixtures[tuple(row[k] for k in GROUP[:-1])].append(row)
            final_fixture = tuple(run_rows[-1][k] for k in GROUP[:-1])
            for fixture, values in fixtures.items():
                cases = {tuple(row[k] for k in GROUP) for row in values}
                required = {case for case in expected if case[:-1] == fixture}
                # Every fixture ends with heap checks before the next fixture's
                # first ROW. A terminal fixture cannot prove those checks ran.
                if fixture == final_fixture or cases != required:
                    for row in values:
                        discarded.add(id(row))
                        log, record = row_records[id(row)]
                        excluded.append({"log": log, "record": record,
                                         "reason": "Unfinished/terminal fixture in interrupted suite; discarded"})
                    state["cases"] -= cases
        rows = [row for row in rows if id(row) not in discarded]
    for run, state in runs.items():
        if (not state["end"] and not args.allow_partial_suites) or (state["cases"] != expected and args.min_observations is None):
            raise ValueError(f"Incomplete {run}: {len(state['cases'])}/{len(expected)} cases, "
                             f"end={state['end']}, missing={expected - state['cases']}")
    out = args.output_dir
    out.mkdir(parents=True, exist_ok=True)
    build_path = out / "build_metadata.json"
    build = json.loads(build_path.read_text()) if build_path.exists() else {}
    measured_commit = build.get("roo_collections_commit", "unrecorded")
    if build.get("frozen_sources"):
        source_note = (f"Measured Roo Collections source: commit `{measured_commit}`, "
                       "from a frozen source snapshot. Source hashes and snapshot "
                       "provenance are recorded in build_metadata.json.")
    else:
        source_note = (f"Measured Roo Collections source: commit `{measured_commit}`. "
                       "Source hashes in build_metadata.json identify the measured working tree.")
    if any(line[3:].startswith("src/") for line in build.get("roo_collections_status", [])):
        source_note += (" This snapshot includes uncommitted library source changes; "
                        "the commit identifies its HEAD base, not a clean checkout. "
                        "The recorded status and hashes identify the measured code.")
    with (out / "measurements.csv").open("w") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS)
        writer.writeheader()
        writer.writerows(rows)
    groups = defaultdict(list)
    for row in rows:
        groups[tuple(row[k] for k in GROUP)].append(row)
    if set(groups) != expected:
        raise ValueError("Combined observations do not cover the expected case set")
    if args.min_observations is not None:
        insufficient = {case: len(values) for case, values in groups.items()
                        if len(values) < args.min_observations}
        if insufficient:
            raise ValueError(f"Insufficient intact observations: {insufficient}")
    summary = {}
    for case, values in sorted(groups.items()):
        times = [r["median_ns"] for r in values]
        item = dict(zip(GROUP, case))
        item.update(median_ns=statistics.median(times),
                    run_min_ns=min(times), run_max_ns=max(times),
                    sample_min_ns=min(r["min_ns"] for r in values),
                    sample_max_ns=max(r["max_ns"] for r in values),
                    heap_bytes=statistics.median(r["heap_bytes"] for r in values),
                    object_bytes=values[0]["object_bytes"], runs=len(values))
        summary[case] = item
    with (out / "summary.csv").open("w") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(next(iter(summary.values()))))
        writer.writeheader()
        writer.writerows(summary.values())

    def value(name, hashing, key, n, work, field="median_ns"):
        return summary[(name, hashing, key, n, work)][field]

    ended = sum(state["end"] for state in runs.values())
    complete = sum(state["end"] and state["cases"] == expected for state in runs.values())
    minimum = min(len(values) for values in groups.values())
    maximum = max(len(values) for values in groups.values())
    text = ["# ESP32 hash map results", "", f"Benchmark suite: `{suite}`.", "",
            f"Validated {complete} complete suites, {len(rows)} rows and {minimum}–{maximum} observations per case. "
            "Each observation is the median of seven timed samples. Between-suite variation is unavailable with one suite.", "",
            source_note, "",
            "## Measurement", "",
            "Mutable maps; warm repeated access; deterministic keys and visit order. "
            "Lookup/iteration are ns per entry, erase+insert is ns per pair, copy/build include destruction, "
            "and cache churn is ns per 90 hits + 10 misses + one eviction + one insertion. "
            "Heap is retained internal 8-bit memory including the map object and owned keys, not peak memory.", "",
            "Build metadata: `build_metadata.json`. Raw logs are preserved; summary.csv contains medians and "
            "ranges, and measurements.csv contains each validated observation. Default policies are not normalized.", "",
            f"Transport separators recovered: {recovered_separators}; excluded records: {len(excluded)}. "
            "Payload fields are never repaired. See excluded_records.json for coverage and exclusions.", "",
            "## Device records", "", "```text", *metadata, "```", ""]
    for key in SUITES[suite]:
        text += [f"## {key}, 100 entries, default hashing", "",
                 "| Map | Hit | Miss | Iterate | Copy | Erase+insert | Build | Presized build | Heap B |",
                 "|---|---:|---:|---:|---:|---:|---:|---:|---:|"]
        for name in MAPS:
            hashing = "ordered" if name == "std_map" else "default"
            values = [value(name, hashing, key, 100, work) for work in WORKLOADS[:-1]]
            heap = value(name, hashing, key, 100, "find_hit", "heap_bytes")
            text.append("| " + name + " | " + " | ".join(f"{v:.1f}" for v in values) + f" | {heap:.0f} |")
        text.append("")
    text += ["## Lookup speed ratios, 16 entries through each fixture limit (at most 512)", "",
             "Geometric mean of competitor/Roo time. Above 1 means Roo is faster; this is a fixed fixture, "
             "not a confidence interval or universal ranking.", "",
             "| Hash | Key | Workload | Abseil flat / Roo | std unordered / Roo | Abseil node / Roo |",
             "|---|---|---|---:|---:|---:|"]
    selected = [n for n in SIZES if 16 <= n <= 512]
    for hashing in ("default", "absl_hash"):
        for key in SUITES[suite]:
            key_sizes = [n for n in selected if ("roo", hashing, key, n, "find_hit") in summary]
            for work in ("find_hit", "find_miss"):
                ratios = [math.exp(statistics.mean(math.log(value(name, hashing, key, n, work) /
                          value("roo", hashing, key, n, work)) for n in key_sizes))
                          for name in ("absl_flat", "std_unordered", "absl_node")]
                text.append(f"| {hashing} | {key} | {work} | " + " | ".join(f"{v:.3f}x" for v in ratios) + " |")
    text += ["", "Only the recorded board, toolchain and one deterministic seed are represented. "
             "The suite does not measure immutable owners, read-only base interfaces, heterogeneous lookup, "
             "latency tails, fragmentation after arbitrary lifetimes, peak heap or isolated firmware size. "
             "Maximum fixture sizes are constrained by contiguous memory needed for copies.", ""]
    (out / "excluded_records.json").write_text(json.dumps({
        "benchmark_suite": suite, "attempted_suites": len(runs), "ended_suites": ended,
        "interrupted_suites": len(runs) - ended, "suites_with_complete_case_coverage": complete,
        "minimum_observations_per_case": minimum, "maximum_observations_per_case": maximum,
        "excluded_records": excluded,
        "suite_coverage": [{"suite": run, "intact_cases": len(state["cases"]),
                            "expected_cases": len(expected), "ended": state["end"]}
                           for run, state in runs.items()],
    }, indent=2) + "\n")
    (out / "REPORT.md").write_text("\n".join(text))
    if args.charts:
        subprocess.run([sys.executable, str(Path(__file__).with_name("plot.py")),
                        "--results-dir", str(out)], check=True)
    print(f"Validated {complete} complete suites and {len(rows)} rows; wrote {out}")


if __name__ == "__main__":
    main()
