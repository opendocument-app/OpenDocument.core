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
  --platform linux/amd64 \
  ghcr.io/opendocument-app/odr_core_test:1.0.15 \
  compare-html-server /repo/$REF /repo/$OBS --compare --driver $DRIVER --port 8000
