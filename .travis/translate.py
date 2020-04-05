#!/usr/bin/env python3

import os
import sys
import re
import argparse
import subprocess

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('translator')
  parser.add_argument('output')
  parser.add_argument('input', nargs='+')
  args = parser.parse_args()

  failed = []

  for infile in args.input:
    password = re.search('\$(.*)\$', infile)
    password = password.group(1) if password else None
    outhtml = os.path.join(args.output, os.path.basename(infile) + '.html')

    print('translate %s to %s' % (infile, outhtml))

    translate = [args.translator, infile, outhtml]
    if password:
      translate.append(password)
    result = subprocess.run(translate, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    print(result.stdout.decode("utf-8").strip())
    if result.returncode != 0 or not os.path.isfile(outhtml):
      if os.path.isfile(outhtml):
        os.remove(outhtml)
      failed.append(infile)
      print('FAILED')
      continue

  if not failed:
    return 0
  print('translation failed for the following files:')
  print('\n'.join(failed))
  return 1

if __name__== "__main__":
  sys.exit(main())
