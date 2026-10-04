"""Propose one frozen upstream merge; never execute code from the merged tree."""
import base64
import json
import os
import re
import subprocess
import sys

PROFILES = {
    "chrismacdonaldw/Shipwright": "HarbourMasters/Shipwright",
    "chrismacdonaldw/2ship2harkinian": "2ship2harkinian/2ship2harkinian",
}
BASE = "develop"
PREFIX = "sync/upstream-"


def run(*args, env=None):
    return subprocess.check_output(args, text=True, encoding="utf-8", env=env).rstrip("\r\n")


def api(path, *args):
    return json.loads(run("gh", "api", path, *args))


def sha(value):
    if not re.fullmatch(r"[0-9a-f]{40}", value):
        raise ValueError("Invalid commit identity")
    return value


def unsafe_paths(paths):
    # Workflow definitions, local actions and executable CI automation need review.
    return [p for p in paths if p == ".github" or p.startswith(".github/")]


def outstanding(pulls):
    return any(p["head"]["ref"].startswith(PREFIX) for p in pulls)


def validate_proposal(repo, proposal):
    head = proposal["head"]
    branch = head["ref"]
    if not re.fullmatch(re.escape(PREFIX) + r"[0-9a-f]{40}", branch) or head["repo"]["full_name"] != repo:
        raise ValueError("Outstanding sync proposal is not an owned frozen branch")
    candidate = sha(head["sha"])
    if api(f"repos/{repo}/git/ref/heads/{branch}")["object"]["sha"] != candidate:
        raise ValueError("Proposal moved; inspect it manually")
    run("git", "fetch", "--no-tags", "origin", branch)
    if run("git", "rev-parse", "FETCH_HEAD") != candidate:
        raise ValueError("Proposal moved during fetch")
    parents = run("git", "rev-list", "--parents", "-n", "1", candidate).split()
    if len(parents) != 3 or parents[2] != branch[len(PREFIX):]:
        raise ValueError("Proposal no longer matches its frozen upstream merge")
    if unsafe_paths(run("git", "diff", "--name-only", "-z", parents[1], candidate).split("\0")):
        raise ValueError("Proposal automation changed; manual review required")
    # First-parent metadata is not itself a trust anchor after a user branch edit.
    trusted = sha(api(f"repos/{repo}/git/ref/heads/{BASE}")["object"]["sha"])
    run("git", "fetch", "--no-tags", "origin", BASE)
    if run("git", "rev-parse", trusted + ":.github") != run("git", "rev-parse", candidate + ":.github"):
        raise ValueError("Proposal automation differs from the maintained default; manual review required")
    note("Frozen proposal retained. Its required PR validation and review must pass before merging; diagnostic dispatch does not satisfy required checks.")


def note(message):
    print(message)
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as out:
            out.write(message + "\n")


def main():
    repo = os.environ["GITHUB_REPOSITORY"]
    upstream = PROFILES[repo]
    if os.environ["GITHUB_REF"] != "refs/heads/" + BASE:
        raise ValueError("Sync only runs from the maintained default branch")
    info = api("repos/" + repo)
    if info["default_branch"] != BASE or info["parent"]["full_name"] != upstream:
        raise ValueError("Repository/default/upstream identity changed; review configuration")
    pulls = api(f"repos/{repo}/pulls?state=open&base={BASE}&per_page=100")
    if len(pulls) == 100:
        raise ValueError("PR listing incomplete; inspect outstanding proposals")
    if outstanding(pulls):
        proposals = [p for p in pulls if p["head"]["ref"].startswith(PREFIX)]
        if len(proposals) != 1:
            raise ValueError("Multiple sync proposals require manual review")
        validate_proposal(repo, proposals[0])
        note("An upstream sync PR is already open. New commits wait until it is handled.")
        return
    base = sha(api(f"repos/{repo}/git/ref/heads/{BASE}")["object"]["sha"])
    tip = sha(api(f"repos/{upstream}/git/ref/heads/{BASE}")["object"]["sha"])
    run("git", "fetch", "--no-tags", "origin", BASE)
    run("git", "fetch", "--no-tags", "https://github.com/" + upstream + ".git", BASE)
    if run("git", "rev-parse", "FETCH_HEAD") != tip:
        raise ValueError("Upstream moved during fetch; rerun")
    run("git", "checkout", "--detach", base)
    if subprocess.run(["git", "merge-base", "--is-ancestor", tip, base]).returncode == 0:
        note("No upstream commits to integrate.")
        return
    paths = run("git", "diff", "--name-only", "-z", base + "..." + tip).split("\0")
    blocked = unsafe_paths(paths)
    if blocked:
        raise ValueError("Upstream automation changes require a manual integration PR: " + ", ".join(blocked))
    branch = PREFIX + tip
    refs = api(f"repos/{repo}/git/matching-refs/heads/{branch}")
    if refs:
        raise ValueError("This frozen proposal branch already exists; inspect its PR/run instead of rewriting it")
    run("git", "config", "user.name", "github-actions[bot]")
    run("git", "config", "user.email", "41898282+github-actions[bot]@users.noreply.github.com")
    run("git", "checkout", "-b", branch)
    merged = subprocess.run(["git", "-c", "core.hooksPath=/dev/null", "merge", "--no-ff", tip,
                             "-m", "merge: incorporate upstream develop"])
    if merged.returncode:
        conflicts = run("git", "diff", "--name-only", "--diff-filter=U")
        raise ValueError("Merge conflicts require manual integration; no branch published: " + conflicts)
    candidate = sha(run("git", "rev-parse", "HEAD"))
    # Include merged result in the automation boundary, not just upstream's delta.
    if unsafe_paths(run("git", "diff", "--name-only", "-z", base, candidate).split("\0")):
        raise ValueError("Merged automation changed; manual review required")
    if api(f"repos/{repo}/git/ref/heads/{BASE}")["object"]["sha"] != base:
        raise ValueError("Target moved before publication; rerun")
    if api(f"repos/{upstream}/git/ref/heads/{BASE}")["object"]["sha"] != tip:
        raise ValueError("Upstream moved before publication; rerun")
    final_pulls = api(f"repos/{repo}/pulls?state=open&base={BASE}&per_page=100")
    if len(final_pulls) == 100:
        raise ValueError("PR listing incomplete before publication")
    if outstanding(final_pulls):
        raise ValueError("Another sync proposal appeared; no branch published")
    # Auth exists only for this trusted Git push; no merged scripts or hooks run.
    auth = base64.b64encode(("x-access-token:" + os.environ["GH_TOKEN"]).encode()).decode()
    push_env = dict(os.environ, GIT_CONFIG_COUNT="1", GIT_CONFIG_KEY_0="http.https://github.com/.extraheader",
                    GIT_CONFIG_VALUE_0="AUTHORIZATION: basic " + auth)
    run("git", "-c", "core.hooksPath=/dev/null", "push", "origin", "HEAD:refs/heads/" + branch, env=push_env)
    pr = api(f"repos/{repo}/pulls", "--method", "POST", "-f", "title=merge: sync upstream develop",
             "-f", "head=" + branch, "-f", "base=" + BASE, "-F", "draft=true",
             "-f", "body=- Merge upstream develop while preserving fork changes.\n- Validate the merged result with the native Windows build and save regression.")
    note("Frozen candidate " + candidate + ": " + pr["html_url"] + ". Review and merge remain manual.")
    note("The App-created PR starts native PR validation. Review remains required; diagnostic dispatch does not satisfy required checks.")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, subprocess.CalledProcessError) as error:
        note("Sync stopped: " + str(error))
        sys.exit(1)
