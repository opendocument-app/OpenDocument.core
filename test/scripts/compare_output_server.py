#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import argparse
from compare_output import comparable_file
from flask import Flask, send_from_directory


class Config:
    path_a = None
    path_b = None


app = Flask('compare')


@app.route("/")
def root():
    def print_tree(a, b):
        common_path = os.path.relpath(a, Config.path_a)

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

        left_dirs = sorted(
            [name for name in left if os.path.isdir(os.path.join(a, name))])
        right_dirs = sorted(
            [name for name in right if os.path.isdir(os.path.join(b, name))])

        common_files = [name for name in left_files if name in right_files]
        common_dirs = [name for name in left_dirs if name in right_dirs]

        result = '<ul>'

        left_files_missing = ' '.join(
            [name for name in right_files if name not in left_files])
        right_files_missing = ' '.join(
            [name for name in left_files if name not in right_files])
        left_dirs_missing = ' '.join(
            [name for name in right_dirs if name not in left_dirs])
        right_dirs_missing = ' '.join(
            [name for name in left_dirs if name not in right_dirs])
        if left_files_missing:
            result += f'<li><b>A files missing: {left_files_missing}</b></li>'
        if right_files_missing:
            result += f'<li><b>B files missing: {right_files_missing}</b></li>'
        if left_dirs_missing:
            result += f'<li><b>A dirs missing: {left_dirs_missing}</b></li>'
        if right_dirs_missing:
            result += f'<li><b>B dirs missing: {right_dirs_missing}</b></li>'

        for name in common_files:
            result += f'<li><a href="/compare/{os.path.join(common_path, name)}">{name}</a></li>'

        for name in common_dirs:
            result += f'<li>{name}'
            result += print_tree(os.path.join(a, name), os.path.join(b, name))
            result += '</li>'

        result += '</ul>'
        return result

    result = ''
    result += f'<p>compare A {Config.path_a} vs B {Config.path_b}</p>'
    result += print_tree(Config.path_a, Config.path_b)
    return result


@app.route('/compare/<path:path>')
def compare(path):
    return f'''<!DOCTYPE html>
<html>
<head>
<style>
html,body {{height:100%;margin:0;}}
</style>
</head>
<body style="display:flex;">
<div style="flex:1;display:flex;flex-flow:column;margin:5px;">
  <a href="/file/a/{path}">{os.path.join(Config.path_a, path)}</a>
  <iframe id="a" src="/file/a/{path}" title="a" frameborder="0" align="left" style="flex:1;"></iframe>
</div>
<div style="flex:1;display:flex;flex-flow:column;margin:5px;">
  <a href="/file/b/{path}">{os.path.join(Config.path_b, path)}</a>
  <iframe id="b" src="/file/b/{path}" title="b" frameborder="0" align="right" style="flex:1;"></iframe>
</div>
<script>
var iframe_a = document.getElementById('a');
var iframe_b = document.getElementById('b');
iframe_a.contentWindow.addEventListener('scroll', function(event) {{
  iframe_b.contentWindow.scrollTo(iframe_a.contentWindow.scrollX, iframe_a.contentWindow.scrollY);
}});
iframe_b.contentWindow.addEventListener('scroll', function(event) {{
  iframe_a.contentWindow.scrollTo(iframe_b.contentWindow.scrollX, iframe_b.contentWindow.scrollY);
}});
</script>
</body>
</html> 
'''


@app.route('/file/<variant>/<path:path>')
def file(variant, path):
    variant_root = Config.path_a if variant == 'a' else Config.path_b
    return send_from_directory(variant_root, path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('a')
    parser.add_argument('b')
    args = parser.parse_args()

    Config.path_a = args.a
    Config.path_b = args.b

    app.run()
    return 0


if __name__ == "__main__":
    sys.exit(main())
