"""Fetch checksum-pinned benchmark-only dependencies into ignored local storage."""

import json
import os
from pathlib import Path
import shutil
import tempfile

from common import PROJECT, digest, download, extract_files

OVERRIDES = {"abseil": "ABSL_SOURCE_DIR", "roo_backport": "ROO_BACKPORT_SOURCE_DIR"}


def validate(name, source, pin):
    if name == "abseil":
        text = (source / "absl/base/config.h").read_text()
        major, patch = pin["version"].split(".")
        if (f"#define ABSL_LTS_RELEASE_VERSION {major}" not in text or
                f"#define ABSL_LTS_RELEASE_PATCH_LEVEL {patch}" not in text):
            raise ValueError("Abseil source override has the wrong release")
    else:
        info = json.loads((source / "library.json").read_text())
        if info["version"] != pin["version"]:
            raise ValueError("roo_backport source override has the wrong version")


def resolve(name):
    pin = json.loads((PROJECT / "dependencies.json").read_text())[name]
    override = os.environ.get(OVERRIDES[name])
    if override:
        source = Path(override).expanduser().resolve()
        validate(name, source, pin)
        return source
    cache = PROJECT / ".pio/dependencies"
    cache.mkdir(parents=True, exist_ok=True)
    target = cache / f"{name}-{pin['version']}"
    marker = target / ".archive-sha256"
    if marker.exists() and marker.read_text().strip() == pin["sha256"]:
        validate(name, target, pin)
        return target
    if target.exists():
        raise ValueError(f"Unrecognized dependency directory; remove it or set {OVERRIDES[name]}: {target}")
    with tempfile.TemporaryDirectory(prefix=name + "-", dir=cache) as directory:
        temp = Path(directory)
        archive = temp / "source.tar.gz"
        print(f"Fetching {name} {pin['version']}", flush=True)
        download(pin["url"], archive)
        if digest(archive) != pin["sha256"]:
            raise ValueError(f"Checksum mismatch for {name}")
        source = temp / "source"
        source.mkdir()
        extract_files(archive, source, pin["prefix"])
        validate(name, source, pin)
        (source / ".archive-sha256").write_text(pin["sha256"] + "\n")
        shutil.move(source, target)
    return target


def main():
    for name in OVERRIDES:
        print(f"{name}: {resolve(name)}")


if __name__ == "__main__":
    main()
