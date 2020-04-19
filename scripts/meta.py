#!/usr/bin/env python3

import os
import sys
import re
import argparse
import subprocess
import json

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('meta')
  parser.add_argument('output')
  parser.add_argument('input', nargs='+')
  args = parser.parse_args()

  failed = []

  for infile in args.input:
    password = re.search('\$(.*)\$', infile)
    password = password.group(1) if password else None
    outjson = os.path.join(args.output, os.path.basename(infile) + '.json')
    print('meta %s to %s' % (infile, outjson))

    cmd = [args.meta, infile]
    if password:
        cmd.append(password)
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode != 0:
      failed.append(infile)
      print('ERROR')
      print(result.stdout.decode('utf-8'))
      continue

    try:
      parsed = json.loads(result.stdout.decode("utf-8"))
    except:
      failed.append(infile)
      print('FAILED json')
      continue
    with open(outjson, 'w') as f:
      json.dump(parsed, f, indent=4, sort_keys=True)

  if not failed:
    return 0
  print('meta failed for the following files:')
  print('\n'.join(failed))
  return 1

if __name__== "__main__":
  sys.exit(main())
