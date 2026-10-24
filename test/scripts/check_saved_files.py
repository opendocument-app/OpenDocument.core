#!/usr/bin/env python3
"""Load every file `SaveCorpus` saved in LibreOffice.

Our own reader accepts packages that LibreOffice refuses, so reopening a saved
file with odr proves little. `SaveCorpus.a_small_edit_saves_and_opens_again`
edits a few words in every editable corpus document and saves it under
`output/saved`. This script converts each of those files to pdf with headless
LibreOffice, and fails where LibreOffice cannot load one. See #1211.

    check_saved_files.py build/test/output/saved

Needs LibreOffice: `soffice` on PATH, or the macOS app.
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile

MACOS_SOFFICE = "/Applications/LibreOffice.app/Contents/MacOS/soffice"
# a sheet of a million rows takes LibreOffice minutes, not hours
TIMEOUT_SECONDS = 600

# LibreOffice opens a file it cannot read as a package as plain text in Writer,
# so the module it names has to match the type
MODULES = {
    ".odt": "Writer",
    ".docx": "Writer",
    ".ods": "Calc",
    ".xlsx": "Calc",
    ".csv": "Calc",
    ".odp": "Impress",
    ".pptx": "Impress",
    ".odg": "Draw",
}


def find_soffice():
    if path := shutil.which("soffice"):
        return path
    if os.path.isfile(MACOS_SOFFICE):
        return MACOS_SOFFICE
    sys.exit("soffice not found: install LibreOffice")


def saved_files(root):
    for directory, _, names in os.walk(root):
        for name in sorted(names):
            yield os.path.join(directory, name)


def loads(soffice, path, scratch):
    """LibreOffice's complaint, or None if it converts the file."""
    # soffice exits 0 also where it cannot load the file, so read what it says
    try:
        result = subprocess.run(
            [
                soffice,
                "--headless",
                "--norestore",
                f"-env:UserInstallation=file://{scratch}/profile",
                "--convert-to",
                "pdf",
                "--outdir",
                os.path.join(scratch, "pdf"),
                path,
            ],
            capture_output=True,
            text=True,
            check=False,
            timeout=TIMEOUT_SECONDS,
        )
    except subprocess.TimeoutExpired:
        return f"no answer within {TIMEOUT_SECONDS} s"
    output = (result.stdout + result.stderr).strip()
    if result.returncode != 0 or "Error" in output or " -> " not in output:
        return output or f"soffice exited {result.returncode}"
    module = MODULES.get(os.path.splitext(path)[1].lower())
    if module is not None and f"as a {module} document" not in output:
        return f"not opened as a {module} document\n{output}"
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", help="the `output/saved` tree odr_test writes")
    arguments = parser.parse_args()

    soffice = find_soffice()
    checked = 0
    failed = []
    with tempfile.TemporaryDirectory() as scratch:
        for path in saved_files(arguments.root):
            checked += 1
            if (complaint := loads(soffice, path, scratch)) is not None:
                failed.append((path, complaint))

    if checked == 0:
        sys.exit("No saved files found to check")
    for path, complaint in failed:
        print(f"FAIL {path}", file=sys.stderr)
        for line in complaint.splitlines():
            print(f"     {line}", file=sys.stderr)
    print(f"{checked} saved file(s) checked, {len(failed)} refused by LibreOffice")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
