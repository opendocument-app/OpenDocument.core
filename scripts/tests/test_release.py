import argparse
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import release


class ReleaseVersionTest(unittest.TestCase):
    def test_package_versions_are_preserved(self):
        for value in ("v7.4.1", "v8.0.0-rc.1", "v8.0.0-rc.1+build.2"):
            with self.subTest(value=value):
                self.assertEqual(release.release_version(value), value)

    def test_invalid_versions_cannot_reach_metadata_or_workflow_outputs(self):
        for value in (
            "", "7.4.1", "v7.4", "v7.4.1-.", "v7.4.1\nversion=v0.0.0",
            "v7.4.1 --asset x", "v7.4.1$(touch marker)",
        ):
            with self.subTest(value=value):
                with self.assertRaises(argparse.ArgumentTypeError):
                    release.release_version(value)
