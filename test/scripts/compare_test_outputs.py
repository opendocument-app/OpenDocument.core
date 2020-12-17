#!/usr/bin/env python3

import sys
import argparse
import json
from html_render_diff import get_browser, html_render_diff


def parse_json(path):
    with open(path) as f:
        return json.load(f)


def compare_json(a, b):
    json_a = json.dumps(parse_json(a), sort_keys=True)
    json_b = json.dumps(parse_json(b), sort_keys=True)
    return json_a == json_b


def compare_html(a, b):
    browser = get_browser()
    diff = html_render_diff(browser, a, b)
    return True if diff is None else False


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('a')
    parser.add_argument('b')
    args = parser.parse_args()

    return 0


if __name__ == "__main__":
    sys.exit(main())
