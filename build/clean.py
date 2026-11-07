#!/usr/bin/env python3
"""Remove the build outputs."""

import shutil

from common import DIST, ROOT


def clean():
    if DIST.exists():
        shutil.rmtree(DIST)
        print(f"removed {DIST.relative_to(ROOT)}")
    else:
        print("nothing to clean")


if __name__ == "__main__":
    clean()
