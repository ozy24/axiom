#!/usr/bin/env python3
"""Every cvar a mode switch resets (core_gameplay_cvars in core/rulesets.cpp)
must reset to the default its module registers (`gi.cvar("name", "default",
...)` in rerelease/ or core/): otherwise a switch leaves a setting at a value
nobody chose. Fails on a mismatch, or on a listed cvar nothing registers.
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
if bad:
    sys.exit("check_gameplay_cvars:\n  " + "\n  ".join(bad))
print(f"check_gameplay_cvars: ok ({len(listed)} cvars)")
