#!/usr/bin/env python3

import os
import sys
import argparse
import subprocess
import json

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('editor')
  parser.add_argument('output')
  parser.add_argument('input', nargs='+')
  args = parser.parse_args()

  failed = []

  for infile in args.input:
    outjson = os.path.join(args.output, os.path.basename(infile) + '.json')
    print('edit %s to %s' % (infile, outjson))

    result = subprocess.run([args.editor, infile, outjson], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
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
    with open(outjson, 'wb') as f:
      json.dumps(parsed, f, indent=4, sort_keys=True)

  if not failed:
    return 0
  print('edit failed for the following files:')
  print('\n'.join(failed))
  return 1

if __name__== "__main__":
  sys.exit(main())
