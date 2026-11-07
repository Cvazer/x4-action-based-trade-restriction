"""The extension's identity, as declared in mod.yaml."""

import yaml

from common import ROOT, STAGING, BuildError

MANIFEST = ROOT / "mod.yaml"
FIELDS = ("id", "name", "description", "author", "folder", "archive")


def load():
    if not MANIFEST.exists():
        raise BuildError(f"no {MANIFEST.name} at {ROOT}")
    declared = yaml.safe_load(MANIFEST.read_text(encoding="utf-8")) or {}
    missing = [field for field in FIELDS if not declared.get(field)]
    if missing:
        raise BuildError(f"{MANIFEST.name} declares no {', '.join(missing)}")
    return declared


def staged():
    """The extension folder inside the staging area."""
    return STAGING / load()["folder"]
