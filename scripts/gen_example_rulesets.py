#!/usr/bin/env python3
"""Writes examples/server/rulesets/<gametype>.cfg from the built-in rulesets
in core/rulesets.cpp: every built-in setting, commented out at its built-in
value, so an admin sees what they get with no file and uncomments what they
change. The release zip ships them (docs/admin.md).

    python3 -I scripts/gen_example_rulesets.py           # rewrite the files
    python3 -I scripts/gen_example_rulesets.py --check   # fail if they differ

scripts/check.sh runs --check, so the examples can't drift from the code:
after changing a built-in ruleset, run this and commit the files with it.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE = ROOT / "core" / "rulesets.cpp"
OUT = ROOT / "examples" / "server" / "rulesets"

TITLES = {"ffa": "Free For All", "duel": "Duel", "tdm": "Team Deathmatch", "ctf": "Capture The Flag"}

# Stock maps only: every player has them (Chris, 2026-11-12).
MAP_LISTS = {
    "ffa": "q2dm1 q2dm2 q2dm3 q2dm4 q2dm5 q2dm6 q2dm7 q2dm8",
    "duel": "q2dm1 q2dm2 q2dm3 q64/dm3 q64/dm4 q64/dm8",  # as the ranked duel server
    "tdm": "q2dm1 q2dm2 q2dm3 q2dm4 q2dm5 q2dm6 q2dm7 q2dm8",
    "ctf": "q2ctf1 q2ctf2 q2ctf3 q2ctf4 q2ctf5",
}

# These say which gametype the file is for: never an admin's to change.
GAMETYPE_CVARS = {"g_gametype", "ctf", "teamplay"}

# One line each, as docs/admin.md has them. A built-in setting missing here
# fails the run, so a new one gets a description.
MEANING = {
    "match_flow": "warmup, countdown, live match, intermission; 0 = stock",
    "match_ready": "everyone types `ready` to start; 0 = start once enough are in",
    "match_minplayers": "players needed to start, bots counted",
    "match_allow_no_humans": "1 = bots alone may play a match (a bot server)",
    "match_warmup_time": "seconds of warmup before starting anyway; 0 = wait",
    "match_countdown": "seconds from all ready to the start",
    "match_overtime": "on a tie: N-minute overtimes; -1 = sudden death; 0 = it stays tied",
    "match_overtime_max": "still tied this many minutes into overtime: a draw; 0 = never",
    "match_lock": "lock both teams as the match goes live",
    "match_warmup_weapons": "warmup: every weapon the map places, full ammo",
    "match_timeout": "seconds a timeout lasts (one per side a match); 0 = none",
    "match_timeout_resume": "seconds of countdown back to play",
    "timelimit": "minutes",
    "fraglimit": "0 = none",
    "capturelimit": "0 = none",
    "g_map_list_shuffle": "play g_map_list in a random order; 0 = as written",
    "g_scoreboard": "the mod's scoreboards; 0 = stock's",
    "g_frag_messages": "\"You fragged ...\" and your place, on each frag",
    "g_loc": "location callouts: 2 = .loc file, else the nearest item; 0 = off",
}


def builtins():
    """{gametype: [(cvar, value), ...]} from core_rulesets[], in order."""
    text = SOURCE.read_text()
    table = text[text.index("const RulesetDef core_rulesets[]"):]
    table = table[:table.index("\n};")]
    rulesets = {}
    for m in re.finditer(r'\{"(\w+)", "(\w+)", "[^"]*",\s*((?:"[^"]*"\s*)+)\}', table):
        name, gametype, body = m.group(1), m.group(2), m.group(3)
        if name != gametype:
            continue
        lines = "".join(re.findall(r'"([^"]*)"', body)).replace("\\n", "\n").splitlines()
        rulesets[gametype] = [tuple(line.split(" ", 2)[1:]) for line in lines if line.startswith("set ")]
    return rulesets


def render(gametype, settings):
    out = [
        f"// rulesets/{gametype}.cfg: your changes to {TITLES[gametype]}.",
        "//",
        "// Without this file the server plays the built-in settings below. Each",
        "// line is commented out at its built-in value: uncomment one and change",
        "// it to use your own, and delete the rest if you like. Settings here",
        "// apply whenever this gametype starts (by vote, `sv ruleset`, or",
        "// `set g_ruleset` in server.cfg); nothing carries over from another.",
        "// Every setting: docs/admin.md.",
        "//",
        "// Generated from the module's built-in rulesets",
        "// (scripts/gen_example_rulesets.py); the map list is a suggestion.",
        "",
        "// The rotation, and the maps a vote may pick (none built in).",
        f'set g_map_list "{MAP_LISTS[gametype]}"',
        "",
    ]
    width = max(len(f"// set {c} {v}") for c, v in settings if c not in GAMETYPE_CVARS)
    for cvar, value in settings:
        if cvar in GAMETYPE_CVARS:
            continue
        if cvar not in MEANING:
            sys.exit(f"{cvar} (built-in {gametype}) has no line in MEANING: add one")
        out.append(f"// set {cvar} {value}".ljust(width + 2) + f"// {MEANING[cvar]}")
    return "\n".join(out) + "\n"


def main():
    check = "--check" in sys.argv[1:]
    rulesets = builtins()
    if set(rulesets) != set(TITLES):
        sys.exit(f"built-in rulesets {sorted(rulesets)}, expected {sorted(TITLES)}")
    stale = []
    for gametype, settings in rulesets.items():
        path = OUT / f"{gametype}.cfg"
        text = render(gametype, settings)
        if check:
            if not path.exists() or path.read_text() != text:
                stale.append(str(path.relative_to(ROOT)))
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
    if stale:
        sys.exit("gen_example_rulesets: out of date with core/rulesets.cpp: " + ", ".join(stale)
                 + "\n  run: python3 -I scripts/gen_example_rulesets.py")
    print("gen_example_rulesets: ok" if check else f"wrote {len(rulesets)} files to {OUT.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
