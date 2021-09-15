#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import argparse
import json
from html_render_diff import get_browser, html_render_diff
from common import bcolors
import filecmp


def parse_json(path):
    with open(path) as f:
        return json.load(f)


def compare_json(a, b):
    json_a = json.dumps(parse_json(a), sort_keys=True)
    json_b = json.dumps(parse_json(b), sort_keys=True)
    return json_a == json_b


def compare_html(a, b, browser=None):
    if browser is None:
        browser = get_browser()
    diff = html_render_diff(browser, a, b)
    return True if diff.getbbox() is None else False


def compare_files(a, b, **kwargs):
    if filecmp.cmp(a, b):
        return True
    if a.endswith('.json'):
        return compare_json(a, b)
    elif a.endswith('.html'):
        return compare_html(a, b, **kwargs)


def comparable_file(path):
    if path.endswith('.json'):
        return True
    if path.endswith('.html'):
        return True
    return False


def compare_dirs(a, b, level=0, prefix='', **kwargs):
    prefix_file = prefix + '├── '
    if level == 0:
        print(f'compare dir {a} with {b}')

    result = {
        'left_files_missing': [],
        'right_files_missing': [],
        'left_dirs_missing': [],
        'right_dirs_missing': [],
        'files_different': [],
    }

    left = sorted(os.listdir(a))
    right = sorted(os.listdir(b))

    left_files = sorted([
        name for name in left if os.path.isfile(os.path.join(a, name))
        and comparable_file(os.path.join(a, name))
    ])
    right_files = sorted([
        name for name in right if os.path.isfile(os.path.join(b, name))
        and comparable_file(os.path.join(b, name))
    ])
    common_files = [name for name in left_files if name in right_files]

    if left_files != right_files:
        left_missing = ' '.join(
            [name for name in right_files if name not in left_files])
        right_missing = ' '.join(
            [name for name in left_files if name not in right_files])
        if left_missing:
            print(
                f'{prefix_file}{bcolors.FAIL}missing files left: {left_missing} ✘{bcolors.ENDC}'
            )
            result['left_files_missing'].extend([
                os.path.join(a, name) for name in right_files
                if name not in left_files
            ])
        if right_missing:
            print(
                f'{prefix_file}{bcolors.FAIL}missing files right: {right_missing} ✘{bcolors.ENDC}'
            )
            result['right_files_missing'].extend([
                os.path.join(b, name) for name in left_files
                if name not in right_files
            ])

    for name in common_files:
        cmp = compare_files(os.path.join(a, name), os.path.join(b, name),
                            **kwargs)
        if cmp:
            print(f'{prefix_file}{bcolors.OKGREEN}{name} ✓{bcolors.ENDC}')
        else:
            print(f'{prefix_file}{bcolors.FAIL}{name} ✘{bcolors.ENDC}')
            result['files_different'].append(os.path.join(a, name))

    left_dirs = sorted(
        [name for name in left if os.path.isdir(os.path.join(a, name))])
    right_dirs = sorted(
        [name for name in right if os.path.isdir(os.path.join(b, name))])
    common_dirs = [path for path in left_dirs if path in right_dirs]

    if left_dirs != right_dirs:
        left_missing = ' '.join(
            [name for name in right_dirs if name not in left_dirs])
        right_missing = ' '.join(
            [name for name in left_dirs if name not in right_dirs])
        if left_missing:
            print(
                f'{prefix_file}{bcolors.FAIL}missing dirs left: {left_missing} ✘{bcolors.ENDC}'
            )
            result['left_dirs_missing'].extend([
                os.path.join(a, name) for name in right_dirs
                if name not in left_dirs
            ])
        if right_missing:
            print(
                f'{prefix_file}{bcolors.FAIL}missing dirs right: {right_missing} ✘{bcolors.ENDC}'
            )
            result['right_dirs_missing'].extend([
                os.path.join(b, name) for name in left_dirs
                if name not in right_dirs
            ])

    for name in common_dirs:
        print(prefix + '├── ' + name)
        subresult = compare_dirs(os.path.join(a, name),
                                 os.path.join(b, name),
                                 level=level + 1,
                                 prefix=prefix + '│   ',
                                 **kwargs)
        result['left_files_missing'].extend(subresult['left_files_missing'])
        result['right_files_missing'].extend(subresult['right_files_missing'])
        result['left_dirs_missing'].extend(subresult['left_dirs_missing'])
        result['right_dirs_missing'].extend(subresult['right_dirs_missing'])
        result['files_different'].extend(subresult['files_different'])

    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('a')
    parser.add_argument('b')
    args = parser.parse_args()

    result = compare_dirs(args.a, args.b, browser=get_browser())
    if result['left_files_missing'] or result['right_files_missing'] or result[
            'left_dirs_missing'] or result['right_dirs_missing'] or result[
                'files_different']:
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
