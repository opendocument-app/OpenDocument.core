#!/usr/bin/env python3

import os
import sys
import argparse
import cv2
from skimage.measure import compare_ssim
import filecmp

def imagediff(a, b):
  imageA = cv2.imread(a)
  imageB = cv2.imread(b)
  ssim, diff = compare_ssim(imageA, imageB, full=True, multichannel=True)
  return ssim

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('a')
  parser.add_argument('b')
  parser.add_argument('--ssim-limit', type=float, default=0.99)
  args = parser.parse_args()

  missing = False
  min_ssim = 1

  if os.path.isdir(args.a) and os.path.isdir(args.b):
    dircmp = filecmp.dircmp(args.a, args.b)
    for file in dircmp.common:
      ssim = imagediff(os.path.join(args.a, file), os.path.join(args.b, file))
      min_ssim = min(ssim, min_ssim)
      print('ssim {} for {}'.format(ssim, file))
    missing = dircmp.left_only or dircmp.right_only
    for a in dircmp.left_only:
      print('left only {}'.formt(a))
    for b in dircmp.right_only:
      print('right only {}'.formt(b))
  else:
    ssim = imagediff(args.a, args.b)
    min_ssim = min(ssim, min_ssim)
    print('ssim {} for {} vs {}'.format(ssim, args.a, args.b))

  if missing or min_ssim < args.ssim_limit:
    return 1
  return 0

if __name__== "__main__":
    sys.exit(main())
