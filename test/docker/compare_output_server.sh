#!/usr/bin/env bash

REF="test/data/reference-output/"
OBS="cmake-build-relwithdebinfo/test/output/"
DRIVER="firefox"

if [ ! -d "$REF" ]; then
  echo "Reference output directory does not exist: $REF"
  exit 1
fi
if [ ! -d "$OBS" ]; then
  echo "Observed output directory does not exist: $OBS"
  exit 1
fi

docker run -ti \
  -v $(pwd):/repo \
  -p 8000:8000 \
  ghcr.io/opendocument-app/odr_core_test \
  python3 /repo/test/scripts/compare_output_server.py /repo/$REF /repo/$OBS --compare --driver $DRIVER --port 8000
