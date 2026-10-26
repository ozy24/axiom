#!/usr/bin/env python3
"""Every change to id's code is marked (AGENTS.md workflow rule).

Diffs rerelease/ against the stock-reference tag and fails if any changed
hunk carries no `[mod]` marker in its added lines. A pure deletion hunk has
nothing to mark, so it must sit next to a marked line instead; it is reported
too, so deletions get a marker comment left in their place.

    scripts/check_mod_markers.py
"""

import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def main():
    try:
        diff = subprocess.run(["git", "diff", "-U0", "stock-reference", "--", "rerelease"],
                              cwd=ROOT, capture_output=True, text=True, check=True).stdout
    except subprocess.CalledProcessError as e:
        sys.exit(f"check_mod_markers: cannot diff against stock-reference ({e.stderr.strip()}); "
                 "is the tag fetched? (CI: fetch-depth 0)")
    problems, path, hunk, added = [], None, None, []

    def close():
        if hunk and not any("[mod]" in a for a in added):
            problems.append(f"{path} {hunk}")

    for line in diff.splitlines():
        if line.startswith("+++ "):
            close(); hunk = None
            path = line[6:] if line.startswith("+++ b/") else line[4:]
        elif line.startswith("@@"):
            close()
            hunk, added = re.match(r"@@ [^@]+ @@", line).group(0), []
        elif line.startswith("+") and not line.startswith("+++"):
            added.append(line)
    close()

    if problems:
        print("check_mod_markers: FAIL - changes to id's code without a [mod] marker:")
        for p in problems:
            print(f"  {p}")
        print("Add `// [mod] <why>` to the changed lines (or one in place of deleted ones).")
        sys.exit(1)
    count = diff.count("\n@@")
    print(f"check_mod_markers: ok ({count} marked hunk(s) in rerelease/)")


if __name__ == "__main__":
    main()
