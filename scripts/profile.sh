#!/usr/bin/env bash
# Record the command in Instruments without replacing an earlier trace.
set -euo pipefail
repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
trace="$(mktemp -d "${TMPDIR:-/tmp}/odr-profile.XXXXXX")/profile.trace"
"$repo/scripts/run-with-env.sh" xctrace record --template 'Time Profiler' \
  --output "$trace" --launch -- "$@"
open "$trace"
