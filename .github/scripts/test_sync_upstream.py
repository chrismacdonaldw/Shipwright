import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("sync", Path(__file__).with_name("sync_upstream.py"))
sync = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sync)
BASE, TIP, CANDIDATE = "1" * 40, "2" * 40, "3" * 40


class ProposalTests(unittest.TestCase):
    def exercise(self, *, noop=False, conflict=False, open_pr=False, unsafe=False, race=False):
        calls = []
        reads = 0

        def api(path, *args):
            nonlocal reads
            calls.append((path, args))
            if path.endswith("/Shipwright"):
                return {"default_branch": "develop", "parent": {"full_name": "HarbourMasters/Shipwright"}}
            if "/pulls?" in path:
                return [{"head": {"ref": "sync/upstream-" + TIP, "sha": CANDIDATE,
                                   "repo": {"full_name": "chrismacdonaldw/Shipwright"}}}] if open_pr else []
            if "/runs?" in path:
                return {"total_count": 1}
            if "matching-refs" in path:
                return []
            if "/git/ref/" in path:
                if "sync/upstream-" in path:
                    return {"object": {"sha": CANDIDATE}}
                if "HarbourMasters" in path:
                    return {"object": {"sha": TIP}}
                reads += 1
                return {"object": {"sha": CANDIDATE if race and reads > 1 else BASE}}
            if path.endswith("/pulls"):
                return {"html_url": "https://github.com/chrismacdonaldw/Shipwright/pull/3"}
            raise AssertionError(path)

        def run(*args, env=None):
            calls.append((args, env))
            if args == ("git", "rev-parse", "FETCH_HEAD"):
                return CANDIDATE if open_pr else TIP
            if "rev-list" in args:
                return " ".join((CANDIDATE, BASE, TIP))
            if args == ("git", "rev-parse", "HEAD"):
                return CANDIDATE
            if "rev-parse" in args and args[-1].endswith(":.github"):
                return "4" * 40
            if "diff" in args:
                return ".github/workflows/évil.yml\0" if unsafe else "soh/source.cpp\0"
            return ""

        def command(args, **kwargs):
            calls.append((args, kwargs))
            if "merge-base" in args:
                return subprocess.CompletedProcess(args, 0 if noop else 1)
            if "merge" in args:
                return subprocess.CompletedProcess(args, 1 if conflict else 0)
            return subprocess.CompletedProcess(args, 0)

        with patch.dict(os.environ, {"GITHUB_REPOSITORY": "chrismacdonaldw/Shipwright", "GITHUB_REF": "refs/heads/develop", "GH_TOKEN": "test-token"}), patch.object(sync, "api", api), patch.object(sync, "run", run), patch.object(sync.subprocess, "run", command), patch.object(sync, "note"):
            try:
                sync.main()
                error = None
            except ValueError as exception:
                error = str(exception)
        return calls, error

    def test_noop_has_no_mutation(self):
        calls, error = self.exercise(noop=True)
        self.assertIsNone(error)
        self.assertFalse(any("push" in c[0] or "dispatches" in str(c[0]) for c in calls))

    def test_open_pr_queues_new_tip(self):
        calls, error = self.exercise(open_pr=True)
        self.assertIsNone(error)
        self.assertFalse(any("push" in c[0] or "dispatches" in str(c[0]) for c in calls))

    def test_conflict_never_publishes(self):
        calls, error = self.exercise(conflict=True)
        self.assertIn("conflicts", error)
        self.assertFalse(any("push" in c[0] for c in calls))

    def test_automation_changes_hold(self):
        calls, error = self.exercise(unsafe=True)
        self.assertIn("manual integration", error)
        self.assertFalse(any("merge" in c[0] for c in calls))

    def test_target_race_stops_push(self):
        calls, error = self.exercise(race=True)
        self.assertIn("Target moved", error)
        self.assertFalse(any("push" in c[0] for c in calls))

    def test_candidate_pr_is_frozen_without_dispatch(self):
        calls, error = self.exercise()
        self.assertIsNone(error)
        push = next(c for c in calls if "push" in c[0])
        self.assertEqual(push[0][-1], "HEAD:refs/heads/sync/upstream-" + TIP)
        self.assertNotIn("--force", push[0])
        self.assertFalse(any("dispatches" in str(c[0]) for c in calls))
        pr = next(c for c in calls if isinstance(c[0], str) and c[0].endswith("/pulls"))
        self.assertIn("head=sync/upstream-" + TIP, pr[1])

    def test_invalid_sha_refuses(self):
        with self.assertRaises(ValueError):
            sync.sha("develop; command")

    def test_exact_github_path_is_unsafe(self):
        self.assertEqual(sync.unsafe_paths([".github", ".github/workflows/évil.yml", "soh/é.cpp"]),
                         [".github", ".github/workflows/évil.yml"])

    def test_real_git_unicode_path_is_unquoted(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(["git", "init", "-q", directory], check=True)
            subprocess.run(["git", "-C", directory, "config", "user.name", "Fixture"], check=True)
            subprocess.run(["git", "-C", directory, "config", "user.email", "fixture@example.invalid"], check=True)
            subprocess.run(["git", "-C", directory, "commit", "-q", "--allow-empty", "-m", "chore: seed"], check=True)
            path = root / ".github/workflows/évil.yml"
            path.parent.mkdir(parents=True)
            path.write_text("name: fixture\n", encoding="utf-8")
            subprocess.run(["git", "-C", directory, "add", ".github"], check=True)
            paths = sync.run("git", "-C", directory, "diff", "--cached", "--name-only", "-z").split("\0")
            self.assertEqual(sync.unsafe_paths(paths), [".github/workflows/évil.yml"])

    def test_open_proposal_waits_for_real_pr_validation(self):
        proposal = {"head": {"ref": sync.PREFIX + TIP, "sha": CANDIDATE,
                              "repo": {"full_name": "chrismacdonaldw/Shipwright"}}}
        def api(path):
            return {"total_count": 0} if "/runs?" in path else {"object": {"sha": CANDIDATE}}
        def run(*args):
            if "rev-list" in args:
                return " ".join((CANDIDATE, BASE, TIP))
            return CANDIDATE if "rev-parse" in args else ""
        with patch.object(sync, "api", api), patch.object(sync, "run", run), patch.object(sync, "note") as note:
            sync.validate_proposal("chrismacdonaldw/Shipwright", proposal)
            self.assertIn("required PR validation", note.call_args.args[0])

    def test_rewritten_validator_cannot_be_dispatched(self):
        proposal = {"head": {"ref": sync.PREFIX + TIP, "sha": CANDIDATE,
                              "repo": {"full_name": "chrismacdonaldw/Shipwright"}}}
        def run(*args):
            if "rev-list" in args:
                return " ".join((CANDIDATE, BASE, TIP))
            if "rev-parse" in args:
                return CANDIDATE if args[-1] == "FETCH_HEAD" else args[-1].split(":")[0]
            return ""
        def api(path):
            return {"object": {"sha": CANDIDATE if "sync/upstream-" in path else BASE}}
        with patch.object(sync, "api", api), patch.object(sync, "run", run):
            with self.assertRaisesRegex(ValueError, "maintained default"):
                sync.validate_proposal("chrismacdonaldw/Shipwright", proposal)


if __name__ == "__main__":
    unittest.main()
