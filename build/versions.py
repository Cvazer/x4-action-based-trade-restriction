#!/usr/bin/env python3
"""Recompute versions.yaml from the mod version given and the installed game.

Takes the mod version, X.Y.Z[-suffix], and the path to an X4 installation. Every other task reads
the committed file instead of either source.
"""

import datetime
import re
import sys
from pathlib import Path

import yaml

from common import ROOT, BuildError

VERSIONS = ROOT / "versions.yaml"
HEADER = "# Written by `just sync-versions`; edits here are overwritten.\n"

# X.Y.Z with an optional suffix that contains no whitespace.
VERSION_PATTERN = re.compile(r"^(\d+)\.(\d+)\.(\d+)(?:-(\S+))?$")


def parse_version(version):
    """Validate a release version and return its numeric part. Any suffix is accepted and ignored;
    the game's version field holds numbers only."""
    match = VERSION_PATTERN.match(version)
    if not match:
        raise BuildError(
            f"version {version!r} is not X.Y.Z with an optional suffix containing no spaces"
        )
    major, minor, patch, _ = match.groups()
    return int(major), int(minor), int(patch)


def content_version(version):
    """content.xml carries the mod's version as an integer the game shows divided by 100: 1.2.3
    becomes 123, shown as 1.23. Minor and patch each occupy one digit, so neither may exceed 9."""
    major, minor, patch = parse_version(version)
    if minor > 9 or patch > 9:
        raise BuildError(
            f"version {major}.{minor}.{patch} does not fit the game's version field: "
            "minor and patch must each be 9 or less"
        )
    return major * 100 + minor * 10 + patch


def load():
    """The committed versions, as the rendering context."""
    if not VERSIONS.exists():
        raise BuildError(f"no {VERSIONS.name}; run `just sync-versions` against an install")
    declared = yaml.safe_load(VERSIONS.read_text(encoding="utf-8")) or {}
    missing = [field for field in ("mod", "game", "date") if not declared.get(field)]
    if missing:
        raise BuildError(f"{VERSIONS.name} declares no {', '.join(missing)}")
    return {
        "mod_version": str(declared["mod"]),
        "content_version": content_version(str(declared["mod"])),
        "game_version": str(declared["game"]),
        "date": str(declared["date"]),
    }


def installed_version(game):
    """The installed game's own version, in the integer form content.xml carries."""
    declared = game / "version.dat"
    if not declared.exists():
        raise BuildError(f"{game} has no version.dat; it does not look like an X4 installation")
    number = declared.read_text(encoding="utf-8").strip()
    if not number.isdigit():
        raise BuildError(f"{declared} holds no version number: {number!r}")
    return number


def sync(version, game_dir):
    if not version:
        raise BuildError("no mod version: set MOD_VERSION in .env, or pass one")
    if not game_dir:
        raise BuildError("no game directory: set GAME_DIR in .env, or pass one")
    content_version(version)
    game = installed_version(Path(game_dir))

    released = datetime.date.today().isoformat()

    declared = yaml.safe_dump(
        {"mod": version, "game": int(game), "date": released}, sort_keys=True
    )
    VERSIONS.write_text(HEADER + declared, encoding="utf-8")
    print(f"{VERSIONS.name}: mod {version}, game {game}, date {released}")


if __name__ == "__main__":
    args = sys.argv[1:]
    try:
        sync(args[0] if args else "", args[1] if len(args) > 1 else "")
    except BuildError as error:
        sys.exit(f"error: {error}")
