"""Run the on-device suite and retain its complete serial transcript."""
import argparse
from pathlib import Path
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--runs", type=int, default=1)
    parser.add_argument("--timeout", type=float, default=1800)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    connection = serial.Serial(port=None, baudrate=115200, timeout=1)
    connection.dtr = False
    connection.rts = False
    connection.port = args.port
    connection.open()
    started = time.monotonic()
    last_progress = started
    rows = completed = 0
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with connection, args.output.open("x", buffering=1) as output:
        time.sleep(2)
        connection.reset_input_buffer()
        connection.write(b"r")
        while time.monotonic() - started < args.timeout:
            line = connection.readline().decode("utf-8", errors="replace")
            if line:
                output.write(line)
                if line.startswith("ROW,"):
                    rows += 1
                if any(word in line for word in ("FAIL,", "Guru Meditation", "CORRUPT HEAP", "abort()")):
                    raise RuntimeError("Board failure: " + line.strip())
                if line.startswith(("BEGIN,", "END,")):
                    print(line.strip(), flush=True)
                if line.startswith("END,"):
                    completed += 1
                    if completed == args.runs:
                        print(f"Captured {rows} rows in {completed} complete runs", flush=True)
                        return
                    connection.write(b"r")
            now = time.monotonic()
            if now - last_progress >= 15:
                print(f"{now - started:.0f}s: {rows} rows, {completed} runs; "
                      f"last={line.strip()[:110]}", flush=True)
                last_progress = now
    raise TimeoutError(f"Only {completed}/{args.runs} runs completed; transcript: {args.output}")


if __name__ == "__main__":
    main()
