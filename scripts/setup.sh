#!/bin/bash

# clean old setup
rm .git/hooks/pre-commit

# setup git hooks
ln -s ../../scripts/githooks/clang-format.sh .git/hooks/pre-commit
