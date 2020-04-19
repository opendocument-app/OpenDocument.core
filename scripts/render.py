#!/usr/bin/env python3

import os
import sys
import argparse
import subprocess
import shlex

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('renderer')
  parser.add_argument('output')
  parser.add_argument('input', nargs='+')
  args = parser.parse_args()

  failed = []

  for infile in args.input:
    outimage = os.path.join(args.output, os.path.splitext(os.path.basename(infile))[0] + '.png')

    print('render %s to %s' % (infile, outimage))

    cmd = shlex.split(args.renderer) + [infile, outimage]
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode != 0 or not os.path.isfile(outimage):
      try:
        os.remove(outimage)
      except OSError:
        pass
      failed.append(infile)
      print('ERROR')
      print(result.stdout.decode('utf-8'))
      continue

  if not failed:
    return 0
  print('render failed for the following files:')
  print('\n'.join(failed))
  return 1

if __name__== "__main__":
  sys.exit(main())
