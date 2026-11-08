"""Correctness checks only: host execution is not a 68000 speed benchmark."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[1]
source = root / "ports/macintosh"
out = root / "build/host-tests"
out.mkdir(parents=True, exist_ok=True)
cases = {"air_inset": [], "deadline_clock": [], "camera": [], "frame_stats": [],
         "frame_diagnostics": [], "hud_format": [], "controls": [],
         "win32_controls": [], "scores": [], "terrain": [], "courses": ["sim.c"]}
for name, extra in cases.items():
    exe = out / name
    subprocess.run(["clang", "-O1", "-fwrapv", "-fno-strict-aliasing",
                    "-fsanitize=address,undefined", str(source / "tests" / (name + ".c")),
                    *[str(source / item) for item in extra], "-o", str(exe)], check=True)
    subprocess.run([str(exe)], cwd=out, check=True)
