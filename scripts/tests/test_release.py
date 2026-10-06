import argparse
import sys
import unittest
from unittest import mock
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import release


class ReleaseVersionTest(unittest.TestCase):
    def test_package_versions_are_preserved(self):
        for value in ("v7.4.1", "v8.0.0-rc.1", "v8.0.0-rc.1+build.2", "v0.0.0-0+001", "v1.2.3-01a"):
            with self.subTest(value=value):
                self.assertEqual(release.release_version(value), value)

    def test_invalid_versions_cannot_reach_metadata_or_workflow_outputs(self):
        for value in (
            "", "7.4.1", "v7.4", "v7.4.1-.", "v07.4.1", "v7.04.1",
            "v7.4.01", "v7.4.1-01", "v7.4.1-rc.01", "v7.4.1\nversion=v0.0.0",
            "v7.4.1 --asset x", "v7.4.1$(touch marker)",
        ):
            with self.subTest(value=value):
                with self.assertRaises(argparse.ArgumentTypeError):
                    release.release_version(value)

    def test_explicit_version_cannot_reuse_an_existing_tag(self):
        with mock.patch.object(release.subprocess, "run", return_value=argparse.Namespace(returncode=0)):
            with self.assertRaises(SystemExit):
                release.command_version(argparse.Namespace(version="v7.4.1"))
