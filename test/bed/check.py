"""Build the bed and run it.

The bed is a program: it builds once, reads its tests and their parameters, and runs them. This
script builds that program and runs its three executables.
"""

from __future__ import annotations

import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent
PROJECT = ROOT.parent.parent

BED = (
    ROOT / "build" / "Release" / "bed.exe",
    ROOT / "build" / "bed.exe",
)
INTEGRATION = (
    ROOT / "build" / "Release" / "integration.exe",
    ROOT / "build" / "integration.exe",
)
LUA_HOST = (
    ROOT / "build" / "Release" / "lua_host.exe",
    ROOT / "build" / "lua_host.exe",
)


def game_dir() -> Path:
    """The installed game, from `.env`, so the Lua runtime is the one the mod runs against."""
    env = PROJECT / ".env"
    if env.is_file():
        for line in env.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if line.startswith("GAME_DIR"):
                return Path(line.split("=", 1)[1].strip().strip('"'))
    raise SystemExit(f"no GAME_DIR in {env}; the game location comes from .env")


def rebuild() -> None:
    build = ROOT / "build"
    if not (build / "CMakeCache.txt").is_file():
        configure = subprocess.run(["cmake", "-S", str(ROOT), "-B", str(build)],
                                   capture_output=True, text=True)
        if configure.returncode != 0:
            raise SystemExit("the bed does not configure:\n" + configure.stdout + configure.stderr)
    result = subprocess.run(["cmake", "--build", str(build), "--config", "Release"],
                            capture_output=True, text=True)
    if result.returncode != 0:
        raise SystemExit("the bed does not build:\n" + result.stdout + result.stderr)


def executable(candidates: tuple[Path, ...], what: str) -> Path:
    for path in candidates:
        if path.is_file():
            return path
    raise SystemExit(f"{what} produced no executable at " + ", ".join(str(p) for p in candidates))


def run(binary: Path, *arguments: str) -> subprocess.CompletedProcess:
    return subprocess.run([str(binary), *arguments], capture_output=True, text=True)


def main() -> int:
    problems: list[str] = []

    rebuild()

    bed = executable(BED, "the decision tests")
    result = run(bed)
    if result.returncode != 0:
        failed = [line for line in result.stdout.splitlines() if " FAIL " in line]
        problems.append(f"the decision tests are not green: {len(failed)} case(s) fail")
        problems.extend(f"  {line}" for line in failed)

    engine = executable(INTEGRATION, "the engine tests")
    result = run(engine)
    if result.returncode != 0:
        failed = [line for line in result.stdout.splitlines() if line.startswith("BED FAIL")]
        problems.append(f"the engine tests are not green: {len(failed)} check(s) fail")
        problems.extend(f"  {line}" for line in failed)

    host = executable(LUA_HOST, "the editor test")
    ui_xml = PROJECT / "src" / "extension" / "ui.xml"
    editor = ROOT / "tests" / "editor"
    runtime = game_dir() / "lua51_64.dll"
    if not runtime.is_file():
        problems.append(f"no Lua runtime at {runtime}")
    else:
        result = run(host, str(runtime), str(ui_xml), str(editor / "surface.lua"),
                     str(editor / "editor_test.lua"))
        if result.returncode != 0:
            failed = [line for line in result.stdout.splitlines() if line.startswith("BED FAIL")]
            problems.append(f"the editor test is not green: {len(failed)} check(s) fail")
            problems.extend(f"  {line}" for line in failed)

    if problems:
        print()
        for problem in problems:
            print(f"FAIL {problem}")
        return 1

    print("the decision, the engine and the editor tests are green")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
