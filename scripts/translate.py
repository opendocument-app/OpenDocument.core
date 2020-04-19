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

    cmd = [args.translator, infile, outhtml]
    if password:
      cmd.append(password)
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode != 0 or not os.path.isfile(outhtml):
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
  print('translation failed for the following files:')
  print('\n'.join(failed))
  return 1

if __name__== "__main__":
  sys.exit(main())
