#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import argparse
import io
from PIL import Image, ImageChops
from selenium import webdriver
import pathlib


def to_url(something):
    if os.path.isfile(something):
        return pathlib.Path(os.path.abspath(something)).as_uri()
    return something


def screenshot(browser, url):
    browser.get(url)

    body = browser.find_element('tag name', 'body')
    png = body.screenshot_as_png
    return Image.open(io.BytesIO(png))


def get_browser(driver='firefox', max_width=1000, max_height=10000):
    if driver == 'phantomjs':
        browser = webdriver.PhantomJS()
    elif driver == 'firefox':
        options = webdriver.FirefoxOptions()
        options.add_argument("--headless")
        browser = webdriver.Firefox(options=options)
    else:  # chrome or unknown
        options = webdriver.ChromeOptions()
        options.add_argument("--headless=new")
        browser = webdriver.Chrome(options=options)
    browser.set_window_size(max_width, max_height)
    return browser


def html_render_diff(a, b, browser):
    image_a = screenshot(browser, to_url(a))
    image_b = screenshot(browser, to_url(b))

    image_a = image_a.convert('RGB')
    image_b = image_b.convert('RGB')
    diff = ImageChops.difference(image_a, image_b)
    return diff, (image_a, image_b)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('a')
    parser.add_argument('b')
    parser.add_argument('--driver',
                        choices=['chrome', 'firefox', 'phantomjs'],
                        default='firefox')
    parser.add_argument('--max-width', default=1000)
    parser.add_argument('--max-height', default=10000)
    args = parser.parse_args()

    browser = get_browser(args.driver, args.max_width, args.max_height)
    diff, _ = html_render_diff(args.a, args.b, browser)
    browser.quit()
    bounding_box = diff.getbbox()

    if bounding_box:
        print('images are different')
        print('first error at %f%%' % (1e2 * bounding_box[1] / diff.height))
        print('bounding box %f%%' %
              (1e2 * ((bounding_box[2] - bounding_box[0]) *
                      (bounding_box[3] - bounding_box[1])) /
               (diff.width * diff.height)))
        return 1

    print('images are the same')
    return 0


if __name__ == "__main__":
    sys.exit(main())
