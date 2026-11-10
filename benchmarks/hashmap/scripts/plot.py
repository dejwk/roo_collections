"""Plot current map comparisons from validated summary.csv; no hardware access."""

import argparse
import csv
import hashlib
import html
import json
import math
import os
from pathlib import Path
import shlex
import sys

from suites import (GENERAL_KEYS, SMALL_STRING_KEYS, string_key, suite_for_keys,
                    ALL_MAPS, MAP_PROFILES, profile_for_maps, supports_workload)

PROJECT = Path(__file__).resolve().parents[1]
os.environ.setdefault("MPLCONFIGDIR", str(PROJECT / ".pio/matplotlib-cache"))

MAPS = ("roo", "absl_flat", "std_unordered", "absl_node", "std_map")
SMALL_STRING_SERIES = "roo_small_string"
PLOT_MAPS = ALL_MAPS + (SMALL_STRING_SERIES,)
KEYS = ("u32_random", "u32_sequential", "u64_random", "string12", "string48")
KEY_LABELS = ("uint32 · random", "uint32 · sequential", "uint64 · random",
              "String · 12 bytes", "String · 48 bytes")
SHORT_KEYS = ("uint32\nrandom", "uint32\nsequential", "uint64\nrandom",
              "String\n12 bytes", "String\n48 bytes")
LABELS = ("Roo", "Abseil flat", "std::unordered_map", "Abseil node", "std::map", "Tessil robin_map", "unordered_dense",
          "ETL (fixed N)", "Boost flat", "emhash5", "Roo (SmallString)")
COLORS = ("#0072B2", "#E69F00", "#009E73", "#CC79A7", "#606770", "#56B4E9", "#332288", "#CCBB44", "#44AA99", "#882255", "#D55E00")
MARKERS = ("o", "s", "^", "D", "v", ">", "<", "h", "*", "X", "P")
WORKS = ("find_hit", "find_miss", "iteration", "copy_ctor", "erase_insert",
         "build_grow", "build_presized", "cache_cycle")
