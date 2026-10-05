import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("cmake") and shutil.which("git"), "needs CMake and Git")
class TestDataCheckoutTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="odr test data ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.remote = self.root / "remote"
        self.remote.mkdir()
        self.git(self.remote, "init", "--quiet")
        self.git(self.remote, "config", "user.email", "test@example.invalid")
        self.git(self.remote, "config", "user.name", "Test")
        self.git(self.remote, "config", "commit.gpgsign", "false")
        self.git(self.remote, "config", "core.hooksPath", str(self.root / "no-hooks"))
        self.revisions = []
        for value in ("first", "second"):
            (self.remote / "fixture").write_text(value)
            self.git(self.remote, "add", "fixture")
            self.git(self.remote, "commit", "--quiet", "-m", value)
            self.revisions.append(self.git(self.remote, "rev-parse", "HEAD"))
        self.checkout = self.root / "data" / "fixture-repo"
        self.pins = self.root / "pins.cmake"

    @staticmethod
    def git(directory, *args):
        return subprocess.run(["git", "-C", str(directory), *args], check=True,
                              capture_output=True, text=True).stdout.strip()

    def run_setup(self, revision, update=False):
        self.pins.write_text(
            f'odr_test_data(PATH fixture-repo URL "{self.remote}" REVISION {revision})\n')
        return subprocess.run(
            ["cmake", f"-DODR_TEST_DATA_ROOT={self.root / 'data'}",
             f"-DODR_TEST_DATA_PINS={self.pins}", f"-DODR_TEST_DATA_UPDATE={'ON' if update else 'OFF'}",
             "-P", str(ROOT / "cmake/test_data.cmake")], capture_output=True, text=True)

    def assert_setup_succeeds(self, revision, update=False):
        result = self.run_setup(revision, update)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_interrupted_initialization_resumes(self):
        self.assertNotEqual(self.run_setup("f" * 40).returncode, 0)
        self.assertTrue((self.checkout / ".git").exists())
        self.assert_setup_succeeds(self.revisions[0])
        self.assertEqual(self.git(self.checkout, "rev-parse", "HEAD"), self.revisions[0])

        shutil.rmtree(self.checkout)
        self.checkout.mkdir()
        self.git(self.checkout, "init", "--quiet")
        (self.checkout / "fixture").write_text("untracked work")
        self.assertNotEqual(self.run_setup(self.revisions[0]).returncode, 0)
        self.assertEqual((self.checkout / "fixture").read_text(), "untracked work")
        (self.checkout / "fixture").unlink()
        self.assert_setup_succeeds(self.revisions[0])
        self.assertEqual((self.checkout / "fixture").read_text(), "first")

    def test_updates_preserve_local_changes_and_require_opt_in(self):
        self.assert_setup_succeeds(self.revisions[0])
        self.assert_setup_succeeds(self.revisions[1])
        self.assertEqual(self.git(self.checkout, "rev-parse", "HEAD"), self.revisions[0])
        (self.checkout / "fixture").write_text("local changes")
        self.assertNotEqual(self.run_setup(self.revisions[1], update=True).returncode, 0)
        self.assertEqual((self.checkout / "fixture").read_text(), "local changes")
        self.git(self.checkout, "checkout", "--", "fixture")
        self.assert_setup_succeeds(self.revisions[1], update=True)
        self.assertEqual(self.git(self.checkout, "rev-parse", "HEAD"), self.revisions[1])
