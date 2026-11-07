#!/usr/bin/env python3
"""Zip the staged extension into dist/."""

import sys
import zipfile

import manifest
import versions
from common import DIST, ROOT, STAGING, BuildError


def build():
    if not STAGING.is_dir():
        raise BuildError(f"nothing staged at {STAGING}; run `just stage` first")

    declared = manifest.load()
    released = versions.load()
    DIST.mkdir(exist_ok=True)
    archive = DIST / f"{declared['archive']}.zip"

    # The archive root holds the extension folder.
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as zipped:
        for path in sorted(STAGING.rglob("*")):
            if path.is_file():
                zipped.write(path, path.relative_to(STAGING))

    print(
        f"built {archive.relative_to(ROOT)} as {released['mod_version']}, "
        f"for game {released['game_version']}"
    )
    return archive


if __name__ == "__main__":
    try:
        build()
    except BuildError as error:
        sys.exit(f"error: {error}")
