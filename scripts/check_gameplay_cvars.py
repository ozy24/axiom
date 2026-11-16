#!/usr/bin/env python3
"""Every cvar a mode switch resets (core_gameplay_cvars in core/rulesets.cpp)
must reset to the default its module registers (`gi.cvar("name", "default",
...)` in rerelease/ or core/): otherwise a switch leaves a setting at a value
nobody chose. Fails on a mismatch, or on a listed cvar nothing registers.
Every listed gameplay cvar must also be in docs/admin.md's "Stock's gameplay
settings" table, with its default (`| `name` | default |`). The match
settings, limits and map list are described elsewhere in the guide, at
their built-in rulesets' values, so they are left out.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
table = (ROOT / "core" / "rulesets.cpp").read_text()
table = table[table.index("const GameplayCvar core_gameplay_cvars[]"):]
table = table[:table.index("\n};")]
listed = re.findall(r'\{"(\w+)", "([^"]*)"\}', table)

registered = {}
for path in list((ROOT / "rerelease").rglob("*.cpp")) + list((ROOT / "core").rglob("*.cpp")):
    for name, value in re.findall(r'gi\.cvar\(\s*"(\w+)"\s*,\s*"([^"]*)"', path.read_text(errors="replace")):
        registered.setdefault(name, set()).add(value)

bad = []
for name, value in listed:
    if name not in registered:
        bad.append(f"{name}: listed, but no gi.cvar registers it")
    elif registered[name] != {value}:
        bad.append(f"{name}: reset to \"{value}\", registered as {sorted(registered[name])}")
guide = (ROOT / "docs" / "admin.md").read_text()
for name, value in listed:
    if name in ("g_gametype", "ctf", "teamplay") or name.startswith("match_") or name in (
            "timelimit", "fraglimit", "capturelimit", "g_map_list", "g_map_list_shuffle"):
        continue  # the gametype itself, or described with the match and maps
    row = re.search(rf"^\| `{re.escape(name)}` \| ([^|]*) \|", guide, re.M)
    if not row:
        bad.append(f"{name}: no row in docs/admin.md (| `{name}` | default | what |)")
    elif row.group(1).strip() not in (value, "(empty)" if value == "" else value):
        bad.append(f"{name}: docs/admin.md says the default is {row.group(1).strip()!r}, the code {value!r}")
if bad:
    sys.exit("check_gameplay_cvars:\n  " + "\n  ".join(bad))
print(f"check_gameplay_cvars: ok ({len(listed)} cvars)")
