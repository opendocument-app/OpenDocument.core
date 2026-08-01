#!/usr/bin/env python3
"""The release procedure, in one place.

`main` carries no version. Nothing in the tree says 6.2.0, no file is bumped,
and `git log main` never mentions a release — the only thing a build records
about itself is `GIT_HEAD_SHA1` plus a dirty flag (`CMakeLists.txt`). The
version of a release is derived from its commits, and the tag is where it
lives.

`releases` is the mainline train: merge main into it and push, and the run below
cuts the next version. Its first-parent history is the release history, which is
the one thing main deliberately cannot tell you. When the train has already left
and an older line needs a patch, branch off the *tag* — `git branch
release/v6.1.X v6.1.0` — cherry-pick, and push that. Off the tag rather than off
`releases`, because the version is derived against the nearest reachable one.

The flow, driven by `.github/workflows/release.yml`:

    version   what the conventional commits since the last reachable tag say
              the next version is
    notes     the release body, from the same commits
    stamp     commit whatever the workflow wrote into the tree, if anything
    publish   create or update the *draft* release, targeting HEAD, with assets

`stamp` deliberately does not know what it is committing. Producing the files a
release has to carry is the workflow's business — today the xcframework
checksum in `Package.swift`, which cannot exist before the artifact is built —
and this script's business is only that they land in one commit that nothing
else in the history has to know about. If a run writes nothing, there is no
commit and the tag lands directly on the merge from main, which is the shape a
release should have when nothing forces otherwise.

Everything is runnable by hand. `--dry-run` prints what would be executed and
mutates nothing, which is how to try this against a scratch branch:

    scripts/release.py version
    scripts/release.py notes --version v6.2.0 --output /tmp/notes.md
    scripts/release.py publish --version v6.2.0 --notes /tmp/notes.md --dry-run
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
CLIFF_CONFIG = REPO_ROOT / ".github" / "cliff.toml"

# The stamp commit is skipped by `cliff.toml`'s parsers, so it never shows up in
# the next release's notes. Keep the two in step.
STAMP_SUBJECT = "chore(release): {version}"


def run(command: list[str], *, capture: bool = False, dry_run: bool = False) -> str:
    """Run a command, echoing it the way `set -x` would."""
    printable = " ".join(command)
    if dry_run:
        print(f"+ (dry run) {printable}", file=sys.stderr)
        return ""
    print(f"+ {printable}", file=sys.stderr)
    result = subprocess.run(
        command,
        cwd=REPO_ROOT,
        check=True,
        text=True,
        stdout=subprocess.PIPE if capture else None,
    )
    return (result.stdout or "").strip()


def git(*arguments: str, capture: bool = False, dry_run: bool = False) -> str:
    return run(["git", *arguments], capture=capture, dry_run=dry_run)


def cliff(*arguments: str, capture: bool = False) -> str:
    environment = os.environ.copy()
    environment["GIT_CLIFF_CONFIG"] = str(CLIFF_CONFIG)
    command = ["git-cliff", *arguments]
    print(f"+ {' '.join(command)}", file=sys.stderr)
    result = subprocess.run(
        command,
        cwd=REPO_ROOT,
        env=environment,
        check=True,
        text=True,
        stdout=subprocess.PIPE if capture else None,
    )
    return (result.stdout or "").strip()


def head() -> str:
    return git("rev-parse", "HEAD", capture=True)


def command_version(arguments: argparse.Namespace) -> None:
    """The next version, on stdout and nothing else — the workflow reads it."""
    version = arguments.version or cliff("--bumped-version", capture=True)

    # A bump is computed against the last *reachable* tag, which is what lets a
    # maintenance line version itself without knowing the mainline exists — and
    # is also how it can collide with it. `release/v6.2.X` branched from
    # `v6.2.0` sees only that tag, so a `feat:` on it bumps to `v6.3.0`, which
    # the mainline may already have shipped. A maintenance line usually wants
    # `fix:` only; where it genuinely does not, say the version outright.
    tagged = subprocess.run(
        ["git", "rev-parse", "--verify", "--quiet", f"refs/tags/{version}"],
        cwd=REPO_ROOT,
        capture_output=True,
    ).returncode == 0
    if tagged and not arguments.version:
        raise SystemExit(
            f"the commits say {version}, but {version} is already tagged — a "
            f"maintenance line has bumped into a version the mainline used. "
            f"Re-run with an explicit --version."
        )

    print(version)


def command_notes(arguments: argparse.Namespace) -> None:
    """The release body for `version`, from the commits since the last tag."""
    cliff("--tag", arguments.version, "--latest", "--unreleased",
          "-o", str(arguments.output))


def command_stamp(arguments: argparse.Namespace) -> None:
    """Commit whatever the workflow wrote, and push it. A no-op if nothing did.

    `add --update` on purpose: only files git already tracks. A release run
    downloads artifacts, and none of them may ever be swept into the commit by
    an `add -A` that was convenient at the time.
    """
    # A dispatch from the wrong branch would otherwise push a version commit
    # onto it. Releases come from `releases` or a maintenance line; nothing else.
    if arguments.branch != "releases" and not arguments.branch.startswith("release/"):
        raise SystemExit(
            f"refusing to stamp {arguments.branch}: a release is cut from "
            f"`releases`, or from a `release/v<major>.<minor>.X` branched off "
            f"the tag it patches"
        )

    # Asked before staging rather than after, so `--dry-run` reports the same
    # decision the real run would take instead of always seeing a clean index.
    dirty = git("status", "--porcelain", "--untracked-files=no", capture=True)
    if not dirty:
        print("nothing to stamp — the tag will land on the commit as it is")
        return

    print(f"stamping:\n{dirty}")
    git("add", "--update", dry_run=arguments.dry_run)
    git("commit", "-m", STAMP_SUBJECT.format(version=arguments.version),
        dry_run=arguments.dry_run)
    git("push", "origin", f"HEAD:{arguments.branch}", dry_run=arguments.dry_run)


def command_publish(arguments: argparse.Namespace) -> None:
    """Create or update the draft release for `version`, targeting HEAD.

    A draft, always. GitHub does not create the tag until a release is
    published, which is the whole reason the tag can point at a commit that did
    not exist when the run started. Publishing stays a human action — and it
    has to be, because a release created by `GITHUB_TOKEN` does not raise the
    `release: published` event that conan, maven and android hang off.
    """
    target = head()

    exists = subprocess.run(
        ["gh", "release", "view", arguments.version],
        cwd=REPO_ROOT,
        capture_output=True,
    ).returncode == 0

    if exists:
        # Re-running on a later push to the same release branch is normal:
        # merge another fix in and the draft is rewritten. A release that is
        # already out is not ours to rewrite.
        published = run(
            ["gh", "release", "view", arguments.version, "--json", "isDraft",
             "--jq", ".isDraft"],
            capture=True,
        )
        if published == "false":
            raise SystemExit(
                f"{arguments.version} is already published — cut a new version "
                f"rather than rewriting a release consumers may have resolved"
            )
        run(["gh", "release", "edit", arguments.version,
             "--draft", "--target", target, "--notes-file", str(arguments.notes)],
            dry_run=arguments.dry_run)
    else:
        run(["gh", "release", "create", arguments.version,
             "--draft", "--target", target, "--title", arguments.version,
             "--notes-file", str(arguments.notes)],
            dry_run=arguments.dry_run)

    if arguments.asset:
        run(["gh", "release", "upload", arguments.version,
             *[str(asset) for asset in arguments.asset], "--clobber"],
            dry_run=arguments.dry_run)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    subparsers = parser.add_subparsers(dest="command", required=True)

    version = subparsers.add_parser("version", help="print the next version")
    version.add_argument("--version", help="override what the commits say")
    version.set_defaults(function=command_version)

    notes = subparsers.add_parser("notes", help="write the release body")
    notes.add_argument("--version", required=True)
    notes.add_argument("--output", type=Path, required=True)
    notes.set_defaults(function=command_notes)

    stamp = subparsers.add_parser("stamp", help="commit and push what the run wrote")
    stamp.add_argument("--version", required=True)
    stamp.add_argument("--branch", required=True, help="branch to push to")
    stamp.add_argument("--dry-run", action="store_true")
    stamp.set_defaults(function=command_stamp)

    publish = subparsers.add_parser("publish", help="create or update the draft")
    publish.add_argument("--version", required=True)
    publish.add_argument("--notes", type=Path, required=True)
    publish.add_argument("--asset", type=Path, action="append", default=[])
    publish.add_argument("--dry-run", action="store_true")
    publish.set_defaults(function=command_publish)

    arguments = parser.parse_args()
    arguments.function(arguments)


if __name__ == "__main__":
    main()
