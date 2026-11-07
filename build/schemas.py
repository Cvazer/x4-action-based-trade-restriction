#!/usr/bin/env python3
"""Mines the installed game's own XSD schemas into a local cache, keyed to that install.

The repo never carries a copy of the schemas: they come out of the game's own catalogs on demand,
and the cache is rebuilt whenever the game's catalog files change under it.
"""

import hashlib
import shutil
import sys
from pathlib import Path

from common import ROOT, BuildError
from x4cat import run_x4cat

CACHE = ROOT / ".schemas"
FINGERPRINT = CACHE / ".fingerprint"


def _fingerprint(game):
    catalogs = [
        f"{path.name}:{path.stat().st_size}:{int(path.stat().st_mtime)}"
        for path in sorted(game.glob("*.cat"))
    ]
    return hashlib.sha256("\n".join(catalogs).encode()).hexdigest()


def _mined_libraries(game):
    libraries = CACHE / "libraries"
    current = _fingerprint(game)
    if libraries.exists() and FINGERPRINT.exists():
        if FINGERPRINT.read_text(encoding="utf-8") == current:
            return libraries

    if CACHE.exists():
        shutil.rmtree(CACHE)
    CACHE.mkdir(parents=True)
    if not run_x4cat("extract", str(game), "-o", str(CACHE), "--include", r"^libraries/.*\.xsd$"):
        raise BuildError("could not mine the game schemas")
    FINGERPRINT.write_text(current, encoding="utf-8")
    return libraries


def schemas_dir(game):
    """The directory holding md.xsd/aiscripts.xsd for this game install."""
    unpacked = game / "libraries"
    if (unpacked / "md.xsd").exists():
        return unpacked
    return _mined_libraries(game)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit("error: mining schemas needs the path to an X4 Foundations installation")
    try:
        print(f"schemas ready at {schemas_dir(Path(sys.argv[1]))}")
    except BuildError as error:
        sys.exit(f"error: {error}")
