import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class DevelopmentScriptsTest(unittest.TestCase):
    def test_generated_environment_preserves_literal_values(self):
        with tempfile.TemporaryDirectory(prefix="odr scripts '") as directory:
            root = Path(directory)
            (root / "scripts").mkdir()
            for name in ("gen-vscode-env.py", "run-with-env.sh"):
                shutil.copy2(ROOT / "scripts" / name, root / "scripts" / name)
            build = root / "build"
            build.mkdir()
            value = " space's \\\"quote\\\" # $HOME $(touch marker) `touch marker` \\path "
            preset = {"testPresets": [{"environment": {
                "ODR_LITERAL": value, "ODR_EMPTY": "", "ODR_PARENT": "$penv{ODR_PARENT}"
            }}]}
            (build / "CMakePresets.json").write_text(json.dumps(preset))
            subprocess.run([sys.executable, root / "scripts/gen-vscode-env.py", build],
                           cwd=root, env={**os.environ, "ODR_PARENT": "parent value"},
                           check=True, capture_output=True)
            result = subprocess.run(
                [root / "scripts/run-with-env.sh", sys.executable, "-c",
                 "import json,os; print(json.dumps([os.environ[k] for k in "
                 "['ODR_LITERAL','ODR_EMPTY','ODR_PARENT']]))"],
                cwd=root, check=True, capture_output=True, text=True,
            )
            self.assertEqual(json.loads(result.stdout), [value, "", "parent value"])
            self.assertFalse((root / "marker").exists())

    def test_setup_and_conan_stop_on_failure(self):
        with tempfile.TemporaryDirectory(prefix="odr scripts ") as directory:
            root = Path(directory)
            (root / "scripts/git_hooks").mkdir(parents=True)
            fake = '#!/bin/sh\nprintf "called\\n" >> calls\nexit 7\n'
            for name in ("conan", "scripts/git_hooks/git-hooks"):
                path = root / name
                path.write_text(fake)
                path.chmod(0o755)
            for name in ("conan_install", "conan_lock", "setup"):
                shutil.copy2(ROOT / "scripts" / name, root / "scripts" / name)
            for name in ("conan_install", "conan_lock", "setup"):
                with self.subTest(name=name):
                    (root / "calls").write_text("")
                    result = subprocess.run(
                        [root / "scripts" / name], cwd=root, capture_output=True,
                        env={**os.environ, "PATH": str(root) + os.pathsep + os.environ["PATH"]},
                    )
                    self.assertEqual(result.returncode, 7, result.stderr)
                    self.assertEqual((root / "calls").read_text(), "called\n")

    def test_hook_installation_from_a_worktree_with_spaces(self):
        with tempfile.TemporaryDirectory(prefix="odr hooks ") as directory:
            root = Path(directory) / "repo with spaces"
            root.mkdir()
            def git(*args, cwd=root, check=True):
                return subprocess.run(["git", *args], cwd=cwd, check=check,
                                      capture_output=True, text=True)
            git("init", "-q")
            git("config", "user.name", "Review")
            git("config", "user.email", "review@example.invalid")
            hooks = root / "scripts/git_hooks"
            (hooks / "pre-commit").mkdir(parents=True)
            shutil.copy2(ROOT / "scripts/git_hooks/git-hooks", hooks / "git-hooks")
            hook = hooks / "pre-commit/reject"
            hook.write_text("#!/bin/sh\nexit 7\n")
            hook.chmod(0o755)
            git("add", ".")
            git("commit", "-qm", "initial")
            worktree = Path(directory) / "worktree with spaces"
            git("worktree", "add", "-qb", "review", str(worktree))
            subprocess.run([worktree / "scripts/git_hooks/git-hooks", "--install"],
                           cwd=worktree, check=True, capture_output=True)
            self.assertTrue((root / ".git/hooks/pre-commit").exists())
            result = git("commit", "--allow-empty", "-m", "must fail", cwd=worktree,
                         check=False)
            self.assertNotEqual(result.returncode, 0, result.stdout)
