"""Read/verify ESP32 flash with one in-flight block (USBIP-friendly).

Uses esptool's documented stub READ_FLASH command with a window of 1 rather
than its default 64. Checks both the returned digest and a separate full-flash
MD5. This changes host transfer pacing only; it does not write device flash.
"""
import argparse
import hashlib
from pathlib import Path
import struct
import time

import esptool


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--size", type=lambda s: int(s, 0), default=0x400000)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    chip = esptool.cmds.detect_chip(args.port, 115200)
    stub = chip.run_stub()
    if args.baud != 115200:
        stub.change_baud(args.baud)
    stub.flash_set_parameters(args.size)
    block = 256
    segment = 65536
    partial = args.output.with_suffix(".partial")
    data = bytearray(partial.read_bytes() if partial.exists() else b"")
    if len(data) % segment or len(data) > args.size:
        raise RuntimeError("Unexpected partial backup size")
    while len(data) < args.size:
        offset = len(data)
        length = min(segment, args.size - offset)
        for attempt in range(10):
            try:
                stub.check_command("read flash", stub.ESP_CMDS["READ_FLASH"],
                                   struct.pack("<IIII", offset, length, block, 1))
                chunk = bytearray()
                for received in range(0, length, block):
                    stub._port.timeout = 5
                    packet = stub.read()
                    if len(packet) != min(block, length - received):
                        raise RuntimeError(f"Truncated packet: {len(packet)}")
                    chunk.extend(packet)
                    stub.write(struct.pack("<I", received + len(packet)))
                remote = stub.read().hex()
                if hashlib.md5(chunk).hexdigest() != remote:
                    raise RuntimeError("Segment digest mismatch")
                data.extend(chunk)
                partial.write_bytes(data)
                print(f"Verified {len(data)}/{args.size} bytes", flush=True)
                break
            except (esptool.FatalError, RuntimeError, StopIteration) as error:
                print(f"Retry {attempt + 1} at {offset:#x}: {error}", flush=True)
                stub._port.close()
                time.sleep(0.5)
                chip = esptool.cmds.detect_chip(args.port, 115200)
                stub = chip.run_stub()
                if args.baud != 115200:
                    stub.change_baud(args.baud)
                stub.flash_set_parameters(args.size)
        else:
            raise RuntimeError(f"Unable to read segment at {offset:#x}")
    digest = hashlib.md5(data).hexdigest()
    remote = stub.flash_md5sum(0, args.size).lower()
    if digest != remote:
        raise RuntimeError(f"Flash digest mismatch: {digest} != {remote}")
    args.output.write_bytes(data)
    partial.unlink()
    print(f"VERIFIED {args.output}: {len(data)} bytes, MD5 {digest}", flush=True)
    stub.hard_reset()
    stub._port.close()


if __name__ == "__main__":
    main()
