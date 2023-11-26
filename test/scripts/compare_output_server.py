#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import argparse
import threading
import io
from concurrent.futures import ThreadPoolExecutor
from html_render_diff import get_browser, html_render_diff
from compare_output import comparable_file, compare_files
from flask import Flask, send_from_directory, send_file
import watchdog.observers
import watchdog.events


class Config:
    path_a = None
    path_b = None
    driver = None
    observer = None
    comparator = None
    browser = None
    thread_local = threading.local()


class Observer:
    def __init__(self):
        class Handler(watchdog.events.FileSystemEventHandler):
            def __init__(self, path):
                self._path = path

            def dispatch(self, event):
                if event.event_type in ['opened']:
                    return

                if os.path.isfile(event.src_path):
                    Config.comparator.submit(
                        os.path.relpath(event.src_path, self._path))

        self._observer = watchdog.observers.Observer()
        self._observer.schedule(Handler(Config.path_a),
                                Config.path_a,
                                recursive=True)
        self._observer.schedule(Handler(Config.path_b),
                                Config.path_b,
                                recursive=True)

    def start(self):
        self._observer.start()

        def init_compare(a, b):
            common_path = os.path.relpath(a, Config.path_a)

            left = sorted(os.listdir(a))
            right = sorted(os.listdir(b))

            common = [name for name in left if name in right]

            for name in common:
                if os.path.isfile(os.path.join(a, name)) and comparable_file(
                        os.path.join(a, name)):
                    Config.comparator.submit(os.path.join(common_path, name))
                elif os.path.isdir(os.path.join(a, name)):
                    init_compare(os.path.join(a, name), os.path.join(b, name))

        init_compare(Config.path_a, Config.path_b)

    def stop(self):
        self._observer.stop()

    def join(self):
        self._observer.join()


class Comparator:
    def __init__(self, max_workers):
        def initializer():
            browser = getattr(Config.thread_local, 'browser', None)
            if browser is None:
                browser = get_browser(driver=Config.driver)
                Config.thread_local.browser = browser

        self._executor = ThreadPoolExecutor(max_workers=max_workers,
                                            initializer=initializer)
        self._result = {}
        self._future = {}

    def submit(self, path):
        if path in self._future:
            try:
                self._future[path].cancel()
                self._future[path].result()
                self._future.pop(path)
            except Exception:
                pass

        self._result[path] = 'pending'
        self._future[path] = self._executor.submit(self.compare, path)

    def compare(self, path):
        browser = getattr(Config.thread_local, 'browser', None)
        result = compare_files(os.path.join(Config.path_a, path),
                               os.path.join(Config.path_b, path),
                               browser=browser)
        self._result[path] = 'same' if result else 'different'
        self._future.pop(path)

    def result(self, path):
        if path in self._result:
            return self._result[path]
        return 'unknown'

    def result_symbol(self, path):
        result = self.result(path)
        if result == 'pending':
            return '🔄'
        if result == 'same':
            return '✔'
        if result == 'different':
            return '❌'
        return '⛔'

    def result_css(self, path):
        result = self.result(path)
        if result == 'pending':
            return 'color:blue;'
        if result == 'same':
            return 'color:green;'
        if result == 'different':
            return 'color:orange;'
        return 'color:red;'


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
            if Config.comparator is None:
                result += f'<li><a href="/compare/{os.path.join(common_path, name)}">{name}</a></li>'
            else:
                symbol = Config.comparator.result_symbol(
                    os.path.join(common_path, name))
                css = Config.comparator.result_css(
                    os.path.join(common_path, name))
                result += f'<li style="{css}"><a style="{css}" href="/compare/{os.path.join(common_path, name)}">{name}</a> {symbol}</li>'

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
<body style="display:flex;flex-flow:row;">
<div style="display:flex;flex:1;flex-flow:column;margin:5px;">
  <a href="/file/a/{path}">{os.path.join(Config.path_a, path)}</a>
  <iframe id="a" src="/file/a/{path}" title="a" frameborder="0" align="left" style="flex:1;"></iframe>
</div>
<div style="display:flex;flex:0 0 50px;flex-flow:column;">
  <a href="/image_diff/{path}">diff</a>
  <img src="/image_diff/{path}" width="50" height="0" style="flex:1;">
</div>
<div style="display:flex;flex:1;flex-flow:column;margin:5px;">
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


@app.route('/image_diff/<path:path>')
def image_diff(path):
    diff, _ = html_render_diff(os.path.join(Config.path_a, path),
                               os.path.join(Config.path_b, path),
                               Config.browser)
    tmp = io.BytesIO()
    diff.save(tmp, 'JPEG', quality=70)
    tmp.seek(0)
    return send_file(tmp, mimetype='image/jpeg')


@app.route('/file/<variant>/<path:path>')
def file(variant, path):
    variant_root = Config.path_a if variant == 'a' else Config.path_b
    return send_from_directory(variant_root, path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('a')
    parser.add_argument('b')
    parser.add_argument('--driver',
                        choices=['chrome', 'firefox', 'phantomjs'],
                        default='firefox')
    parser.add_argument('--max-workers', type=int, default=1)
    parser.add_argument('--compare', action='store_true')
    args = parser.parse_args()

    Config.path_a = args.a
    Config.path_b = args.b
    Config.driver = args.driver
    Config.browser = get_browser(driver=args.driver)

    if args.compare:
        Config.comparator = Comparator(max_workers=args.max_workers)

        Config.observer = Observer()
        Config.observer.start()

    app.run(host='0.0.0.0')

    if args.compare:
        Config.observer.stop()
        Config.observer.join()

    return 0


if __name__ == "__main__":
    sys.exit(main())
