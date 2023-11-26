#!/usr/bin/env bash

REF="test/data/reference-output/"
OBS="cmake-build-debug/test/output/"
DRIVER="firefox"

# manually build the image
#docker build --tag odr_core_test test/docker

docker run -ti \
  -v $(pwd):/repo \
  -p 5000:5000 \
  ghcr.io/opendocument-app/odr_core_test:sha-f9aab98 \
  python3 /repo/test/scripts/compare_output_server.py /repo/$REF /repo/$OBS --compare --driver $DRIVER
