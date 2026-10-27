#!/usr/bin/env bash
# Builds a local game-test kit - the pinned Q2RED (with its sim client), the
# pinned ozbot, links to the retail data and the stock reference module - in a
# persistent folder, so q2harness.py runs on a developer machine the way it
# runs on the jumpbox. The harness uses this kit by default when /srv/q2ci
# isn't there.
#
#     scripts/harness/setup-local-kit.sh        # kit in ~/.cache/q2harness-kit
#     KIT=<dir> scripts/harness/setup-local-kit.sh
#
# Never put the kit in /tmp: it's slow to rebuild and a reboot clears it.
# Everything is keyed by pin, so re-running after a pin bump adds the new
# versions and leaves the old ones. Q2REX isn't built (it runs on the jumpbox).
set -euo pipefail

repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
KIT=${KIT:-${XDG_CACHE_HOME:-$HOME/.cache}/q2harness-kit}
RETAIL=${RETAIL:-$HOME/.local/share/Steam/steamapps/common/Quake 2/rerelease}
# shellcheck source=pins.env
source "$repo/scripts/harness/pins.env"

mkdir -p "$KIT/engines"
ENGINES="$KIT/engines" INSTALL_Q2REX=0 bash "$repo/scripts/jumpbox/install-engines.sh"

echo "== retail data (links into $RETAIL)"
[[ -f $RETAIL/Q2Game.kpf && -f $RETAIL/baseq2/pak0.pak ]] ||
	{ echo "no retail install at $RETAIL (set RETAIL=...)"; exit 1; }
mkdir -p "$KIT/retail/baseq2"
ln -sfn "$RETAIL/Q2Game.kpf" "$KIT/retail/Q2Game.kpf"
ln -sfn "$RETAIL/baseq2/pak0.pak" "$KIT/retail/baseq2/pak0.pak"

stock="$KIT/stock-reference/${STOCK_REFERENCE_COMMIT:0:8}"
if [[ -f $stock/game_x64.so ]]; then
	echo "== stock reference ${STOCK_REFERENCE_COMMIT:0:8} already built"
else
	echo "== stock reference ${STOCK_REFERENCE_COMMIT:0:8}"
	[[ "$(git -C "$repo" rev-parse stock-reference^{commit})" == "$STOCK_REFERENCE_COMMIT" ]] ||
		{ echo "the stock-reference tag doesn't point at $STOCK_REFERENCE_COMMIT"; exit 1; }
	src="$stock.src"
	rm -rf "$src" && mkdir -p "$src"
	git -C "$repo" archive stock-reference | tar -x -C "$src"
	cmake -S "$src" -B "$src/build" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null
	nice cmake --build "$src/build"
	mkdir -p "$stock" && cp "$src/build/game_x64.so" "$stock/"
	rm -rf "$src"
fi

echo
echo "Kit ready in $KIT. The harness finds it on its own:"
echo "  python3 -I scripts/harness/q2harness.py test --candidate build/linux-release/game_x64.so"
