#!/usr/bin/env python3
"""Watches a published release fan out, and makes a partial one loud.

Publishing starts one workflow per destination, each re-runnable on its own —
and nothing tells you whether they all worked. This waits for them and writes
the outcome onto the release between markers, exiting non-zero if one failed or
never started. `EXPECTED` is the only complete list of where a release goes.

Read-only trial run:

    scripts/release_status.py --tag v6.1.0 --once --dry-run
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time

# workflow `name:` -> what publishing it means; not listed is not watched
EXPECTED = {
    "conan": "conan package (`conan.yml`)",
    "maven": "`app.opendocument:odr-core-java` — Maven Central + GitHub Packages",
    "android": "`app.opendocument:odr-core-android` — Maven Central + GitHub Packages",
    "python": "`pyodr` wheels — PyPI",
    "apple": "`OdrCoreObjC.xcframework` — the release asset and its manifest",
}

BEGIN = "<!-- release-status -->"
END = "<!-- /release-status -->"

SYMBOL = {
    "success": "✅",
    "failure": "❌",
    "cancelled": "⚪",
    "timed_out": "⏱️",
    "skipped": "⏭️",
    "startup_failure": "❌",
}


def gh(*arguments: str) -> str:
    result = subprocess.run(
        ["gh", *arguments], check=True, text=True, stdout=subprocess.PIPE
    )
    return result.stdout.strip()


def repository() -> str:
    return os.environ.get("GITHUB_REPOSITORY") or gh(
        "repo", "view", "--json", "nameWithOwner", "--jq", ".nameWithOwner"
    )


def release_commit(tag: str) -> str:
    return gh("api", f"repos/{repository()}/git/ref/tags/{tag}", "--jq",
              ".object.sha")


def runs_for(commit: str, self_run_id: str | None) -> dict[str, dict]:
    """Latest release-triggered run per workflow — a re-run replaces the failure."""
    payload = json.loads(gh(
        "api", "-X", "GET", f"repos/{repository()}/actions/runs",
        "-f", "event=release", "-f", f"head_sha={commit}", "-f", "per_page=100",
    ))

    latest: dict[str, dict] = {}
    for run in payload.get("workflow_runs", []):
        if self_run_id and str(run["id"]) == str(self_run_id):
            continue
        name = run["name"]
        if name not in EXPECTED:
            continue
        seen = latest.get(name)
        if seen is None or run["run_number"] > seen["run_number"]:
            latest[name] = run
    return latest


def report(tag: str, runs: dict[str, dict]) -> tuple[str, bool]:
    lines = [BEGIN, "", f"### Publishing {tag}", ""]
    ok = True

    for name, what in EXPECTED.items():
        run = runs.get(name)
        if run is None:
            lines.append(f"- ❌ **{name}** — did not start · {what}")
            ok = False
            continue

        conclusion = run["conclusion"] or run["status"]
        symbol = SYMBOL.get(conclusion, "❓")
        if conclusion != "success":
            ok = False
        lines.append(
            f"- {symbol} **{name}** — [{conclusion}]({run['html_url']}) · {what}"
        )

    lines += ["", END]
    return "\n".join(lines), ok


def amend_release_notes(tag: str, block: str, *, dry_run: bool) -> None:
    """Replace the status block in the release body, or append one."""
    body = gh("api", f"repos/{repository()}/releases/tags/{tag}", "--jq", ".body")

    if BEGIN in body and END in body:
        head, _, rest = body.partition(BEGIN)
        _, _, tail = rest.partition(END)
        body = f"{head.rstrip()}\n\n{block}\n{tail.lstrip()}"
    else:
        body = f"{body.rstrip()}\n\n{block}\n"

    if dry_run:
        print("--- release body would become ---")
        print(body)
        return

    subprocess.run(
        ["gh", "release", "edit", tag, "--notes-file", "-"],
        check=True, text=True, input=body,
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--self-run-id", help="this run, so it is not watched")
    parser.add_argument("--timeout-minutes", type=int, default=110)
    parser.add_argument("--poll-seconds", type=int, default=60)
    parser.add_argument("--once", action="store_true",
                        help="report the state as it is and stop waiting")
    parser.add_argument("--dry-run", action="store_true",
                        help="print the release body instead of writing it")
    arguments = parser.parse_args()

    commit = release_commit(arguments.tag)
    print(f"{arguments.tag} is {commit}")

    deadline = time.monotonic() + arguments.timeout_minutes * 60
    while True:
        runs = runs_for(commit, arguments.self_run_id)
        pending = [
            name for name in EXPECTED
            if name in runs and runs[name]["status"] != "completed"
        ]
        # absence may just mean not registered yet, so it waits rather than fails
        missing = [name for name in EXPECTED if name not in runs]

        if arguments.once or (not pending and not missing):
            break
        if time.monotonic() >= deadline:
            print(f"timed out waiting for: {', '.join(pending + missing)}")
            break

        print(f"waiting for {', '.join(pending + missing)} …")
        time.sleep(arguments.poll_seconds)

    block, ok = report(arguments.tag, runs)
    print(block)

    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as handle:
            handle.write(block.replace(BEGIN, "").replace(END, "") + "\n")

    amend_release_notes(arguments.tag, block, dry_run=arguments.dry_run)

    if not ok:
        raise SystemExit(f"{arguments.tag} did not publish everywhere")
    print(f"{arguments.tag} published everywhere")


if __name__ == "__main__":
    main()
