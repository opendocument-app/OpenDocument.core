#!/usr/bin/env python3

import os
import sys
import argparse
import io
from PIL import Image, ImageChops
from selenium import webdriver


def path_to_url(path):
    return 'file://' + path


def to_url(something):
    if os.path.isfile(something):
        return path_to_url(something)
    return something


def screenshot(browser, url):
    browser.get(url)

    body = browser.find_element_by_tag_name('body')
    png = body.screenshot_as_png
    return Image.open(io.BytesIO(png))


def get_browser(driver='chrome', max_width=1000, max_height=10000):
    options = webdriver.ChromeOptions()
    options.headless = True
    browser = webdriver.Chrome(options=options)
    browser.set_window_size(max_width, max_height)
    return browser


def html_render_diff(browser, a, b):
    image_a = screenshot(browser, to_url(a))
    image_b = screenshot(browser, to_url(b))

    diff = ImageChops.difference(image_a, image_b)
    return diff


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('a')
    parser.add_argument('b')
    parser.add_argument('--driver', choices=['chrome'], default='chrome')
    parser.add_argument('--max-width', default=1000)
    parser.add_argument('--max-height', default=10000)
    args = parser.parse_args()

    browser = get_browser(args.driver, args.max_width, args.max_height)
    diff = html_render_diff(browser, args.a, args.b)
    browser.quit()
    bounding_box = diff.getbbox()

    if bounding_box:
        print('images are different')
        print('first error at %f%%' % (1e2 * bounding_box[1] / diff.height))
        print('bounding box %f%%' % (
                    1e2 * ((bounding_box[2] - bounding_box[0]) * (bounding_box[3] - bounding_box[1])) / (
                        diff.width * diff.height)))
        return 1

    print('images are the same')
    return 0


if __name__ == "__main__":
    sys.exit(main())
