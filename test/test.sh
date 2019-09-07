#!/bin/bash

rm test.html
rm test.png

../cmake-build-debug/main/odrmain "/home/andreas/Desktop/odr/ruski.odt" "test.html"

phantomjs --debug=true render.js
