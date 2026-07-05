#!/usr/bin/env python3
"""Extract the runtime environment from the Conan-generated CMake test preset
into a plain KEY=VALUE file that VS Code launch configs (CodeLLDB `envFile`) and
tasks can consume.

The paths point into the Conan cache and change whenever dependencies are
re-installed, so re-run this after every `conan install` (there is a VS Code task
"env: regenerate .vscode/.env" that does exactly this).

Usage: scripts/gen-vscode-env.py [BUILD_DIR]
    BUILD_DIR defaults to cmake-build-relwithdebinfo.
"""
import json
import os
import re
import sys
from pathlib import Path

repo = Path(__file__).resolve().parent.parent
build_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else repo / "cmake-build-relwithdebinfo"
preset = build_dir / "CMakePresets.json"

if not preset.is_file():
    sys.exit(f"no CMakePresets.json in {build_dir} — run `conan install` first")

data = json.loads(preset.read_text())
test_presets = data.get("testPresets") or [{}]
env = test_presets[0].get("environment", {})


def resolve(value: str) -> str:
    # Resolve `$penv{VAR}` (parent/current environment) references to the value
    # present when this script runs; CodeLLDB envFile has no variable expansion.
    return re.sub(r"\$penv\{(\w+)\}", lambda m: os.environ.get(m.group(1), ""), value)


out = repo / ".vscode" / ".env"
out.parent.mkdir(exist_ok=True)
lines = [f"{k}={resolve(v)}" for k, v in env.items()]
out.write_text("\n".join(lines) + "\n")
print(f"wrote {len(lines)} vars to {out}")
for line in lines:
    print("  " + line.split("=", 1)[0])