GROUP = ("map", "hash", "key", "n", "workload")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class Results:
    """Reject inconsistent inputs instead of silently omitting chart series."""

    def __init__(self, directory, small_string_directory=None):
        self.directory = directory
        self.small_string = None
        self.build = json.loads((directory / "build_metadata.json").read_text())
        with (directory / "summary.csv").open(newline="") as stream:
            rows = list(csv.DictReader(stream))
        if not rows:
            raise ValueError("summary.csv is empty")
        self.profile = profile_for_maps({row["map"] for row in rows})
        self.maps = MAP_PROFILES[self.profile]
        self.suite = suite_for_keys({row["key"] for row in rows})
        self.keys = GENERAL_KEYS if self.suite == "general" else SMALL_STRING_KEYS
        self.key_labels = KEY_LABELS if self.suite == "general" else ("SmallString<13> · 12 chars", "SmallString<49> · 48 chars")
        self.short_keys = SHORT_KEYS if self.suite == "general" else ("SmallString\n12 chars", "SmallString\n48 chars")
        self.data = {}
        self.sizes = {key: set() for key in self.keys}
        for row in rows:
            row["n"] = int(row["n"])
            row["runs"] = int(row["runs"])
            for field in ("median_ns", "run_min_ns", "run_max_ns", "sample_min_ns",
                          "sample_max_ns", "heap_bytes", "object_bytes"):
                row[field] = float(row[field])
                if not math.isfinite(row[field]):
                    raise ValueError(f"Non-finite {field}: {row}")
            if (row["map"] not in self.maps or row["key"] not in self.keys or
                    row["workload"] not in WORKS or row["n"] < 1 or row["runs"] < 1):
                raise ValueError(f"Invalid case: {row}")
            allowed = ("ordered",) if row["map"] == "std_map" else ("default", "absl_hash")
            if row["hash"] not in allowed:
                raise ValueError(f"Invalid hash policy: {row}")
            for low, high in (("run_min_ns", "run_max_ns"),
                              ("sample_min_ns", "sample_max_ns")):
                if not 0 < row[low] <= row["median_ns"] <= row[high]:
                    raise ValueError(f"Invalid timing range: {row}")
            if row["heap_bytes"] < 0 or row["object_bytes"] <= 0:
                raise ValueError(f"Invalid footprint: {row}")
            case = tuple(row[field] for field in GROUP)
            if case in self.data:
                raise ValueError(f"Duplicate case: {case}")
            self.data[case] = row
            self.sizes[row["key"]].add(row["n"])
        expected = set()
        for key, sizes in self.sizes.items():
            if not sizes:
                raise ValueError(f"No observations for {key}")
            for n in sizes:
                for name in self.maps:
                    for hashing in (("ordered",) if name == "std_map" else ("default", "absl_hash")):
                        for work in WORKS:
                            if not supports_workload(name, work):
                                continue
                            if work == "cache_cycle" and (not string_key(key) or n < 10):
                                continue
                            expected.add((name, hashing, key, n, work))
        if self.data.keys() != expected:
            raise ValueError(f"Incomplete/inconsistent summary: "
                             f"missing={expected - self.data.keys()}, "
                             f"unexpected={self.data.keys() - expected}")
        counts = [row["runs"] for row in self.data.values()]
        self.minimum_runs, self.maximum_runs = min(counts), max(counts)
        if small_string_directory is not None:
            if self.suite != "general":
                raise ValueError("SmallString overlay requires a general base suite")
            extra = Results(small_string_directory)
            if extra.suite != "small_string":
                raise ValueError("SmallString overlay requires a small_string source suite")
            for field in ("roo_collections_commit", "compiler", "arduino", "platform",
                          "cpu_mhz", "psram", "flags", "abseil", "fill_ratio"):
                if self.build.get(field) != extra.build.get(field):
                    raise ValueError(f"SmallString overlay configuration changed: {field}")
            # Harnesses intentionally differ; require the measured containers and
            # recorded dependency sources to match across the two key types.
            def library_sources(build):
                return {name: value for name, value in build["source_sha256"].items()
                        if name.startswith(("lib/", "absl/"))}
            if library_sources(self.build) != library_sources(extra.build):
                raise ValueError("SmallString overlay library/dependency source hashes differ")
            if ("dependency_content_sha256" in self.build and "dependency_content_sha256" in extra.build
                    and self.build["dependency_content_sha256"] != extra.build["dependency_content_sha256"]):
                raise ValueError("SmallString overlay dependency content differs")
            self.small_string = extra

    def maps_for(self, key, n=None):
        if self.small_string is not None and key.startswith("string"):
            sizes = self.small_string.sizes["small_" + key]
            if n is None or n in sizes:
                return self.maps + (SMALL_STRING_SERIES,)
        return self.maps

    def get(self, name, policy, key, n, work, field="median_ns"):
        if name == SMALL_STRING_SERIES:
            return self.small_string.get("roo", policy, "small_" + key, n, work, field)
        hashing = "ordered" if name == "std_map" else policy
        return self.data[(name, hashing, key, n, work)][field]

    def series(self, name, policy, key, work, field="median_ns", factor=0.001):
        if name == SMALL_STRING_SERIES:
            return self.small_string.series("roo", policy, "small_" + key, work, field, factor)
        if not supports_workload(name, work):
            return [], []
        sizes = sorted(n for n in self.sizes[key] if work != "cache_cycle" or n >= 10)
        return sizes, [self.get(name, policy, key, n, work, field) * factor for n in sizes]

    def fill_label(self):
        ratio = self.build.get("fill_ratio")
        return (f"Roo fill limit {100 * ratio['numerator'] / ratio['denominator']:g}%"
                if ratio else "")

    def footer(self):
        counts = (str(self.minimum_runs) if self.minimum_runs == self.maximum_runs else
                  f"{self.minimum_runs}–{self.maximum_runs}")
        suite = "suite" if self.maximum_runs == 1 else "suites"
        fill = self.fill_label()
        prefix = fill + " · " if fill else ""
        text = (f"{prefix}ESP32 · {self.build['cpu_mhz']} MHz · {' '.join(self.build['flags'])} · warm access · "
                f"heap checks: {self.build.get('heap_poisoning', 'unrecorded')} · "
                f"{counts} {suite}/case, 7 samples/row\n"
                "Medians shown; no confidence intervals. Default capacity policies; "
                "fill ratios are not normalized. Lower is better.")
        if self.profile == "extended":
            text += "\nETL: fixed capacity N, N buckets; embedded storage + adapter included; no dynamic-growth series."
        if self.small_string is not None:
            limit = max(self.small_string.sizes["small_string48"])
            text += ("\nRoo (SmallString): separate capture/board; other string series use std::string. "
                     f"48-character SmallString ends at {limit} entries.")
        return text


