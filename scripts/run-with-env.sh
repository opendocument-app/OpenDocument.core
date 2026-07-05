#!/usr/bin/env bash
# Load the Conan runtime env from .vscode/.env (see gen-vscode-env.py) and exec
# the given command with it. Used by VS Code tasks so the test binary and CLI
# tools can find pdf2htmlEX / poppler / fontconfig / wvWare / libmagic data.
#
# Usage: scripts/run-with-env.sh <command> [args...]
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
env_file="$repo/.vscode/.env"

if [[ -f "$env_file" ]]; then
  set -a
  # shellcheck disable=SC1090
  source "$env_file"
  set +a
else
  echo "warning: $env_file missing — run scripts/gen-vscode-env.py" >&2
fi

exec "$@"
