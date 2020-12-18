#!/usr/bin/env python3

import os
import sys
import argparse
import json
from html_render_diff import get_browser, html_render_diff
from common import bcolors
import filecmp


class CompareError(Exception):
    pass


def parse_json(path):
    with open(path) as f:
        return json.load(f)


def compare_json(a, b, **kwargs):
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
        return compare_html(a, b)


def comparable_file(path, **kwargs):
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

    left = [os.path.join(a, name) for name in os.listdir(a)]
    right = [os.path.join(a, name) for name in os.listdir(b)]

    left_files = sorted([path for path in left if os.path.isfile(path) and comparable_file(path)])
    right_files = sorted([path for path in right if os.path.isfile(path) and comparable_file(path)])

    if left_files != right_files:
        left_missing = ' '.join([os.path.basename(path) for path in right_files if path not in left_files])
        right_missing = ' '.join([os.path.basename(path) for path in left_files if path not in right_files])
        if left_missing:
            print(f'{prefix_file}{bcolors.FAIL}missing files left: {left_missing} ✘{bcolors.ENDC}')
            result['left_files_missing'].extend([path for path in right_files if path not in left_files])
        if right_missing:
            print(f'{prefix_file}{bcolors.FAIL}missing files right: {right_missing} ✘{bcolors.ENDC}')
            result['right_files_missing'].extend([path for path in left_files if path not in right_files])
    common_files = [path for path in left_files if path in right_files]

    for filename in [os.path.basename(path) for path in common_files]:
        cmp = compare_files(os.path.join(a, filename), os.path.join(b, filename), **kwargs)
        if cmp:
            print(f'{prefix_file}{bcolors.OKGREEN}{filename} ✓{bcolors.ENDC}')
        else:
            print(f'{prefix_file}{bcolors.FAIL}{filename} ✘{bcolors.ENDC}')
            result['files_different'].append(os.path.join(a, filename))

    left_dirs = sorted([path for path in left if os.path.isdir(path)])
    right_dirs = sorted([path for path in right if os.path.isdir(path)])

    if left_dirs != right_dirs:
        left_missing = ' '.join([os.path.basename(path) for path in right_dirs if path not in left_dirs])
        right_missing = ' '.join([os.path.basename(path) for path in left_dirs if path not in right_dirs])
        if left_missing:
            print(f'{prefix_file}{bcolors.FAIL}missing dirs left: {left_missing} ✘{bcolors.ENDC}')
            result['left_dirs_missing'].extend([path for path in right_dirs if path not in left_dirs])
        if right_missing:
            print(f'{prefix_file}{bcolors.FAIL}missing dirs right: {right_missing} ✘{bcolors.ENDC}')
            result['right_dirs_missing'].extend([path for path in left_dirs if path not in right_dirs])
    common_dirs = [path for path in left_dirs if path in right_dirs]

    for dirname in [os.path.basename(path) for path in common_dirs]:
        print(prefix + '├── ' + dirname)
        subresult = compare_dirs(os.path.join(a, dirname), os.path.join(b, dirname), level=level + 1,
                                 prefix=prefix + '│   ', **kwargs)
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
    if result['left_files_missing'] or result['right_files_missing'] or result['left_dirs_missing'] or result[
        'right_dirs_missing'] or result['files_different']:
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
