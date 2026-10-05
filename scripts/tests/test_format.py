import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FORMATTER = shutil.which(os.environ.get("CLANG_FORMAT", "clang-format"))


@unittest.skipUnless(FORMATTER, "clang-format is required")
class FormatHookTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="odr format ")
        self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name)
        self.run_command("git", "init", "-q")
        (self.repo / ".clang-format").write_text("BasedOnStyle: LLVM\n")

    def run_command(self, *args, **kwargs):
        return subprocess.run(args, cwd=self.repo, capture_output=True,
                              check=True, **kwargs)

    def test_partial_staging_and_unusual_paths_remain_unchanged(self):
        self.run_command("git", "add", ".clang-format")
        self.run_command("git", "-c", "user.name=Review", "-c",
                         "user.email=review@example.invalid", "-c",
                         "core.hooksPath=/dev/null", "commit", "-qm", "initial")
        for name in ("file.cpp", "-space and\nnewline.cpp"):
            for staged, unstaged, succeeds in (
                ("int x = 1;\n", "int x=2;\n", True),
                ("int x=1;\n", "int x = 2;\n", False),
            ):
                with self.subTest(name=name, succeeds=succeeds):
                    path = self.repo / name
                    path.write_text(staged)
                    self.run_command("git", "add", "--", name)
                    path.write_text(unstaged)
                    index = (self.repo / ".git/index").read_bytes()
                    result = self.run_hook()
                    self.assertEqual(result.returncode == 0, succeeds, result.stderr)
                    self.assertEqual((self.repo / ".git/index").read_bytes(), index)
                    self.assertEqual(path.read_text(), unstaged)
                    self.run_command("git", "rm", "--cached", "-f", "--", name)

    def test_checks_initial_commit(self):
        (self.repo / "file.cpp").write_text("int x=1;\n")
        self.run_command("git", "add", "file.cpp")
        self.assertNotEqual(self.run_hook().returncode, 0)

    def run_hook(self):
        return subprocess.run(
            [ROOT / "scripts/git_hooks/pre-commit/0_format"],
            cwd=self.repo, capture_output=True,
            env={**os.environ, "CLANG_FORMAT": FORMATTER},
        )


class FormatScriptTest(unittest.TestCase):
    def test_formatter_failures_stop_the_script(self):
        with tempfile.TemporaryDirectory(prefix="odr format ") as directory:
            root = Path(directory)
            (root / "scripts").mkdir()
            shutil.copy2(ROOT / "scripts/format", root / "scripts/format")
            for name in ("src", "cli", "test/src", "python/src", "jni/src",
                         "apple/src", "apple/include", "wasm/src"):
                (root / name).mkdir(parents=True)
            (root / "wasm/src/file with spaces.cpp").write_text("int x;\n")
            formatter = root / "fake formatter"
            formatter.write_text('#!/bin/sh\n[ "$1" = --version ] && exit 0\nexit 7\n')
            formatter.chmod(0o755)
            result = subprocess.run(
                [root / "scripts/format"], cwd=directory, capture_output=True,
                env={**os.environ, "CLANG_FORMAT": str(formatter)},
            )
            self.assertEqual(result.returncode, 7, result.stderr)
            self.assertIn(b"wasm/src/file with spaces.cpp", result.stdout)
