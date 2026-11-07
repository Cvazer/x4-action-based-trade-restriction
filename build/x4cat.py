"""Runs x4cat, wherever pip installed it."""

import shutil
import subprocess
import sys
from pathlib import Path

from common import BuildError


def _executable():
    """pip installs x4cat's console script beside the interpreter running it, in the same venv's
    Scripts/bin folder; that is checked before falling back to the caller's own PATH."""
    name = "x4cat.exe" if sys.platform == "win32" else "x4cat"
    beside_interpreter = Path(sys.executable).parent / name
    if beside_interpreter.exists():
        return str(beside_interpreter)
    on_path = shutil.which("x4cat")
    if on_path:
        return on_path
    raise BuildError("x4cat is not installed; install -r requirements.txt into the venv running this")


def run_x4cat(*args):
    result = subprocess.run(
        [_executable(), *args], capture_output=True, text=True, check=False
    )
    print(result.stdout.strip() or result.stderr.strip())
    return result.returncode == 0
