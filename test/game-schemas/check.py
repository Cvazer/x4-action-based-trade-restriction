import os
import sys
from pathlib import Path

from lxml import etree

staged = Path(os.environ["X4_STAGED"])
mined = os.environ.get("X4_SCHEMAS")
if not mined:
    sys.exit("no mined schemas: set GAME_DIR in .env, or pass a game directory")
schemas = Path(mined)

failures = []
checked = 0
for folder, schema_name in (("md", "md.xsd"), ("aiscripts", "aiscripts.xsd")):
    schema_file = schemas / schema_name
    if not (staged / folder).is_dir() or not schema_file.exists():
        continue
    schema = etree.XMLSchema(etree.parse(str(schema_file)))
    diff_file = schemas / "diff.xsd"
    diff_schema = (
        etree.XMLSchema(etree.parse(str(diff_file))) if diff_file.exists() else None
    )
    for path in sorted((staged / folder).glob("*.xml")):
        document = etree.parse(str(path))
        against = schema
        if document.getroot().tag == "diff":
            if diff_schema is None:
                continue
            against = diff_schema
        checked += 1
        if not against.validate(document):
            for error in against.error_log:
                failures.append(f"{path.relative_to(staged)}:{error.line}: {error.message}")

if failures:
    sys.exit("schema validation failed:\n  " + "\n  ".join(failures))

print(f"{checked} script(s) validate against the game schemas")
