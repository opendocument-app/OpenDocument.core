#!/usr/bin/env python3
"""The release procedure.

Dispatch `release.yml` against main and the run cuts the next version. To patch
an older line, branch off the *tag* — `git branch release/v6.1.X v6.1.0` — and
dispatch against that, since the version is derived against the nearest
reachable one.

Driven by `.github/workflows/release.yml`:

    version   what the commits since the last reachable tag say comes next
    changelog the CHANGELOG.md section a release would be built from
    cut       rename `## Unreleased` to the version, and open a fresh one
    notes     the release body: that section above the generated commit list
    stamp     commit whatever the workflow wrote into the tree, if anything
    publish   create or update the draft release, targeting HEAD, with assets

`stamp` does not know what it is committing — producing the files a release has
to carry is the workflow's business, this is only that they land in one commit.

Runnable by hand; `--dry-run` mutates nothing:

    scripts/release.py version
    scripts/release.py changelog --version v6.2.0
    scripts/release.py notes --version v6.2.0 --output /tmp/notes.md
    scripts/release.py publish --version v6.2.0 --notes /tmp/notes.md --dry-run
"""

from __future__ import annotations

import argparse
import datetime
import os
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
CLIFF_CONFIG = REPO_ROOT / ".github" / "cliff.toml"
CHANGELOG = REPO_ROOT / "CHANGELOG.md"
UNRELEASED = "Unreleased"

# `## v6.3.0 - 2026-08-08`, or `## Unreleased`; the date is decoration
VERSION_HEADING = re.compile(r"^## +(\S+)")

# skipped by `cliff.toml`, so it stays out of the next release's notes.
# `[skip ci]` because the app token pushing it does raise events, and the commit
# carries no code — the tree at the tag is the one main last built.
STAMP_SUBJECT = "chore(release): {version} [skip ci]"


def run(command: list[str], *, capture: bool = False, dry_run: bool = False) -> str:
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


def command_version(arguments: argparse.Namespace) -> None:
    """The next version, on stdout and nothing else — the workflow reads it."""
    version = arguments.version or cliff("--bumped-version", capture=True)

    # Deriving against the nearest *reachable* tag is what lets a maintenance
    # line version itself, and how it collides: a `feat:` on `release/v6.2.X`
    # bumps the minor to a number the mainline may already have shipped.
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


def section(text: str, wanted: str) -> str | None:
    """The body under `## <wanted>`, or None if there is no such heading."""
    wanted = wanted.strip().removeprefix("v")

    found = False
    body: list[str] = []
    for line in text.splitlines():
        heading = VERSION_HEADING.match(line)
        if heading:
            if found:
                break
            found = heading.group(1).removeprefix("v") == wanted
            continue
        if found:
            body.append(line)

    return "\n".join(body).strip("\n") if found else None


def changelog_section(version: str) -> str:
    """What the release body will be built from.

    `## <version>` once `cut` has run, `## Unreleased` before it — the same
    lines either way, so the workflow can read them before it builds anything.
    """
    text = CHANGELOG.read_text()
    body = section(text, version)
    if body is None:
        body = section(text, "Unreleased")
    if body is None:
        raise SystemExit(
            f"CHANGELOG.md has neither a `## {version}` nor a `## Unreleased` "
            f"section. One of them has to hold the release copy."
        )
    if not body.strip():
        raise SystemExit(
            f"CHANGELOG.md says nothing about {version}. Write what a consumer "
            f"would notice under `## Unreleased` — a release with nothing worth "
            f"telling them should say so, not nothing."
        )
    return body


def command_changelog(arguments: argparse.Namespace) -> None:
    print(changelog_section(arguments.version))


def command_cut(arguments: argparse.Namespace) -> None:
    """Rename `## Unreleased` to the version and open an empty one above it.

    A version that already has a section keeps it, so re-running a release
    neither cuts twice nor swallows what landed in between.
    """
    text = CHANGELOG.read_text()
    if section(text, arguments.version) is not None:
        print(f"{arguments.version} is already cut")
        return

    heading = f"## {UNRELEASED}\n"
    if heading not in text:
        raise SystemExit(f"CHANGELOG.md has no `## {UNRELEASED}` heading to cut")

    date = arguments.date or datetime.date.today().isoformat()
    cut = f"## {UNRELEASED}\n\n## {arguments.version} - {date}\n"
    print(f"cutting `## {UNRELEASED}` to `## {arguments.version} - {date}`")
    if not arguments.dry_run:
        CHANGELOG.write_text(text.replace(heading, cut, 1))


def command_notes(arguments: argparse.Namespace) -> None:
    """The hand-written section, then the commits it summarises."""
    summary = changelog_section(arguments.version)
    generated = cliff("--tag", arguments.version, "--latest", "--unreleased",
                      capture=True)
    arguments.output.write_text(f"{summary}\n\n---\n\n{generated}\n")


def command_stamp(arguments: argparse.Namespace) -> None:
    """Commit whatever the workflow wrote, and push it. A no-op if nothing did."""
    if arguments.branch != "main" and not arguments.branch.startswith("release/"):
        raise SystemExit(
            f"refusing to stamp {arguments.branch}: a release is cut from "
            f"`main`, or from a `release/v<major>.<minor>.X` branched off the "
            f"tag it patches"
        )

    # Asked before staging, so `--dry-run` reaches the same verdict as a real run.
    dirty = git("status", "--porcelain", "--untracked-files=no", capture=True)
    if not dirty:
        print("nothing to stamp — the tag will land on the commit as it is")
        return

    print(f"stamping:\n{dirty}")
    # `--update`: only tracked files, so a downloaded artifact is never swept in
    git("add", "--update", dry_run=arguments.dry_run)
    git("commit", "-m", STAMP_SUBJECT.format(version=arguments.version),
        dry_run=arguments.dry_run)
    git("push", "origin", f"HEAD:{arguments.branch}", dry_run=arguments.dry_run)


def command_publish(arguments: argparse.Namespace) -> None:
    """Create or update the draft release for `version`, targeting HEAD.

    A draft, always: GitHub does not create the tag until one is published,
    which is what lets the tag point at a commit made during the run.
    """
    target = git("rev-parse", "HEAD", capture=True)

    exists = subprocess.run(
        ["gh", "release", "view", arguments.version],
        cwd=REPO_ROOT,
        capture_output=True,
    ).returncode == 0

    if exists:
        # Re-running on a later push is normal; rewriting a published release
        # consumers may already have resolved is not.
        published = run(
            ["gh", "release", "view", arguments.version, "--json", "isDraft",
             "--jq", ".isDraft"],
            capture=True,
        )
        if published == "false":
            raise SystemExit(
                f"{arguments.version} is already published — cut a new version"
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

    changelog = subparsers.add_parser("changelog",
                                      help="print the CHANGELOG.md section")
    changelog.add_argument("--version", required=True)
    changelog.set_defaults(function=command_changelog)

    cut = subparsers.add_parser("cut", help="head the entries with the version")
    cut.add_argument("--version", required=True)
    cut.add_argument("--date", help="defaults to today")
    cut.add_argument("--dry-run", action="store_true")
    cut.set_defaults(function=command_cut)

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
