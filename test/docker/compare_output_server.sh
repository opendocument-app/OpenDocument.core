#!/usr/bin/env bash

REF="test/data/reference-output/"
OBS="cmake-build-debug/test/output/"
DRIVER="firefox"

docker build --tag odr_core_test test/docker
docker run -ti -v $(pwd):/repo -p 5000:5000 odr_core_test python3  /repo/test/scripts/compare_output_server.py /repo/$REF /repo/$OBS --compare --driver $DRIVER