"""Package validated raw runs for release assets; restore results and regenerate tables."""

import argparse
import io
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile

from common import HARNESS, PROJECT, check_url, digest, download, extract_files, normalize_sources


def analyze(logs, output):
    subprocess.run([sys.executable, str(PROJECT / "scripts/analyze.py"), *map(str, logs),
                    "--output-dir", str(output)], check=True)


def pack(results, logs, output, run_id, url=None):
    if re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", run_id) is None:
        raise ValueError("Run ID must contain only letters, digits, dots, underscores or hyphens")
    if url is not None:
        check_url(url)
    metadata = results / "build_metadata.json"
    build = json.loads(metadata.read_text())
    if HARNESS not in normalize_sources(build["source_sha256"]):
        raise ValueError("Missing measured harness hash")
    logs = list(logs) or [results / "esp32.log"]
    if len({path.name for path in logs}) != len(logs):
        raise ValueError("Log filenames must be unique")
    if any(path.suffix != ".log" for path in logs):
        raise ValueError("Only explicitly selected .log transcripts are accepted")
    output.parent.mkdir(parents=True, exist_ok=True)
    (PROJECT / ".pio").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="bundle-", dir=PROJECT / ".pio") as directory:
        temp = Path(directory)
        validation = temp / "validation"
        files = {"build_metadata.json": metadata}
        files.update({"raw/" + path.name: path for path in logs})
        # Freeze the selected bytes before hashing/archiving; outputs and device
        # flash backups are never selected by recursively walking a run folder.
        frozen = temp / "files"
        for name, path in files.items():
            target = frozen / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, target)
        analyze([frozen / "raw" / path.name for path in logs], validation)
        manifest = dict(schema_version=1, id=run_id, raw_logs=["raw/" + p.name for p in logs],
                        files={name: digest(frozen / name) for name in files},
                        tooling_sha256={name: digest(PROJECT / "scripts" / name)
                                        for name in ("analyze.py", "compare.py", "plot.py")})
        data = (json.dumps(manifest, indent=2) + "\n").encode()
        with output.open("xb") as dest, tarfile.open(fileobj=dest, mode="w:gz") as archive:
            info = tarfile.TarInfo("bundle_manifest.json")
            info.size = len(data)
            archive.addfile(info, io.BytesIO(data))
            for name in sorted(files):
                archive.add(frozen / name, arcname=name, recursive=False)
    entry = dict(id=run_id, sha256=digest(output), url=url,
                 roo_collections_commit=build["roo_collections_commit"],
                 board=build.get("board"), cpu_mhz=build["cpu_mhz"])
    entry_path = output.with_name(output.name + ".index.json")
    entry_path.write_text(json.dumps(entry, indent=2) + "\n")
    return entry


def unpack(archive, output, expected_sha256=None):
    if expected_sha256 is not None and digest(archive) != expected_sha256:
        raise ValueError("Result archive checksum mismatch")
    if output.exists():
        raise ValueError("Refusing to overwrite an existing results directory")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="unpack-", dir=output.parent) as directory:
        temp = Path(directory)
        extract_files(archive, temp)
        manifest = json.loads((temp / "bundle_manifest.json").read_text())
        if manifest["schema_version"] != 1:
            raise ValueError("Unsupported result bundle version")
        names = set(manifest["files"])
        if "build_metadata.json" not in names or not manifest["raw_logs"]:
            raise ValueError("Bundle has no build metadata or raw logs")
        if names != {"build_metadata.json", *manifest["raw_logs"]}:
            raise ValueError("Bundle contains unexpected selected files")
        actual = {p.relative_to(temp).as_posix() for p in temp.rglob("*") if p.is_file()}
        if actual != names | {"bundle_manifest.json"}:
            raise ValueError("Bundle inventory does not match extracted files")
        for name, expected in manifest["files"].items():
            if digest(temp / name) != expected:
                raise ValueError(f"Bundled file checksum mismatch: {name}")
        analyze([temp / name for name in manifest["raw_logs"]], temp)
        temp.rename(output)
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    create = commands.add_parser("pack")
    create.add_argument("--results", type=Path, required=True)
    create.add_argument("--log", type=Path, action="append", default=[])
    create.add_argument("--id", required=True)
    create.add_argument("--output", type=Path, required=True)
    create.add_argument("--url", help="Prospective HTTPS release-asset URL for the generated index entry")
    restore = commands.add_parser("unpack")
    restore.add_argument("archive", type=Path)
    restore.add_argument("--output", type=Path, required=True)
    restore.add_argument("--sha256")
    fetch = commands.add_parser("fetch")
    fetch.add_argument("id")
    fetch.add_argument("--output", type=Path, required=True)
    fetch.add_argument("--index", type=Path, default=PROJECT / "results-index.json")
    args = parser.parse_args()
    (PROJECT / ".pio").mkdir(exist_ok=True)
    if args.command == "pack":
        print(json.dumps(pack(args.results, args.log, args.output, args.id, args.url), indent=2))
    elif args.command == "unpack":
        unpack(args.archive, args.output, args.sha256)
    else:
        index = json.loads(args.index.read_text())
        if index["schema_version"] != 1:
            raise ValueError("Unsupported results index version")
        matches = [entry for entry in index["runs"] if entry["id"] == args.id]
        if len(matches) != 1:
            raise ValueError("Run ID is missing or duplicated in results index")
        entry = matches[0]
        with tempfile.TemporaryDirectory(dir=PROJECT / ".pio") as directory:
            archive = Path(directory) / "run.tar.gz"
            download(entry["url"], archive)
            unpack(archive, args.output, entry["sha256"])


if __name__ == "__main__":
    main()
