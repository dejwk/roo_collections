"""Host checks for strict capture validation, provenance checks and result archives."""

import csv
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

SCRIPTS = Path(__file__).resolve().parents[1] / "scripts"
sys.path.insert(0, str(SCRIPTS))
import analyze
import bundle
import common
import fetch_dependencies
import metadata as build_metadata


def fixture(directory, suite="general"):
    """Generate all cases in protocol order without checking in measurement data."""
    directory.mkdir(parents=True, exist_ok=True)
    log = directory / "esp32.log"
    with log.open("w") as output:
        tag = "" if suite == "general" else f",suite={suite}"
        output.write(f"BEGIN,1,chip=synthetic,cpu_mhz=240,psram=0,heap=123456{tag}\n")
        for name, hashing, key, n, work in sorted(analyze.expected_cases(suite)):
            output.write(f"ROW,1,{name},{hashing},{key},{n},{work},1.0,2.0,3.0,1,128,28\n")
        output.write("END,1,heap=123456\n")
    metadata = dict(roo_collections_commit="fixture", roo_backport_commit="fixture",
                    roo_collections_status=[], flags=["-std=gnu++17", "-O2", "-DNDEBUG"],
                    compiler="synthetic", arduino="synthetic", platform="synthetic",
                    cpu_mhz=240, psram=False, abseil="20250512.1", frozen_sources=False,
                    source_sha256={common.HARNESS: "a" * 64,
                                   "lib/roo_collections/src/hash.h": "b" * 64,
                                   "lib/roo_backport/src/header.h": "c" * 64})
    (directory / "build_metadata.json").write_text(json.dumps(metadata))
    return log


def run_script(name, *arguments, check=True):
    return subprocess.run([sys.executable, str(SCRIPTS / name), *map(str, arguments)],
                          check=check, text=True, capture_output=True)


class ToolsTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.results = self.root / "run"
        self.log = fixture(self.results)

    def test_complete_suite_preserves_all_case_values(self):
        run_script("analyze.py", self.log, "--output-dir", self.results)
        with (self.results / "summary.csv").open() as stream:
            rows = list(csv.DictReader(stream))
        self.assertEqual(len(rows), 5553)
        self.assertTrue(all(float(row["median_ns"]) == 2 and int(row["runs"]) == 1 for row in rows))
        coverage = json.loads((self.results / "excluded_records.json").read_text())
        self.assertEqual(coverage["suites_with_complete_case_coverage"], 1)
        self.assertEqual(coverage["excluded_records"], [])
        self.assertNotIn("251 slots", (self.results / "REPORT.md").read_text())

    def test_small_string_suite_is_complete_and_comparable(self):
        before = self.root / "small-before"
        after = self.root / "small-after"
        for directory in (before, after):
            log = fixture(directory, "small_string")
            run_script("analyze.py", log, "--output-dir", directory)
            with (directory / "summary.csv").open() as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(len(rows), 2142)
            self.assertEqual({row["key"] for row in rows}, {"small_string12", "small_string48"})
            self.assertTrue(any(row["workload"] == "cache_cycle" for row in rows))
            import plot
            plotted = plot.Results(directory)
            self.assertEqual(plotted.suite, "small_string")
            self.assertEqual(len(plotted.data), 2142)
        run_script("compare.py", "--baseline", before, "--updated", after)
        with (after / "comparison.csv").open() as stream:
            rows = list(csv.DictReader(stream))
        self.assertTrue(all(float(row["change_percent"]) == 0 for row in rows))

    def test_suite_tag_rejects_mixed_or_unknown_workloads(self):
        small = fixture(self.root / "small", "small_string")
        mixed = run_script("analyze.py", self.log, small, "--output-dir", self.results, check=False)
        self.assertNotEqual(mixed.returncode, 0)
        small.write_text(small.read_text().replace("suite=small_string", "suite=unrecognized"))
        unknown = run_script("analyze.py", small, "--output-dir", self.results, check=False)
        self.assertNotEqual(unknown.returncode, 0)

    def test_small_string_overlay_uses_separate_values_and_available_sizes(self):
        """Verifies string-only overlays use real samples and never invent size 512."""
        import plot
        small = self.root / "small"
        log = fixture(small, "small_string")
        log.write_text(log.read_text().replace(",1.0,2.0,3.0,1,128,28", ",1.0,2.5,3.0,1,256,28"))
        for directory in (self.results, small):
            run_script("analyze.py", directory / "esp32.log", "--output-dir", directory)
        results = plot.Results(self.results, small)
        name = plot.SMALL_STRING_SERIES
        self.assertNotIn(name, results.maps_for("u32_random"))
        self.assertIn(name, results.maps_for("string12", 512))
        self.assertIn(name, results.maps_for("string48", 400))
        self.assertNotIn(name, results.maps_for("string48", 512))
        for policy in ("default", "absl_hash"):
            self.assertEqual(results.get("roo", policy, "string12", 128, "find_hit"), 2.0)
            self.assertEqual(results.get(name, policy, "string12", 128, "find_hit"), 2.5)
            self.assertEqual(results.get(name, policy, "string12", 128, "find_hit", "heap_bytes"), 256)
            sizes, values = results.series(name, policy, "string48", "iteration", factor=1)
            self.assertEqual(max(sizes), 400)
            self.assertTrue(all(value == 2.5 for value in values))
        self.assertIn("separate capture/board", results.footer())

    def test_small_string_overlay_rejects_wrong_suites_and_changed_implementation(self):
        """Verifies overlays cannot silently compare different source implementations."""
        import plot
        small = self.root / "small"
        fixture(small, "small_string")
        for directory in (self.results, small):
            run_script("analyze.py", directory / "esp32.log", "--output-dir", directory)
        with self.assertRaisesRegex(ValueError, "general base"):
            plot.Results(small, self.results)
        with self.assertRaisesRegex(ValueError, "small_string source"):
            plot.Results(self.results, self.results)
        path = small / "build_metadata.json"
        metadata = json.loads(path.read_text())
        metadata["source_sha256"]["lib/roo_collections/src/hash.h"] = "d" * 64
        path.write_text(json.dumps(metadata))
        with self.assertRaisesRegex(ValueError, "source hashes differ"):
            plot.Results(self.results, small)
        metadata["compiler"] = "different compiler"
        path.write_text(json.dumps(metadata))
        with self.assertRaisesRegex(ValueError, "configuration changed: compiler"):
            plot.Results(self.results, small)

    def test_truncated_capture_is_rejected(self):
        self.log.write_text(self.log.read_text().rsplit("END,", 1)[0])
        result = run_script("analyze.py", self.log, "--output-dir", self.results, check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_invalid_integral_fields_are_rejected(self):
        self.log.write_text(self.log.read_text().replace(",1,128,28\n", ",0,128,28\n", 1))
        result = run_script("analyze.py", self.log, "--output-dir", self.results, check=False)
        self.assertNotEqual(result.returncode, 0)

    def test_bundle_roundtrip_selects_only_raw_data_and_metadata(self):
        (self.results / "original-flash.bin").write_bytes(b"private device bytes")
        (self.results / "unselected.log").write_text("not a benchmark")
        output = self.root / "run.tar.gz"
        entry = bundle.pack(self.results, [], output, "test-run")
        self.assertEqual(entry["sha256"], common.digest(output))
        with tarfile.open(output) as archive:
            self.assertEqual(set(archive.getnames()), {"bundle_manifest.json", "build_metadata.json", "raw/esp32.log"})
            manifest = json.load(archive.extractfile("bundle_manifest.json"))
            for name in ("common.py", "suites.py"):
                self.assertEqual(manifest["tooling_sha256"][name], common.digest(SCRIPTS / name))
        restored = self.root / "restored"
        bundle.unpack(output, restored, entry["sha256"])
        self.assertEqual((restored / "raw/esp32.log").read_bytes(), self.log.read_bytes())
        self.assertEqual((restored / "build_metadata.json").read_bytes(), (self.results / "build_metadata.json").read_bytes())
        self.assertTrue((restored / "summary.csv").exists())
        with self.assertRaises(ValueError):
            bundle.unpack(output, restored)
        with self.assertRaises(FileExistsError):
            bundle.pack(self.results, [], output, "test-run")

    def test_archive_checksum_and_path_traversal_are_rejected(self):
        output = self.root / "bad.tar.gz"
        with tarfile.open(output, "w:gz") as archive:
            info = tarfile.TarInfo("../outside")
            info.size = 1
            archive.addfile(info, io.BytesIO(b"x"))
        with self.assertRaises(ValueError):
            bundle.unpack(output, self.root / "restore", "0" * 64)
        with self.assertRaises(ValueError):
            bundle.unpack(output, self.root / "restore")
        self.assertFalse((self.root / "outside").exists())
        self.assertFalse((self.root / "restore").exists())

    def test_incomplete_capture_is_not_packaged(self):
        self.log.write_text(self.log.read_text().rsplit("END,", 1)[0])
        output = self.root / "run.tar.gz"
        with self.assertRaises(subprocess.CalledProcessError):
            bundle.pack(self.results, [], output, "bad-run")
        self.assertFalse(output.exists())

    def test_legacy_path_normalization_keeps_harness_integrity(self):
        updated = self.root / "updated"
        fixture(updated)
        metadata_path = self.results / "build_metadata.json"
        metadata = json.loads(metadata_path.read_text())
        metadata["source_sha256"]["prototypes/hashmap_benchmark/src/main.cpp"] = metadata["source_sha256"].pop(common.HARNESS)
        metadata_path.write_text(json.dumps(metadata))
        for directory in (self.results, updated):
            run_script("analyze.py", directory / "esp32.log", "--output-dir", directory)
        run_script("compare.py", "--baseline", self.results, "--updated", updated)
        metadata_path = updated / "build_metadata.json"
        metadata = json.loads(metadata_path.read_text())
        metadata["source_sha256"][common.HARNESS] = "f" * 64
        metadata_path.write_text(json.dumps(metadata))
        result = run_script("compare.py", "--baseline", self.results, "--updated", updated, check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Benchmark firmware source changed", result.stderr)

    def test_compilation_database_selects_harness_not_arduino_main(self):
        commands = [
            {"file": "/framework/cores/esp32/main.cpp", "command": "wrong-g++ -O0"},
            {"file": "src/main.cpp", "directory": str(build_metadata.PROJECT),
             "arguments": ["right-g++", "-O2", "-DNDEBUG"]},
        ]
        self.assertEqual(build_metadata.harness_command(commands), ["right-g++", "-O2", "-DNDEBUG"])
        with self.assertRaises(ValueError):
            build_metadata.harness_command(commands[:1])

    def test_build_receipt_rejects_source_or_image_changes(self):
        image = self.root / "firmware.bin"
        image.write_bytes(b"built image")
        fingerprint = {"source_sha256": {"header.h": "original"}}
        receipt = self.root / "receipt.json"
        receipt.write_text(json.dumps(dict(fingerprint=fingerprint, firmware_sha256=common.digest(image))))
        build_metadata.verify_receipt(fingerprint, image, receipt)
        with self.assertRaises(ValueError):
            build_metadata.verify_receipt({"source_sha256": {"header.h": "edited"}}, image, receipt)
        image.write_bytes(b"different image")
        with self.assertRaises(ValueError):
            build_metadata.verify_receipt(fingerprint, image, receipt)

    def test_dependency_checksum_is_checked_before_install(self):
        (self.root / "dependencies.json").write_text(json.dumps({"abseil": {
            "version": "20250512.1", "url": "https://example.invalid/archive", "sha256": "0" * 64,
            "prefix": "abseil-cpp-20250512.1"}}))
        with patch.object(fetch_dependencies, "PROJECT", self.root), \
                patch.dict(os.environ, {}, clear=True), \
                patch.object(fetch_dependencies, "download", side_effect=lambda url, path: Path(path).write_bytes(b"bad archive")):
            with self.assertRaisesRegex(ValueError, "Checksum mismatch"):
                fetch_dependencies.resolve("abseil")
        self.assertFalse((self.root / ".pio/dependencies/abseil-20250512.1").exists())


if __name__ == "__main__":
    unittest.main()
