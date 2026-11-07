"""Shared locations for the build scripts."""

from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"
EXTENSION_SRC = SRC / "extension"
DLL_SRC = SRC / "dll"
DIST = ROOT / "dist"
STAGING = DIST / "staging"


class BuildError(Exception):
    """A task could not complete. The message is shown to the user without a traceback."""
