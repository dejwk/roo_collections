"""Check the benchmark's shared timing loop with the GNU host compiler."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


SOURCE = """
#include <cstdint>

#include "benchmark_timing.h"

struct Work {
  uint32_t& ticks;

  void operator()(uint32_t count) const { ticks += count; }
};

struct ReadCycles {
  const uint32_t& ticks;

  uint32_t operator()() const { return ticks; }
};

[[gnu::noinline]] uint32_t Calibrate(uint32_t& ticks) {
  Work work{ticks};
  return hashmap_benchmark::TimeBatch(4, work, ReadCycles{ticks});
}

[[gnu::noinline]] uint32_t Sample(uint32_t& ticks) {
  Work work{ticks};
  return hashmap_benchmark::TimeBatch(8, work, ReadCycles{ticks});
}

int main() {
  uint32_t ticks = UINT32_MAX - 6;
  if (Calibrate(ticks) != 4 || Sample(ticks) != 8) return 1;
  return ticks == 5 ? 0 : 1;
}
"""


class TimingTest(unittest.TestCase):
    @unittest.skipUnless(shutil.which("g++") and shutil.which("nm"),
                         "GNU compiler and nm required")
    def test_calibration_and_sampling_share_one_compiled_timer(self):
        """Verifies one timer survives optimization and measures through wraparound."""
        include = Path(__file__).resolve().parents[1] / "src"
        with tempfile.TemporaryDirectory() as temp:
            binary = Path(temp) / "timing"
            subprocess.run(
                ["g++", "-std=c++17", "-O2", "-Werror=attributes", "-I",
                 str(include), "-x", "c++", "-", "-o", str(binary)],
                input=SOURCE, text=True, check=True)
            subprocess.run([str(binary)], check=True)
            symbols = subprocess.check_output(
                ["nm", "-C", "--defined-only", str(binary)], text=True)
        timers = [line for line in symbols.splitlines()
                  if "hashmap_benchmark::TimeBatch<" in line]
        self.assertEqual(len(timers), 1, timers)
