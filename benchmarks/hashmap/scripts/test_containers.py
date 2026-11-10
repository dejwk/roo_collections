"""Compile/run native competitor and hash-adapter checks without an ESP32."""

import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess

from common import PROJECT, REPOSITORY
from fetch_dependencies import resolve


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default=os.environ.get("CXX", "c++"),
                        help="GCC/Clang-compatible host C++ compiler")
    parser.add_argument("--sanitize", action="store_true", help="Enable address/undefined behavior sanitizers")
    args = parser.parse_args()
    pins = json.loads((PROJECT / "dependencies.json").read_text())
    roots = {name: resolve(name) for name in pins}
    roo = Path(os.environ.get("ROO_COLLECTIONS_SOURCE_DIR", REPOSITORY)).resolve()
    includes = [PROJECT / "src", roo / "src", roots["abseil"], roots["roo_backport"] / "src"]
    includes += [root / "include" for name, root in roots.items() if name not in ("abseil", "roo_backport")]
    output = PROJECT / ".pio/tests/competitors_test"
    output.parent.mkdir(parents=True, exist_ok=True)
    command = shlex.split(args.cxx) + ["-std=c++17", "-O1", "-g", "-fno-exceptions", "-fno-rtti"]
    if args.sanitize:
        command += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    command += ["-I" + str(path) for path in includes]
    command += [str(PROJECT / "tests/competitors_test.cpp"), "-o", str(output)]
    subprocess.run(command, check=True)
    subprocess.run([str(output)], check=True)
    print("All five competitors passed native/container and explicit-hasher checks")


if __name__ == "__main__":
    main()
