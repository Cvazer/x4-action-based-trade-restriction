import os
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

staged = Path(os.environ["X4_STAGED"])

for path in sorted(staged.rglob("*.xml")):
    try:
        ET.parse(path)
    except ET.ParseError as error:
        sys.exit(f"{path.relative_to(staged)} is not well formed: {error}")

print(f"{len(list(staged.rglob('*.xml')))} XML file(s) well formed")
