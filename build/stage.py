#!/usr/bin/env python3
"""Copy the extension source into dist/staging/ and render its templates.

The checks and the zip both work on the result. The folder is rebuilt from nothing each run.
"""

import shutil
import sys

from jinja2 import StrictUndefined, Template

import manifest
import versions
from common import EXTENSION_SRC, ROOT, STAGING, BuildError

SUFFIX = ".j2"


def stage():
    if not EXTENSION_SRC.is_dir():
        raise BuildError(f"no extension source at {EXTENSION_SRC}")

    declared = manifest.load()
    context = {**declared, **versions.load()}
    extension = STAGING / declared["folder"]

    if STAGING.exists():
        shutil.rmtree(STAGING)
    STAGING.mkdir(parents=True)
    shutil.copytree(EXTENSION_SRC, extension)

    rendered = 0
    for template in sorted(extension.rglob(f"*{SUFFIX}")):
        text = Template(template.read_text(encoding="utf-8"), undefined=StrictUndefined).render(context)
        template.with_suffix("").write_text(text, encoding="utf-8")
        template.unlink()
        rendered += 1

    print(f"staged {extension.relative_to(ROOT)}, {rendered} template(s) rendered")
    return extension


if __name__ == "__main__":
    try:
        stage()
    except BuildError as error:
        sys.exit(f"error: {error}")
