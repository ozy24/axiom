#!/usr/bin/env bash
# Rename drill (AGENTS.md: renamable). Proves the name lives only in mod.json:
# copy the repo, rename the mod in mod.json to a dummy, build, and require the
# built module to carry the dummy and not the real name or id (any case).
#
#   scripts/rename_drill.sh            # Linux build, from the repo root
#
# Builds in a neutrally named temp directory as a Release build, so neither the
# checkout's folder name nor debug-info paths can put the name in the binary.
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

read -r name id < <(python3 -I -c 'import json; m = json.load(open("mod.json")); print(m["name"], m["id"])')
dummy_name=Zzdrill dummy_id=zzdrill

work=$(mktemp -d /tmp/drill-XXXXXX)
trap '[[ -n ${KEEP:-} ]] || rm -rf "$work"' EXIT
git ls-files -z --cached --others --exclude-standard | tar --null -T - -cf - | tar -xf - -C "$work"
python3 -I - "$work/mod.json" "$dummy_name" "$dummy_id" <<'PY'
import json, sys
path, name, ident = sys.argv[1:]
mod = json.load(open(path))
mod["name"], mod["id"] = name, ident
json.dump(mod, open(path, "w"), indent=2)
PY

cmake -S "$work" -B "$work/build" -G Ninja -DCMAKE_BUILD_TYPE=Release ${CMAKE_CXX_COMPILER:+-DCMAKE_CXX_COMPILER="$CMAKE_CXX_COMPILER"} >/dev/null
cmake --build "$work/build" --target game_x64 >/dev/null

so="$work/build/game_x64.so"
strings -n 4 "$so" >"$work/strings.txt"
fail=0
# core/identity.cpp's stamp is the one guaranteed textual copy of the identity
# (short literals elsewhere may be compiled to immediate stores).
if ! grep -q "mod-identity: name=$dummy_name id=$dummy_id " "$work/strings.txt"; then
	echo "rename_drill: FAIL - the built module's identity stamp is not the renamed identity"
	grep "mod-identity" "$work/strings.txt" | sed 's/^/  stamp: /'
	fail=1
fi
if grep -iE "$name|$id" "$work/strings.txt" | head -5 | sed 's/^/  leaked: /' | grep .; then
	echo "rename_drill: FAIL - the built module still contains '$name' / '$id'"; fail=1
fi
((fail == 0)) && echo "rename_drill: ok (built as $dummy_name/$dummy_id; no trace of $name/$id)"
exit $fail
