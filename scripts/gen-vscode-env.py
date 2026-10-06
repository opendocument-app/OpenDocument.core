#!/usr/bin/env python3
"""Export the Conan test environment for VS Code tasks and CodeLLDB.

Usage: scripts/gen-vscode-env.py [BUILD_DIR]. Regenerate after conan install.
BUILD_DIR defaults to cmake-build-relwithdebinfo.
"""
import json
import os
import re
import shlex
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
    # Resolve CMake parent-environment references before quoting literal values.
    return re.sub(r"\$penv\{(\w+)\}", lambda m: os.environ.get(m.group(1), ""), value)


out = repo / ".vscode" / ".env"
out.parent.mkdir(exist_ok=True)
for key in env:
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", key):
        sys.exit(f"invalid environment variable name: {key!r}")
# Single-quote expansion is shared by Bash and CodeLLDB's dotenv reader.
lines = [f"{k}={shlex.quote(resolve(v))}" for k, v in env.items()]
out.write_text("\n".join(lines) + "\n")
print(f"wrote {len(lines)} vars to {out}")
for line in lines:
    print("  " + line.split("=", 1)[0])