def line(ax, results, name, policy, key, work, field="median_ns", factor=0.001):
    i = PLOT_MAPS.index(name)
    x, y = results.series(name, policy, key, work, field, factor)
    if not x:
        return
    ax.plot(x, y, color=COLORS[i], marker=MARKERS[i], markersize=3.3,
            linewidth=2.1 if name in ("roo", SMALL_STRING_SERIES) else 1.35,
            linestyle="--" if name == SMALL_STRING_SERIES else "-", label=LABELS[i],
            zorder=4 if name in ("roo", SMALL_STRING_SERIES) else 2)


def size_axis(ax, sizes, cache=False):
    ax.set_xscale("log", base=2)
    choices = (10, 16, 64, 128, 512) if cache else (1, 8, 32, 128, 512, 2048)
    ticks = [n for n in choices if min(sizes) <= n <= max(sizes)]
    ax.set_xticks(ticks, labels=[str(n) for n in ticks])
    ax.set_xlim(min(sizes) / 1.12, max(sizes) * 1.12)
    ax.minorticks_off()
    ax.grid(axis="y", color="#d9dee5", linewidth=0.6)
    ax.set_axisbelow(True)


def decorate(fig, axes, results, title, subtitle, footer=None):
    short = fig.get_figheight() < 7
    fig.suptitle(title, fontsize=17, fontweight="bold", y=0.982)
    fig.text(0.5, 0.915 if short else 0.936, subtitle, ha="center", fontsize=10, color="#3b4654")
    legend = {}
    for ax in axes:
        handles, labels = ax.get_legend_handles_labels()
        legend.update(zip(labels, handles))
    fig.legend(list(legend.values()), list(legend), loc="upper center",
               bbox_to_anchor=(0.5, 0.872 if short else 0.913),
               ncol=min(6, len(legend)), frameon=False, fontsize=10)
    fig.text(0.5, 0.018, footer or results.footer(), ha="center", va="bottom",
             fontsize=8.5, color="#4b5563", linespacing=1.5)
    for ax in axes:
        ax.spines[["top", "right"]].set_visible(False)
        ax.tick_params(labelsize=8.5)
    top = (0.70 if short else 0.77) if results.profile == "extended" else (0.77 if short else 0.82)
    fig.subplots_adjust(left=0.065, right=0.99, top=top, bottom=0.18 if results.profile == "extended" else 0.145,
                        hspace=0.44, wspace=0.31)


def overview(plt, results, n):
    for key in results.keys:
        if n not in results.sizes[key]:
            raise ValueError(f"Overview size {n} absent for {key}")
    fig, grid = plt.subplots(2, 2, figsize=(18 if len(results.maps) > 5 else 13.8, 9.8))
    panels = (("find_hit", "median_ns", 0.001, "Successful lookup", "µs / query"),
              ("find_miss", "median_ns", 0.001, "Unsuccessful lookup", "µs / query"),
              ("iteration", "median_ns", 1.0, "Iteration", "ns / visited entry"),
              ("find_hit", "heap_bytes", 1 / 1024, "Retained heap after growth", "KiB"))
    for ax, (work, field, factor, title, unit) in zip(grid.flat, panels):
        if field == "heap_bytes" and results.profile == "extended":
            title = "Retained heap · grown / fixed capacity"
        width = 0.78 / (len(results.maps) + (results.small_string is not None))
        for j, key in enumerate(results.keys):
            names = results.maps_for(key, n)
            for position, name in enumerate(names):
                i = PLOT_MAPS.index(name)
                value = results.get(name, "default", key, n, work, field) * factor
                x = j + (position - (len(names) - 1) / 2) * width
                ax.bar(x, value, width, color=COLORS[i], label=LABELS[i],
                       hatch="//" if name == SMALL_STRING_SERIES else None)
        ax.set_xticks(range(len(results.keys)), results.short_keys, fontsize=9)
        ax.set_ylim(bottom=0)
        ax.set_title(title, fontsize=11, pad=10)
        ax.set_ylabel(unit)
        ax.grid(axis="y", color="#d9dee5", linewidth=0.6)
        ax.set_axisbelow(True)
    decorate(fig, list(grid.flat), results, f"Current map comparison · {n} entries",
             "Default hashers; std::map uses ordering. Heap includes the map object and "
             "allocator overhead; it is not peak memory.")
    fig.subplots_adjust(left=0.07, wspace=0.22, hspace=0.48)
    return fig


