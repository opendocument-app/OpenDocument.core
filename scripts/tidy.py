#!/usr/bin/env python3

import os
import sys
import argparse
import subprocess
import shlex

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('tidy')
  parser.add_argument('input', nargs='+')
  args = parser.parse_args()

  failed = []

  for infile in args.input:
    print('tidy %s' % infile)

    cmd = shlex.split(args.tidy) + [infile]
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode == 1:
      print('WARNING')
      # too much spam
      #print(result.stdout)
    if result.returncode > 1:
      failed.append(infile)
      print('ERROR')
      print(result.stdout.decode('utf-8'))
      continue

  if not failed:
    return 0
  print('tidy failed for the following files:')
  print('\n'.join(failed))
  return 1

if __name__== "__main__":
  sys.exit(main())
