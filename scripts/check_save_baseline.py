#!/usr/bin/env python3
"""Enforce AGENTS.md hard rule 1: never rename or remove a saved name.

Saves store two kinds of name, and with g_strict_saves 1 (the default) an
unknown one is fatal on load:

  * registered pointers: every function or mmove declared through a save
    registration macro in g_local.h (THINK, TOUCH, MMOVE_T, MONSTERINFO_*, ...),
    stored by name (g_save.cpp's save_data_list_t);
  * saved fields: every FIELD*/FIELD_AUTO entry in g_save.cpp's save structs,
    stored by member name.

This scans the source for both and compares the result with the committed
baseline. Names may be added; any name that disappears fails the check, because
a stock save containing it would no longer load.

The baseline is a source scan, so it also lists about 49 names that are
defined but never referenced (mostly unused MMOVEs); compilers drop those, and
saves can never contain them. Keeping them errs on the safe side.

    scripts/check_save_baseline.py           # check
    scripts/check_save_baseline.py --update  # rewrite the baseline (commit it)
"""

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "rerelease"
BASELINE = ROOT / "docs" / "baselines" / "save-names.txt"


def registration_macros():
    """Map each registration macro name to its save tag, read from g_local.h.

    Reading the macros rather than hard-coding them means a new macro form is
    picked up automatically.
    """
    text = (SRC / "g_local.h").read_text(encoding="utf-8", errors="replace")
    text = text.replace("\\\n", " ")  # join continued #define lines
    macros = {}
    for m in re.finditer(r"#define\s+([A-Z_]+)\(n\)\s+SAVE_DATA_FUNC\(n,\s*([A-Z_]+)", text):
        macros[m.group(1)] = "SAVE_FUNC_" + m.group(2)
    for m in re.finditer(r"#define\s+([A-Z_]+)\(n\)[^\n]*save_data_list_t\s+save__##n\(#n,\s*([A-Z_]+)", text):
        macros.setdefault(m.group(1), m.group(2))
    return macros


def registered_names(macros):
    names = set()
    use = re.compile(r"(?<![A-Za-z0-9_#])(" + "|".join(map(re.escape, macros)) + r")\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)")
    for path in sorted(SRC.rglob("*.cpp")):
        for m in use.finditer(path.read_text(encoding="utf-8", errors="replace")):
            names.add(f"ptr {macros[m.group(1)]} {m.group(2)}")
    return names


def saved_fields():
    names = set()
    struct = None
    field = re.compile(r"^\s*FIELD[A-Z_]*\(\s*([A-Za-z_][A-Za-z0-9_.\[\]]*)")
    for line in (SRC / "g_save.cpp").read_text(encoding="utf-8", errors="replace").splitlines():
        m = re.match(r"\s*#define\s+DECLARE_SAVE_STRUCT\s+(\w+)", line)
        if m:
            struct = m.group(1)
            continue
        if line.lstrip().startswith("#") or line.rstrip().endswith("\\"):
            continue  # macro definitions, not save entries
        m = field.match(line)
        if m and struct:
            names.add(f"field {struct} {m.group(1)}")
    return names


def main():
    macros = registration_macros()
    if len(macros) < 20:
        sys.exit(f"check_save_baseline: found only {len(macros)} registration "
                 "macros in g_local.h; the parser no longer matches the source")
    current = registered_names(macros) | saved_fields()

    if "--update" in sys.argv:
        BASELINE.parent.mkdir(parents=True, exist_ok=True)
        BASELINE.write_text("\n".join(sorted(current)) + "\n", encoding="utf-8")
        print(f"check_save_baseline: wrote {len(current)} names "
              f"({len(macros)} macro forms) to {BASELINE.relative_to(ROOT)}")
        return

    if not BASELINE.exists():
        sys.exit(f"check_save_baseline: {BASELINE.relative_to(ROOT)} missing; run with --update")
    baseline = set(BASELINE.read_text(encoding="utf-8").split("\n")) - {""}
    missing = sorted(baseline - current)
    added = sorted(current - baseline)
    if missing:
        print("check_save_baseline: FAIL - saved names were renamed or removed.")
        print("Stock saves containing these would no longer load (g_strict_saves):")
        for name in missing:
            print(f"  - {name}")
        sys.exit(1)
    if added:
        print(f"check_save_baseline: ok, {len(added)} new name(s) not yet in the "
              "baseline; run with --update and commit it:")
        for name in added:
            print(f"  + {name}")
    else:
        print(f"check_save_baseline: ok ({len(current)} names)")


if __name__ == "__main__":
    main()
