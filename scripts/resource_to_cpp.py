#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import argparse


def resource_to_cpp(data, max_string_length=None):
    string = data.decode('utf-8')
    segments = [string]
    if max_string_length is not None:
        n = max_string_length
        segments = [string[i:i+n] for i in range(0, len(string), n)]
    result = []
    for segment in segments:
        result.append('R"V0G0N(' + segment + ')V0G0N"')
    return '\n'.join(result)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('resource')
    parser.add_argument('--max-string-length', default=2048)
    args = parser.parse_args()

    with open(args.resource, 'rb') as f:
        print(resource_to_cpp(f.read(), args.max_string_length))

    return 0


if __name__ == "__main__":
    sys.exit(main())
