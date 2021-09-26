#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import argparse
import json
import subprocess
import shlex
from common import bcolors


def tidy_json(path):
    try:
        with open(path, 'r') as f:
            parsed = json.load(f)
        with open(path, 'w') as f:
            json.dump(parsed, f, indent=4, sort_keys=True)
        return 0
    except ValueError:
        return 1


def tidy_html(path):
    config_path = os.path.join(os.path.dirname(os.path.realpath(__file__)),
                               ".html-tidy")
    cmd = shlex.split(f'tidy -config "{config_path}" -q -m "{path}"')
    result = subprocess.run(cmd,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    if result.returncode == 1:
        return 1
    if result.returncode > 1:
        return 2
    return 0


def tidy_file(path):
    if path.endswith('.json'):
        return tidy_json(path)
    elif path.endswith('.html'):
        return tidy_html(path)


def tidyable_file(path):
    if path.endswith('.json'):
        return True
    if path.endswith('.html'):
        return True
    return False


def tidy_dir(path, level=0, prefix=''):
    prefix_file = prefix + '├── '
    if level == 0:
        print(f'tidy dir {path}')

    result = {
        'warning': [],
        'error': [],
    }

    items = [os.path.join(path, name) for name in os.listdir(path)]
    files = sorted([
        path for path in items if os.path.isfile(path) and tidyable_file(path)
    ])
    dirs = sorted([path for path in items if os.path.isdir(path)])

    for filename in [os.path.basename(path) for path in files]:
        filepath = os.path.join(path, filename)
        tidy = tidy_file(filepath)
        if tidy == 0:
            print(f'{prefix_file}{bcolors.OKGREEN}{filename} ✓{bcolors.ENDC}')
        elif tidy == 1:
            print(f'{prefix_file}{bcolors.WARNING}{filename} ✓{bcolors.ENDC}')
            result['warning'].append(filepath)
        elif tidy > 1:
            print(f'{prefix_file}{bcolors.FAIL}{filename} ✘{bcolors.ENDC}')
            result['error'].append(filepath)

    for dirname in [os.path.basename(path) for path in dirs]:
        print(prefix + '├── ' + dirname)
        subresult = tidy_dir(os.path.join(path, dirname),
                             level=level + 1,
                             prefix=prefix + '│   ')
        result['warning'].extend(subresult['warning'])
        result['error'].extend(subresult['error'])

    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('a')
    args = parser.parse_args()

    result = tidy_dir(args.a)
    if result['error']:
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
