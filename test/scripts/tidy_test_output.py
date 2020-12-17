#!/usr/bin/env python3

import os
import sys
import argparse
import json
import subprocess
import shlex


class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'


class HtmlTidyError(Exception):
    pass


def tidy_json(path, **kwargs):
    try:
        with open(path) as f:
            json.load(f)
        return 0
    except ValueError:
        return 1


def tidy_html(path, **kwargs):
    result = subprocess.run(shlex.split('tidy -q %s' % path), stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode == 1:
        return 1
    if result.returncode > 1:
        return 2
    return 0


def tidy_file(path, **kwargs):
    if path.endswith('.json'):
        return tidy_json(path)
    elif path.endswith('.html'):
        return tidy_html(path)


def tidyable_file(path, **kwargs):
    if path.endswith('.json'):
        return True
    if path.endswith('.html'):
        return True
    return False


def tidy_dir(path, level=0, prefix='', **kwargs):
    prefix_file = prefix + '├── '
    if level == 0:
        print(f'tidy dir {path}')

    items = [os.path.join(path, name) for name in os.listdir(path)]
    files = sorted([path for path in items if os.path.isfile(path) and tidyable_file(path)])
    dirs = sorted([path for path in items if os.path.isdir(path)])

    for filename in [os.path.basename(path) for path in files]:
        tidy = tidy_file(os.path.join(path, filename))
        if tidy == 0:
            print(f'{prefix_file}{bcolors.OKGREEN}{filename} ✓{bcolors.ENDC}')
        elif tidy == 1:
            print(f'{prefix_file}{bcolors.WARNING}{filename} ✓{bcolors.ENDC}')
        elif tidy > 1:
            print(f'{prefix_file}{bcolors.FAIL}{filename} ✘{bcolors.ENDC}')

    for dirname in [os.path.basename(path) for path in dirs]:
        print(prefix + '├── ' + dirname)
        tidy_dir(os.path.join(path, dirname), level=level+1, prefix=prefix + '│   ', **kwargs)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('a')
    args = parser.parse_args()

    tidy_dir(args.a)

    return 0


if __name__ == "__main__":
    sys.exit(main())