def timing_grid(plt, results, policy, panels, title, log_y=False):
    fig, grid = plt.subplots(len(panels), len(results.keys), squeeze=False,
                             figsize=(18, (4.7 if results.profile == "extended" else 4.1) * len(panels)))
    for col, (key, key_label) in enumerate(zip(results.keys, results.key_labels)):
        for row, (work, label, factor, unit) in enumerate(panels):
            ax = grid[row, col]
            for name in results.maps_for(key):
                line(ax, results, name, policy, key, work, factor=factor)
            size_axis(ax, results.sizes[key])
            if log_y:
                ax.set_yscale("log")
                from matplotlib.ticker import LogLocator, NullFormatter
                ax.yaxis.set_major_locator(LogLocator(base=10, subs=(1, 2, 5), numticks=12))
                ax.yaxis.set_major_formatter(plain_ticks())
                ax.yaxis.set_minor_formatter(NullFormatter())
            else:
                ax.set_ylim(bottom=0)
            ax.set_title(f"{key_label}\n{label}", fontsize=10, pad=9)
            ax.set_ylabel(unit, fontsize=9)
            if row == len(panels) - 1:
                ax.set_xlabel("Entries · log₂ scale", fontsize=9)
    # Successful/unsuccessful lookups within each key type share a y range.
    if panels[0][0] == "find_hit":
        for col in range(len(results.keys)):
            maximum = max(ax.get_ylim()[1] for ax in grid[:, col])
            for ax in grid[:, col]:
                ax.set_ylim(0, maximum)
    hashing = ("Default hashers" if policy == "default" else
               "Shared absl::Hash<Key> for every hash map")
    subtitle = f"{hashing}; std::map uses ordering. "
    subtitle += "Y axes are logarithmic." if log_y else "Y axes are linear and start at zero."
    decorate(fig, list(grid.flat), results, title, subtitle)
    if len(panels) == 3:
        fig.subplots_adjust(top=0.79 if results.profile == "extended" else 0.82, bottom=0.14, hspace=0.5)
    return fig


def plain_ticks():
    from matplotlib.ticker import FuncFormatter
    return FuncFormatter(lambda value, _: f"{value:g}")


def cache_chart(plt, results, policy):
    fig, axes = plt.subplots(1, 2, figsize=(13.8, 6.7))
    for ax, key, label in zip(axes, results.keys[-2:], results.key_labels[-2:]):
        for name in results.maps_for(key):
            line(ax, results, name, policy, key, "cache_cycle")
        sizes = [n for n in results.sizes[key] if n >= 10]
        size_axis(ax, sizes, cache=True)
        ax.set_title(label, fontsize=11)
        ax.set_xlabel("Entries · log₂ scale")
        ax.set_ylabel("µs / entire cache cycle")
    limit = max(ax.get_ylim()[1] for ax in axes)
    for ax in axes:
        ax.set_ylim(0, limit)
    hashing = "Default hashers" if policy == "default" else "Shared absl::Hash<Key>"
    decorate(fig, list(axes), results, "Cache churn · string keys",
             f"{hashing}; std::map uses ordering. One cycle = 90 hits + 10 misses + "
             "one eviction + one insertion.")
    fig.subplots_adjust(bottom=0.20, wspace=0.2)
    return fig


