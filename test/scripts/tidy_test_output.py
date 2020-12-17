#!/usr/bin/env python3

import sys
import argparse
import json


def parse_json(path):
    with open(path) as f:
        return json.load(f)


def tidy_html(path):
    pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('a')
    args = parser.parse_args()

    # TODO tidy json files?

    # TODO tidy html files

    return 0


if __name__ == "__main__":
    sys.exit(main())
