#!/usr/bin/env python3
"""Enforce the renamability principle: the mod's name lives only in mod.json.

Reads `name` and `id` from mod.json and fails if either appears (any case) in
any tracked file other than:

  * mod.json itself;
  * documentation (*.md, docs/), which describes the project by name;
  * the GitHub repository slug (owner/<id>), which names the repo rather than
    the mod; repository URLs are checked against mod.json's "repository".

Everything else - code, build files, scripts, configs, CI - must get the name
from mod.json at build time (cmake/identity.cmake). The rename drill
(scripts/rename_drill.sh) proves it end to end on the built binary.

    scripts/check_name_leak.py
"""

import json
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def tracked_files():
    out = subprocess.run(["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
                         cwd=ROOT, capture_output=True, check=True).stdout
    return [p for p in out.decode().split("\0") if p]


def exempt(path):
    return path == "mod.json" or path.endswith(".md") or path.startswith("docs/")


def main():
    mod = json.loads((ROOT / "mod.json").read_text())
    names = {mod["name"], mod["id"]}
    repo = mod.get("repository", "")
    slug = re.sub(r"^https?://github\.com/", "", repo).rstrip("/")
    pattern = re.compile("|".join(re.escape(n) for n in sorted(names)), re.IGNORECASE)

    problems = []
    for path in tracked_files():
        full = ROOT / path
        if not full.is_file():
            continue
        try:
            text = full.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue  # binaries are checked by the drill, on the build output
        for lineno, line in enumerate(text.splitlines(), 1):
            if exempt(path):
                continue
            cleaned = line.replace(slug, "") if slug else line
            if pattern.search(cleaned):
                problems.append(f"{path}:{lineno}: {line.strip()[:100]}")

    if problems:
        print("check_name_leak: FAIL - the mod's name appears outside mod.json:")
        for p in problems[:50]:
            print(f"  {p}")
        print("Get it from mod.json instead (cmake/identity.cmake, mod_identity.h).")
        sys.exit(1)
    print(f"check_name_leak: ok ({', '.join(sorted(names))} only in mod.json and docs)")


if __name__ == "__main__":
    main()
