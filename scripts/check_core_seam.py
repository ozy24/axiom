#!/usr/bin/env python3
"""Logic stays off the engine (AGENTS.md: the Host seam).

Files under core/ and gametypes/ may include g_local.h (and so touch gi, level,
globals or g_edicts) only if they are adapters, named *_register.cpp or
*_gi.cpp. Everything else talks to the engine through Host and is unit-tested
with tests/fake_host.h. Adapters are kept out of the game_tests target.

    scripts/check_core_seam.py
"""

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]g_local\.h[>"]', re.MULTILINE)
ADAPTER = re.compile(r"(_register|_gi)\.cpp$")


def main():
    problems = []
    for top in ("core", "gametypes"):
        for path in sorted((ROOT / top).rglob("*")) if (ROOT / top).exists() else []:
            if path.suffix not in (".cpp", ".h") or not path.is_file():
                continue
            if INCLUDE.search(path.read_text(encoding="utf-8")) and not ADAPTER.search(path.name):
                problems.append(str(path.relative_to(ROOT)))
    if problems:
        print("check_core_seam: FAIL - logic files include g_local.h:")
        for p in problems:
            print(f"  {p}")
        print("Use Host (core/host.h) instead, or move the engine calls into a *_register.cpp / *_gi.cpp adapter.")
        sys.exit(1)
    print("check_core_seam: ok")


if __name__ == "__main__":
    main()
