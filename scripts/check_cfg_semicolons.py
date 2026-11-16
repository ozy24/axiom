#!/usr/bin/env python3
"""No `;` outside quotes in a config file an admin is given: the shipped
examples (examples/**/*.cfg) and the config blocks of the docs an admin
copies from (docs/admin.md, docs/players.md, README.md).

The console splits its command buffer on `;` before it strips a `//`
comment, so the rest of a comment after a semicolon runs as a command
(found 2026-11-16: "below height 0; 0 = the rerelease's" ran `0`). A `;`
inside quotes is a value (g_start_items "weapon_railgun;ammo_slugs 50").
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def unquoted(line):
    return re.sub(r'"[^"]*"', '""', line)


bad = []
for path in sorted(ROOT.glob("examples/**/*.cfg")):
    for n, line in enumerate(path.read_text(errors="replace").splitlines(), 1):
        if ";" in unquoted(line):
            bad.append(f"{path.relative_to(ROOT)}:{n}: {line.strip()}")
for name in ("docs/admin.md", "docs/players.md", "README.md"):
    text = (ROOT / name).read_text()
    for block in re.finditer(r"```(?:cfg|)\n(.*?)```", text, re.S):
        start = text[:block.start()].count("\n") + 2
        for k, line in enumerate(block.group(1).splitlines()):
            if line.lstrip().startswith(("set ", "seta ", "//", "exec ", "kexmultiplayer ")) and ";" in unquoted(line):
                bad.append(f"{name}:{start + k}: {line.strip()}")
if bad:
    sys.exit("check_cfg_semicolons: a `;` outside quotes ends the command and runs the rest "
             "(even inside a // comment):\n  " + "\n  ".join(bad))
print("check_cfg_semicolons: ok")
