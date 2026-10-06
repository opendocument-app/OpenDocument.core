import json
import sys
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import release_status


class ReleaseStatusTest(unittest.TestCase):
    def test_tag_resolves_to_commit_not_annotated_tag_object(self):
        def gh(*args):
            return "commit" if "/commits/tags%2F" in args[1] else "tag-object"
        with mock.patch.object(release_status, "repository", return_value="owner/repo"), \
             mock.patch.object(release_status, "gh", side_effect=gh):
            self.assertEqual(release_status.release_commit("v1.2.3"), "commit")

    def test_runs_are_paginated_and_scoped_to_the_release_tag(self):
        def run(number, tag="v1.2.3", name="python"):
            return {"id": number, "run_number": number, "name": name,
                    "head_branch": tag, "event": "release", "head_sha": "commit"}
        pages = [{"workflow_runs": [run(100, "v1.2.4"), run(3), run(4)]},
                 {"workflow_runs": [run(1), run(2, name="conan")]}]
        def gh(*args):
            return json.dumps(pages if "--slurp" in args else pages[0])
        with mock.patch.object(release_status, "repository", return_value="owner/repo"), \
             mock.patch.object(release_status, "gh", side_effect=gh) as call:
            runs = release_status.runs_for("commit", "4", "v1.2.3")
        self.assertEqual({name: run["id"] for name, run in runs.items()},
                         {"python": 3, "conan": 2})
        self.assertIn("--paginate", call.call_args.args)