def memory_chart(plt, results, presizing=False):
    fig, grid = plt.subplots(2, len(results.keys), figsize=(18, 9.4 if results.profile == "extended" else 8.2))
    policies = ("default", "default") if presizing else ("default", "absl_hash")
    for row, policy in enumerate(policies):
        work = "build_presized" if presizing and row == 1 else "find_hit"
        for col, (key, label) in enumerate(zip(results.keys, results.key_labels)):
            ax = grid[row, col]
            for name in results.maps_for(key):
                line(ax, results, name, policy, key, work, "heap_bytes", 1 / 1024)
            size_axis(ax, results.sizes[key])
            ax.set_yscale("log")
            ax.yaxis.set_major_formatter(plain_ticks())
            ax.set_ylabel("Retained heap · KiB", fontsize=9)
            hashing = "Default hash" if policy == "default" else "Shared Abseil hash"
            if presizing:
                hashing = "Presized for N entries" if row == 1 else "Grown from default capacity"
                if row == 0 and results.profile == "extended":
                    hashing = "Grown / ETL fixed capacity N"
            ax.set_title(f"{label}\n{hashing}", fontsize=10, pad=9)
            if row == 1:
                ax.set_xlabel("Entries · log₂ scale", fontsize=9)
    for col in range(len(results.keys)):
        low = min(ax.get_ylim()[0] for ax in grid[:, col])
        high = max(ax.get_ylim()[1] for ax in grid[:, col])
        for ax in grid[:, col]:
            ax.set_ylim(low, high)
    if presizing:
        decorate(fig, list(grid.flat), results, "Retained memory · growing versus presizing",
                 "Default hashers; logarithmic axes. Heap includes the map object, owned "
                 "strings and allocator overhead; not peak memory. std::map stays ordered.")
    else:
        decorate(fig, list(grid.flat), results, "Retained memory · default and shared hashing",
                 "Logarithmic axes. Grown-map footprint includes its object, owned strings "
                 "and allocator overhead; not peak memory. std::map remains ordered.")
    return fig


