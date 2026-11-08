"""PlatformIO wiring for the repository under test and pinned benchmark dependencies."""

import json
import os
from pathlib import Path
import sys

Import("env")
project = Path(env["PROJECT_DIR"])
sys.path.insert(0, str(project / "scripts"))
from common import REPOSITORY, digest, source_fingerprint
from fetch_dependencies import resolve

roo = Path(os.environ.get("ROO_COLLECTIONS_SOURCE_DIR", REPOSITORY)).expanduser().resolve()
roots = {"roo_collections": roo, "roo_backport": resolve("roo_backport")}
absl = resolve("abseil")
linked = project / ".pio/linked-libs"
linked.mkdir(parents=True, exist_ok=True)
for name, source in roots.items():
    if not (source / "src").is_dir():
        raise ValueError(f"Invalid source directory: {source}")
    link = linked / name
    if link.is_symlink() and link.resolve() != source:
        link.unlink()
    if not link.exists():
        link.symlink_to(source, target_is_directory=True)
env.Append(CPPPATH=[str(absl)])
sources = [
    "absl/container/internal/raw_hash_set.cc",
    "absl/container/internal/hashtablez_sampler.cc",
    "absl/hash/internal/hash.cc", "absl/hash/internal/city.cc",
    "absl/hash/internal/low_level_hash.cc", "absl/base/internal/raw_logging.cc",
    "absl/base/internal/throw_delegate.cc", "absl/base/log_severity.cc",
]
generated = project / ".pio/absl-src"
generated.mkdir(parents=True, exist_ok=True)
for relative in sources:
    link = generated / relative.replace("/", "_")
    if link.is_symlink() and link.resolve() != absl / relative:
        link.unlink()
    if not link.exists():
        link.symlink_to(absl / relative)
env.BuildSources(str(project / ".pio/absl-obj"), str(generated))
platform = env.PioPlatform()
context = {
    "library_source_roots": {name: str(path) for name, path in roots.items()},
    "abseil_root": str(absl), "abseil_sources": sources,
    "board": env.GetProjectOption("board"),
    "platform": "pioarduino espressif32 " + platform.version,
    "compiler_root": platform.get_package_dir("toolchain-xtensa-esp-elf"),
    "framework_root": platform.get_package_dir("framework-arduinoespressif32"),
    "framework_libraries_root": platform.get_package_dir("framework-arduinoespressif32-libs"),
}
context["source_fingerprint"] = source_fingerprint(context)
(project / ".pio/build_context.json").write_text(json.dumps(context, indent=2) + "\n")
# Symlink target changes and copied old timestamps must not reuse an old harness
# object. Only the benchmark translation unit is forced to rebuild.
(Path(env.subst("$BUILD_DIR")) / "src/main.cpp.o").unlink(missing_ok=True)

