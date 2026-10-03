"""Check changed native lines with the repository's existing clang-format version."""
import os
from pathlib import PurePosixPath
import re
import subprocess


def eligible(path, port):
    file = PurePosixPath(path)
    if not path.startswith(port + "/") or path.startswith(port + "/assets/"):
        return False
    if file.suffix in (".c", ".cpp"):
        return True
    headers = (".h", ".hpp") if port == "soh" else (".h",)
    return file.suffix in headers and not path.startswith((port + "/src/", port + "/include/"))


def main():
    base, head = os.environ["BASE_SHA"], os.environ["HEAD_SHA"]
    if not all(re.fullmatch(r"[0-9a-f]{40}", value) for value in (base, head)):
        raise ValueError("Invalid PR source identities")
    port = {"chrismacdonaldw/Shipwright": "soh", "chrismacdonaldw/2ship2harkinian": "mm",
            "HarbourMasters/Shipwright": "soh", "2ship2harkinian/2ship2harkinian": "mm"}[os.environ["GITHUB_REPOSITORY"]]
    base = subprocess.check_output(["git", "merge-base", base, head], text=True).strip()
    paths = subprocess.check_output(["git", "diff", "--name-only", "-z", "--diff-filter=ACMR", base, head]).decode("utf-8").split("\0")
    files = [path for path in paths if eligible(path, port)]
    if not files:
        print("No changed native lines to format.")
        return 0
    if any(any(character in path for character in '\r\n\t"\\') for path in files):
        raise ValueError("Unusual native filename requires manual formatting review")
    # LLVM14's patch parser needs Unicode names unquoted; do not change repo config.
    env = dict(os.environ, GIT_CONFIG_COUNT="1", GIT_CONFIG_KEY_0="core.quotePath", GIT_CONFIG_VALUE_0="false")
    result = subprocess.run(["git-clang-format-14", "--binary", "clang-format-14", "--diff", base, head, "--", *files],
                            text=True, encoding="utf-8", capture_output=True, env=env)
    print(result.stdout, end="")
    print(result.stderr, end="")
    return result.returncode or (1 if "diff --git " in result.stdout else 0)


if __name__ == "__main__":
    raise SystemExit(main())
