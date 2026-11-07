#!/usr/bin/env python3
"""Compile the extension's DLL and stage it inside the extension folder.

Takes the configuration to build, Release when none is given. The native project is src/dll/, and the
SDK it compiles against ships in the X4Native submodule at the project root.
"""

import shutil
import subprocess
import sys

import manifest
from common import DLL_SRC, ROOT, BuildError

SDK = ROOT / "X4Native" / "sdk"
BUILD = DLL_SRC / "build"


def _run(command):
    result = subprocess.run([str(part) for part in command], capture_output=True, text=True, check=False)
    print(result.stdout.strip() or result.stderr.strip())
    if result.returncode != 0:
        raise BuildError(f"{command[0]} failed")


def dll(configuration="Release"):
    if not (DLL_SRC / "CMakeLists.txt").is_file():
        raise BuildError(f"no native project at {DLL_SRC.relative_to(ROOT)}")
    if not (SDK / "x4native.h").is_file():
        raise BuildError(
            f"no X4Native SDK at {SDK.relative_to(ROOT)}; run `git submodule update --init`"
        )

    declared = manifest.load()
    extension = manifest.staged()

    _run([
        "cmake",
        "-S", DLL_SRC,
        "-B", BUILD,
        "-A", "x64",
        f"-DX4_MOD_ID={declared['id']}",
    ])
    _run(["cmake", "--build", BUILD, "--config", configuration])

    built = BUILD / "bin" / configuration / f"{declared['id']}.dll"
    if not built.is_file():
        raise BuildError(f"{built.relative_to(ROOT)} was not produced by the build")

    native = extension / "native"
    native.mkdir(parents=True, exist_ok=True)
    shutil.copy2(built, native / built.name)
    print(f"staged {native.relative_to(ROOT)}/{built.name} as {configuration}")
    return native / built.name


if __name__ == "__main__":
    args = sys.argv[1:]
    try:
        dll(args[0] if args else "Release")
    except BuildError as error:
        sys.exit(f"error: {error}")
