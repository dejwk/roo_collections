"""Post-builder hook: bind the completed firmware to the selected source bytes."""

import json
from pathlib import Path
import sys

Import("env")
project = Path(env["PROJECT_DIR"])
sys.path.insert(0, str(project / "scripts"))
from common import digest, source_fingerprint

context = json.loads((project / ".pio/build_context.json").read_text())

def record_firmware(source, target, env):
    final = source_fingerprint(context)
    if final != context["source_fingerprint"]:
        raise ValueError("Benchmark sources or configuration changed during compilation; rebuild")
    image = Path(env.subst("$BUILD_DIR")) / "firmware.bin"
    receipt = dict(firmware_sha256=digest(image), fingerprint=final)
    (project / ".pio/build_receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")

# Register after the platform builder creates the alias; a pre-script would
# attach to a file node named buildprog instead of the actual alias.
target = env.Alias("buildprog")
env.AddPostAction(target, record_firmware)
env.AlwaysBuild(target)