def write_gallery(output, charts, results, manifest):
    intro = (f"Current ESP32 comparison from {len(results.data):,} validated cases. "
             f"Roo HEAD base: {results.build['roo_collections_commit'][:7]}; "
             "the recorded source hashes identify the working-tree snapshot.")
    if results.fill_label():
        intro += " " + results.fill_label() + "."
    if results.small_string is not None:
        intro += (f" Roo (SmallString) is an additional string-only series from a separate "
                  f"{len(results.small_string.data):,}-case capture on "
                  f"{results.small_string.build['board']}; other string series use std::string. "
                  "Timing differences also include board and binary-layout effects.")
    source_report = os.path.relpath(results.directory / "REPORT.md", output)
    policy_note = (results.fill_label() + "; competitor capacity/load policies remain at defaults. "
                   if results.fill_label() else "Default capacity/load policies are retained. ")
    notes = ("Every point is a per-case median; seven timed samples per observation. "
             "The latest dataset has only one suite, so between-suite variation is "
             "not measured." if results.maximum_runs == 1 else
             "Every point is the median of available suite medians; seven timed samples "
             "per observation. The CSV retains per-case counts and timing ranges.")
    readme = ["# Current performance charts", "", intro, "", notes, "",
              "[Browse all charts](index.html) · [Combined PDF](benchmark_charts.pdf) · "
              f"[Source tables]({source_report})", "",
              "Default hashers reflect normal use. Shared-hash charts supply "
              "`absl::Hash<Key>` to every hash map; `std::map` stays ordered. "
              + policy_note + "Copy/build include "
              "destruction; erase/insert is a pair; cache is an entire 102-call cycle. "
              "These charts do not measure immutable owners.", ""]
    investigation = ""
    cards = []
    for stem, title, group in charts:
        readme += [f"## {title}", "", f"![{title}]({stem}.png)", "",
                   f"[PNG]({stem}.png) · [SVG]({stem}.svg)", ""]
        cards.append(f'<section data-group="{group}"><h2>{html.escape(title)}</h2>'
                     f'<a href="{stem}.svg" title="Open scalable SVG"><img loading="lazy" '
                     f'src="{stem}.png" alt="{html.escape(title)}"></a>'
                     f'<p><a href="{stem}.png">PNG</a> · <a href="{stem}.svg">SVG</a></p></section>')
    command = ["python", "scripts/plot.py", "--results-dir", str(results.directory),
               "--output-dir", str(output), "--overview-size", str(manifest["overview_size"]),
               "--dpi", str(manifest["dpi"])]
    if results.small_string is not None:
        command += ["--small-string-results", str(results.small_string.directory)]
    readme += ["## Regenerate", "", "```sh", shlex.join(command), "```", "",
               "Requires scripts/requirements-charts.txt. Inputs and script hashes, "
               "the Matplotlib version, and plotted point counts are in manifest.json.", ""]
    (output / "README.md").write_text("\n".join(readme))
    page = '<!doctype html><html lang="en"><meta charset="utf-8">' \
           '<meta name="viewport" content="width=device-width,initial-scale=1">' \
           '<title>ESP32 map benchmark charts</title><style>' \
           'body{font:16px system-ui,sans-serif;background:#f5f7fa;color:#182331;' \
           'max-width:1500px;margin:auto;padding:24px}h1{margin-top:0}' \
           'section{background:white;border:1px solid #dbe2e9;border-radius:8px;' \
           'padding:16px;margin:24px 0}img{display:block;width:100%;height:auto}' \
           'a{color:#0072b2}button{font:inherit;padding:8px 16px;margin:4px;cursor:pointer}' \
           'button[aria-pressed=true]{background:#0072b2;color:white;border-color:#0072b2}' \
           'section[hidden]{display:none}nav{position:sticky;top:0;background:#f5f7fa;' \
           'padding:8px 0;border-bottom:1px solid #dbe2e9}</style>' \
           f'<h1>Current ESP32 map performance</h1><p>{html.escape(intro)}</p>' \
           f'<p>{html.escape(notes)}</p><p>Lower is better. Click a chart for its scalable SVG. ' \
           '<a href="benchmark_charts.pdf">Download all charts as PDF</a>.</p>' \
           f'{investigation}' \
           '<nav aria-label="Chart filter"><button data-filter="all" aria-pressed="true">All charts</button>' \
           '<button data-filter="default" aria-pressed="false">Default hashers</button>' \
           '<button data-filter="shared" aria-pressed="false">Shared Abseil hash</button></nav>' \
           + "".join(cards) + '<script>' \
           'document.querySelectorAll("button[data-filter]").forEach(button=>{' \
           'button.onclick=()=>{const group=button.dataset.filter;' \
           'document.querySelectorAll("button[data-filter]").forEach(b=>' \
           'b.setAttribute("aria-pressed",String(b===button)));' \
           'document.querySelectorAll("section[data-group]").forEach(section=>' \
           'section.hidden=group!=="all"&&section.dataset.group!=="both"&&section.dataset.group!==group);};});' \
           '</script></html>'
    (output / "index.html").write_text(page)
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--results-dir", type=Path, default=PROJECT / "results")
    parser.add_argument("--small-string-results", type=Path,
                        help="Add Roo (SmallString) to the general suite's string charts")
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--overview-size", type=int, default=128)
    parser.add_argument("--dpi", type=int, default=160)
    args = parser.parse_args()
    if args.dpi < 72:
        parser.error("--dpi must be at least 72")
    results = Results(args.results_dir, args.small_string_results)
    output = args.output_dir or args.results_dir / "charts"
    output.mkdir(parents=True, exist_ok=True)
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        from matplotlib.backends.backend_pdf import PdfPages
    except ImportError:
        parser.exit(2, "Install chart dependencies: python -m pip install -r "
                    "scripts/requirements-charts.txt\n")
    plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 10,
                         "svg.fonttype": "none", "svg.hashsalt": digest(args.results_dir / "summary.csv"),
                         "pdf.fonttype": 42, "axes.labelcolor": "#243040",
                         "text.color": "#182331", "axes.edgecolor": "#a3adb9"})
    charts = []
    chart_stats = {}
    with PdfPages(output / "benchmark_charts.pdf",
                  metadata={"Title": "Current ESP32 map benchmark", "CreationDate": None,
                            "ModDate": None}) as pdf:
        def save(stem, title, group, fig):
            chart_stats[stem] = {
                "plotted_points": sum(len(line.get_xdata()) for ax in fig.axes for line in ax.lines)
                                  + sum(len(container.datavalues) for ax in fig.axes
                                        for container in ax.containers if hasattr(container, "datavalues")),
                "panels": len(fig.axes),
                "x_scales": sorted({ax.get_xscale() for ax in fig.axes}),
                "y_scales": sorted({ax.get_yscale() for ax in fig.axes}),
            }
            fig.savefig(output / f"{stem}.png", dpi=args.dpi, facecolor="white")
            fig.savefig(output / f"{stem}.svg", facecolor="white", metadata={"Date": None})
            pdf.savefig(fig, facecolor="white")
            plt.close(fig)
            charts.append((stem, title, group))
            print(f"Generated {stem}", flush=True)

        save("overview", f"Overview at {args.overview_size} entries", "default",
             overview(plt, results, args.overview_size))
        for policy, suffix, group in (("default", "default", "default"),
                                       ("absl_hash", "shared", "shared")):
            save(f"lookup_{suffix}", f"Lookup across sizes · {suffix} hashing", group,
                 timing_grid(plt, results, policy,
                             (("find_hit", "Successful lookup", .001, "µs / query"),
                              ("find_miss", "Unsuccessful lookup", .001, "µs / query")),
                             "Lookup performance across map sizes"))
            save(f"construction_{suffix}", f"Construction and copy · {suffix} hashing", group,
                 timing_grid(plt, results, policy,
                             (("copy_ctor", "Copy + destruction", .001, "µs / source entry"),
                              ("build_grow", "Build with growth + destruction", .001, "µs / inserted entry"),
                              ("build_presized", "Presized build + destruction", .001, "µs / inserted entry")),
                             "Copy and construction costs", log_y=True))
            save(f"iteration_mutation_{suffix}", f"Iteration and mutation · {suffix} hashing", group,
                 timing_grid(plt, results, policy,
                             (("iteration", "Iteration", .001, "µs / visited entry"),
                              ("erase_insert", "Erase + insert pair", .001, "µs / pair")),
                             "Iteration and erase/insert costs", log_y=True))
            save(f"cache_{suffix}", f"Cache churn · {suffix} hashing", group,
                 cache_chart(plt, results, policy))
        save("memory", "Retained heap · default and shared hashing", "both",
             memory_chart(plt, results))
        save("memory_presized", "Retained heap · growing versus presizing", "default",
             memory_chart(plt, results, presizing=True))
    manifest = {"input_base": os.path.relpath(args.results_dir, output),
                "fill_ratio": results.build.get("fill_ratio"),
                "inputs": {name: digest(args.results_dir / name)
                            for name in ("summary.csv", "build_metadata.json")},
                "script_sha256": digest(Path(__file__)), "matplotlib_version": matplotlib.__version__,
                "map_profile": results.profile, "cases": len(results.data), "overview_size": args.overview_size,
                "minimum_suites_per_case": results.minimum_runs,
                "maximum_suites_per_case": results.maximum_runs,
                "samples_per_row": 7, "dpi": args.dpi,
                "charts": [{"stem": stem, "title": title, "group": group, **chart_stats[stem]}
                           for stem, title, group in charts]}
    if results.small_string is not None:
        manifest["small_string_overlay"] = {
            "input_base": os.path.relpath(results.small_string.directory, output),
            "inputs": {name: digest(results.small_string.directory / name)
                       for name in ("summary.csv", "build_metadata.json")},
            "cases": len(results.small_string.data),
            "minimum_suites_per_case": results.small_string.minimum_runs,
            "maximum_suites_per_case": results.small_string.maximum_runs,
            "board": results.small_string.build["board"],
            "series": "Roo (SmallString)",
        }
    write_gallery(output, charts, results, manifest)
    print(f"Wrote {len(charts)} charts (PNG + SVG), PDF and gallery to {output}")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, FileNotFoundError) as error:
        print(f"Chart input error: {error}", file=sys.stderr)
        sys.exit(2)
