#!/usr/bin/env python3
"""Run the checks in test/.

Takes a comma-separated list of check ids to narrow the run to, and the path to an X4 installation.
Without a list, every check runs.
"""

import os
import shlex
import shutil
import string
import subprocess
import sys
from pathlib import Path

import yaml

import manifest
from common import ROOT, BuildError
from schemas import schemas_dir

TESTS = ROOT / "test"


def discover():
    """A directory under test/ holding a test.yaml is one check."""
    found = []
    for manifest_file in sorted(TESTS.glob("*/test.yaml")):
        described = yaml.safe_load(manifest_file.read_text(encoding="utf-8")) or {}
        missing = [field for field in ("id", "command") if not described.get(field)]
        if missing:
            raise BuildError(f"{manifest_file.relative_to(ROOT)} declares no {', '.join(missing)}")
        found.append((described, manifest_file.parent))
    return found


def selected(found, suite):
    if not suite:
        return found
    wanted = [name.strip() for name in suite.split(",") if name.strip()]
    known = {described["id"] for described, _ in found}
    unknown = [name for name in wanted if name not in known]
    if unknown:
        raise BuildError(
            f"no such check: {', '.join(unknown)}; test/ holds {', '.join(sorted(known))}"
        )
    return [(described, folder) for described, folder in found if described["id"] in wanted]


def environment(game_dir):
    """What every check is given: the staged extension, the game, the mined schemas, and a PATH
    carrying the interpreter's own Scripts/bin folder.

    The schema cache and x4cat's index are built by `just schemas` and `just index`, which
    `just test` runs first.
    """
    env = dict(os.environ)
    env["PATH"] = os.pathsep.join([str(Path(sys.executable).parent), env.get("PATH", "")])
    env["X4_STAGED"] = str(manifest.staged())
    env["X4_GAME_DIR"] = game_dir
    env["X4_SCHEMAS"] = str(schemas_dir(Path(game_dir))) if game_dir else ""
    return env


def run(suite, game_dir):
    staged = manifest.staged()
    if not staged.is_dir():
        raise BuildError(f"nothing staged at {staged}; run `just stage` first")

    chosen = selected(discover(), suite)
    if not chosen:
        raise BuildError(f"no checks in {TESTS.relative_to(ROOT)}")

    env = environment(game_dir)
    failed = []
    for described, folder in chosen:
        print(f"--- {described['id']}", flush=True)
        try:
            expanded = string.Template(described["command"]).substitute(env)
        except KeyError as unset:
            raise BuildError(f"{described['id']} uses {unset}, which is not set")
        command = shlex.split(expanded)
        # Windows resolves a child's executable against this process's own PATH, not the one handed
        # to it.
        executable = shutil.which(command[0], path=env["PATH"])
        if executable is None:
            raise BuildError(f"{described['id']} runs {command[0]}, which is not installed")
        if subprocess.run([executable, *command[1:]], cwd=folder, env=env).returncode != 0:
            failed.append(described["id"])

    if failed:
        raise BuildError("failed: " + ", ".join(failed))
    print(f"{len(chosen)} check(s) passed")


if __name__ == "__main__":
    args = sys.argv[1:]
    try:
        run(args[0] if args else "", args[1] if len(args) > 1 else "")
    except BuildError as error:
        sys.exit(f"error: {error}")
