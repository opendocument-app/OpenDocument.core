#!/bin/bash

set -e

: ${ODR:=../cmake-build-debug/main/odrmain}
: ${INPUT_DIR:=.}
: ${OUTPUT_DIR:=${INPUT_DIR}}

for file in ${INPUT_DIR}/*; do
  file=${file%*/}
  name=${file##*/}
  name=${name%.*}

  if [[ $file == *.odt ]] || [[ $file == *.ods ]] || [[ $file == *.odp ]]; then
    html="${OUTPUT_DIR}/${name}.html"
    image="${OUTPUT_DIR}/${name}.png"

    rm "${html}" 2> /dev/null || true
    rm "${image}" 2> /dev/null || true

    echo "translate ${file} to ${html}"
    "${ODR}" "${file}" "${html}"
    [[ -f "${html}" ]] || ( echo "html not found ${html}" && exit 1 )

    echo "render ${html} to ${image}"
    phantomjs render.js "${html}" "${image}"
    [[ -f "${image}" ]] || ( echo "image not found ${image}" && exit 1 )
  fi
done
