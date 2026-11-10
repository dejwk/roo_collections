"""Small shared helpers for benchmark sources and portable archives."""

import hashlib
from pathlib import Path, PurePosixPath
import shutil
import tarfile

PROJECT = Path(__file__).resolve().parents[1]
REPOSITORY = PROJECT.parents[1]
HARNESS = "benchmarks/hashmap/src/main.cpp"


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def normalize_sources(sources):
    result = {}
    for name, value in sources.items():
        key = HARNESS if name == "prototypes/hashmap_benchmark/src/main.cpp" else name
        if key in result and result[key] != value:
            raise ValueError(f"Conflicting source hashes: {key}")
        result[key] = value
    return result


def extract_files(archive, output, prefix=None):
    """Extract regular files and directories, rejecting links and traversal."""
    output = Path(output)
    with tarfile.open(archive, "r:gz") as stream:
        seen = set()
        for member in stream:
            path = PurePosixPath(member.name)
            if path.is_absolute() or ".." in path.parts or "\\" in member.name:
                raise ValueError(f"Unsafe archive path: {member.name}")
            if prefix is not None:
                if not path.parts or path.parts[0] != prefix:
                    raise ValueError("Unexpected source archive root")
                path = PurePosixPath(*path.parts[1:])
            if str(path) == ".":
                continue
            if path in seen or not (member.isdir() or member.isfile()):
                raise ValueError(f"Unsupported archive member: {member.name}")
            seen.add(path)
            target = output.joinpath(*path.parts)
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                with stream.extractfile(member) as source, target.open("xb") as dest:
                    shutil.copyfileobj(source, dest)


def check_url(url):
    from urllib.parse import urlsplit
    if urlsplit(url).scheme != "https":
        raise ValueError("Downloads must use HTTPS")


def download(url, target):
    from urllib.request import Request, urlopen
    check_url(url)
    request = Request(url, headers={"User-Agent": "roo_collections-hashmap-benchmark"})
    with urlopen(request, timeout=60) as source, Path(target).open("wb") as output:
        shutil.copyfileobj(source, output)


def source_fingerprint(context):
    """Identify measured library/harness bytes and the entire Abseil source tree."""
    sources = {"benchmarks/hashmap/" + path.relative_to(PROJECT).as_posix(): path
               for path in sorted((PROJECT / "src").rglob("*")) if path.is_file()}
    for name, value in context["library_source_roots"].items():
        root = Path(value)
        for path in sorted((root / "src").rglob("*")):
            if path.is_file():
                sources[f"lib/{name}/{path.relative_to(root).as_posix()}"] = path
    absl = Path(context["abseil_root"])
    for relative in context["abseil_sources"] + [
            "absl/container/internal/raw_hash_set_benchmark.cc",
            "absl/container/internal/unordered_map_lookup_test.h"]:
        sources[relative] = absl / relative
    tree = hashlib.sha256()
    for path in sorted((absl / "absl").rglob("*")):
        if path.is_file() and path.suffix in (".h", ".inc", ".cc", ".c", ".S"):
            tree.update(path.relative_to(absl).as_posix().encode() + b"\0" + bytes.fromhex(digest(path)))
    content = {"abseil": tree.hexdigest()}
    for name, value in context.get("benchmark_dependency_roots", {}).items():
        root = Path(value) / "include"
        tree = hashlib.sha256()
        for path in sorted(root.rglob("*")):
            if path.is_file():
                tree.update(path.relative_to(root).as_posix().encode() + b"\0" + bytes.fromhex(digest(path)))
        content[name] = tree.hexdigest()
    configurations = {path.name: digest(path) for path in [PROJECT / "platformio.ini", PROJECT / "dependencies.json", *sorted(PROJECT.glob("platformio.local*.ini"))] if path.exists()}
    return {"configuration_sha256": configurations,
            "source_sha256": {name: digest(path) for name, path in sources.items()},
            "dependency_content_sha256": content}
