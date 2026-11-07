set dotenv-load := true

# Windows recipes run under cmd.exe; every other platform keeps just's default sh.
set windows-shell := ["cmd.exe", "/c"]

# The venv from `pip install -r requirements.txt` carries just beside its own interpreter.
venv_python := if os() == "windows" { '.venv\Scripts\python.exe' } else { ".venv/bin/python" }
system_python := if os() == "windows" { "python" } else { "python3" }
python := if path_exists(venv_python) == "true" { venv_python } else { system_python }

# copy the extension source into dist/staging/ and render its templates from versions.yaml
stage:
    {{python}} build/stage.py

# compile src/dll/ and stage the built DLL inside the extension folder
dll config="Release":
    {{python}} build/dll.py "{{config}}"

# zip the staged extension into dist/
build: stage dll
    {{python}} build/build.py

# recompute versions.yaml from MOD_VERSION and the installed game, to be committed
sync-versions version=env_var_or_default("MOD_VERSION", "") game_dir=env_var_or_default("GAME_DIR", ""):
    {{python}} build/versions.py "{{version}}" "{{game_dir}}"

# mine the game's XSD schemas into the local cache
schemas game_dir=env_var_or_default("GAME_DIR", ""):
    {{python}} build/schemas.py "{{game_dir}}"

# build x4cat's index of the installed game
index game_dir=env_var_or_default("GAME_DIR", ""):
    {{python}} -m x4_catalog index "{{game_dir}}"

# run the checks in test/, narrowed to a comma-separated list of ids when one is given
test suite="" game_dir=env_var_or_default("GAME_DIR", ""): stage (schemas game_dir) (index game_dir)
    {{python}} build/test.py "{{suite}}" "{{game_dir}}"

# remove the build outputs
clean:
    {{python}} build/clean.py
