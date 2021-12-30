#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import json
import argparse


def binary_resource_to_cpp(data):
    hex_data = data.hex()
    cpp_data = ','.join('0x' + hex_data[i:i+2] for i in range(0, len(hex_data), 2))
    return 'reinterpret_cast<const char *>((const unsigned char[]){' + cpp_data + '})'


def text_resource_to_cpp(data, max_string_length=None):
    string = data.decode('utf-8')
    segments = [string]
    if max_string_length is not None:
        n = max_string_length
        segments = [string[i:i + n] for i in range(0, len(string), n)]
    result = []
    for segment in segments:
        result.append('R"V0G0N(' + segment + ')V0G0N"')
    return ' '.join(result)


def resource_to_cpp(data, file_type, max_string_length=None):
    if file_type == 'text':
        return text_resource_to_cpp(data, max_string_length=max_string_length)
    return binary_resource_to_cpp(data)


def generate_declarations(basedir, resources):
    lines = []

    for i, resource in enumerate(resources['files']):
        lines.append(
            f'extern const char *file{i}_path;'
        )
        lines.append(
            f'extern const char *file{i}_data;'
        )
        lines.append(
            f'extern const std::uint32_t file{i}_size;'
        )

    lines.append(
        f'extern const char *files_path[];'
    )
    lines.append(
        f'extern const char *files_data[];'
    )
    lines.append(
        f'extern const std::uint32_t files_size[];'
    )
    lines.append(
        f'extern const std::uint32_t files_count;'
    )

    return lines


def generate_definitions(basedir, resources, max_string_length=None):
    lines = []

    for i, resource in enumerate(resources['files']):
        with open(os.path.join(basedir, resource['srcPath']), 'rb') as f:
            data = f.read()

        lines.append(
            f'const char *file{i}_path = "{resource["dstPath"]}";'
        )
        lines.append(
            f'const char *file{i}_data = ' +
            resource_to_cpp(data, resource["type"], max_string_length=max_string_length) +
            ';'
        )
        lines.append(
            f'const std::uint32_t file{i}_size = {len(data)};'
        )

    files_count = len(resources['files'])
    files_path = ', '.join(f'file{i}_path' for i in range(files_count))
    files_data = ', '.join(f'file{i}_data' for i in range(files_count))
    files_size = ', '.join(f'file{i}_size' for i in range(files_count))

    lines.append(
        f'const char *files_path[] = {{{files_path}}};'
    )
    lines.append(
        f'const char *files_data[] = {{{files_data}}};'
    )
    lines.append(
        f'const std::uint32_t files_size[] = {{{files_size}}};'
    )
    lines.append(
        f'const std::uint32_t files_count = {files_count};'
    )

    return lines


def replace_lines(path, begin_pattern, end_pattern, new_lines):
    with open(path, 'r') as f:
        lines = f.readlines()
        newlines = f.newlines

    begin_index = next(i for i, line in enumerate(lines) if begin_pattern in line)
    end_index = next(i for i, line in enumerate(lines) if end_pattern in line)

    lines = lines[:begin_index + 1] + [new_line + newlines for new_line in new_lines] + lines[end_index:]

    with open(path, 'w') as f:
        f.writelines(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('resources')
    parser.add_argument('header')
    parser.add_argument('source')
    parser.add_argument('--max-string-length', default=2048)
    args = parser.parse_args()

    with open(args.resources, 'rb') as f:
        resources = json.load(f)

    basedir = os.path.dirname(os.path.abspath(args.resources))

    replace_lines(
        args.header,
        '//>>>>resource declaration start',
        '//>>>>resource declaration end',
        generate_declarations(basedir, resources))

    replace_lines(
        args.source,
        '//>>>>resource definition start',
        '//>>>>resource definition end',
        generate_definitions(basedir, resources, max_string_length=args.max_string_length))

    return 0


if __name__ == "__main__":
    sys.exit(main())
