"""Record actual compiler/SDK versions, source bytes and firmware for a built benchmark."""

import argparse
from datetime import datetime, timezone
from fractions import Fraction
import json
from pathlib import Path
import re
import shlex
import subprocess

from common import HARNESS, PROJECT, digest, source_fingerprint


def package_version(root):
    return json.loads((Path(root) / "package.json").read_text())["version"]


def provenance(root):
    snapshot = root / "source_provenance.json"
    if snapshot.exists():
        return json.loads(snapshot.read_text())
    marker = root / ".archive-sha256"
    if marker.exists():
        return {"commit": None, "status": [], "archive_sha256": marker.read_text().strip()}
    def git(*args):
        return subprocess.check_output(["git", "-C", str(root), *args], text=True).strip()
    return {"commit": git("rev-parse", "HEAD"), "status": git("status", "--short").splitlines()}


def verify_receipt(fingerprint, firmware, receipt_path):
    receipt = json.loads(receipt_path.read_text())
    if receipt["fingerprint"] != fingerprint or receipt["firmware_sha256"] != digest(firmware):
        raise ValueError("Sources or firmware changed after compilation; rebuild before recording metadata")


def harness_command(commands):
    expected = (PROJECT / "src/main.cpp").resolve()
    for row in commands:
        source = Path(row["file"])
        if not source.is_absolute():
            source = Path(row.get("directory", PROJECT)) / source
        if source.resolve() == expected:
            return row.get("arguments") or shlex.split(row["command"])
    raise ValueError("Benchmark translation unit is missing from compile_commands.json")


def collect(board_label=None):
    context = json.loads((PROJECT / ".pio/build_context.json").read_text())
    commands = json.loads((PROJECT / "compile_commands.json").read_text())
    command = harness_command(commands)
    compiler_root = Path(context["compiler_root"])
    executable = next(arg for arg in command if Path(arg).name.endswith(("g++", "c++")))
    compiler = Path(executable)
    if not compiler.is_absolute():
        compiler = compiler_root / "bin" / compiler.name
    flags = [s for s in command if s.startswith(("-O", "-std=", "-DNDEBUG"))]
    roots = {name: Path(path) for name, path in context["library_source_roots"].items()}
    fingerprint = source_fingerprint(context)
    absl = Path(context["abseil_root"])
    firmware = PROJECT / ".pio/build/esp32dev/firmware.bin"
    verify_receipt(fingerprint, firmware, PROJECT / ".pio/build_receipt.json")
    pins = json.loads((PROJECT / "dependencies.json").read_text())
    versions = {name: provenance(root) for name, root in roots.items()}
    configs = list((Path(context["framework_libraries_root"]) / "esp32").glob("*/include/sdkconfig.h"))
    selected = [path for path in configs if str(path.parent) in " ".join(command)]
    sdk = selected[0] if len(selected) == 1 else None
    sdk_text = sdk.read_text() if sdk is not None else ""
    poisoning = "unrecorded"
    for setting, label in (("LIGHT", "light"), ("COMPREHENSIVE", "comprehensive"), ("DISABLED", "disabled")):
        if re.search(r"#define CONFIG_HEAP_POISONING_" + setting + r"\s+1\b", sdk_text):
            poisoning = label
    metadata = dict(schema_version=1, recorded_at=datetime.now(timezone.utc).isoformat(),
        board=board_label or context["board"], board_label=board_label,
        cpu_mhz=240, psram=False, heap_poisoning=poisoning,
        abseil=pins["abseil"]["version"], platform=context["platform"],
        arduino=package_version(context["framework_root"]),
        compiler="xtensa-esp-elf " + package_version(compiler_root), flags=flags,
        compiler_banner=subprocess.check_output([compiler, "--version"], text=True).splitlines()[0],
        roo_collections_commit=versions["roo_collections"]["commit"],
        roo_collections_status=versions["roo_collections"]["status"],
        roo_backport_commit=versions["roo_backport"].get("commit"),
        library_source_roots={name: str(root) for name, root in roots.items()},
        frozen_sources=all((root / "source_provenance.json").exists() for root in roots.values()),
        dependency_content_sha256=fingerprint["dependency_content_sha256"],
        source_sha256=fingerprint["source_sha256"], firmware_sha256=digest(firmware), firmware_bytes=firmware.stat().st_size,
        build_inputs_sha256=fingerprint["configuration_sha256"],
        compiler_options=[s for s in command if s.startswith(("-D", "-U", "-f", "-m", "-O", "-std="))])
    metadata["benchmark_profile"] = context.get("benchmark_profile", "classic")
    metadata["benchmark_dependencies"] = {name: {key: pin[key] for key in ("version", "sha256", "url")}
                                           for name, pin in pins.items()}
    policy = (roots["roo_collections"] / "src/roo_collections/impl/flat_small_hashtable_common.h").read_text()
    ratio = re.search(r"kMaxFillRatio\s*=\s*([\d.]+)f?\s*/\s*([\d.]+)f?", policy)
    if ratio:
        fill = Fraction(ratio[1]) / Fraction(ratio[2])
        metadata["fill_ratio"] = {"numerator": fill.numerator, "denominator": fill.denominator}
    return metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--board-label", help="Optional identity of the physical board; the serial BEGIN also records chip/revision")
    args = parser.parse_args()
    metadata = collect(args.board_label)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / "build_metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print("Recorded actual toolchain/SDK versions, source hashes and firmware hash")


if __name__ == "__main__":
    main()
