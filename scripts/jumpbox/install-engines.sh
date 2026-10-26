#!/usr/bin/env bash
# Builds the pinned engine and bot versions the game tests run against and
# installs them under /srv/q2ci/engines (made by setup-ci-runner.sh).
#
# Run on the jumpbox as chris (no sudo):
#     bash install-engines.sh
#
# The pins live in scripts/harness/pins.env, so a bump is a reviewed change.
# Each pin installs into its own directory (q2red-<sha>, ozbot-<sha>); old ones
# are left alone, so a pin change never breaks a run that's already going.
# Builds come from `git archive` of the pinned commit, never from the working
# checkout, so local edits in ~/Code/Projects can't leak into CI.
set -euo pipefail

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=../harness/pins.env
source "$here/../harness/pins.env"

ENGINES=/srv/q2ci/engines
BUILD=${XDG_CACHE_HOME:-$HOME/.cache}/q2ci-build
Q2RED_SRC=${Q2RED_SRC:-$HOME/Code/Projects/q2red}
OZBOT_SRC=${OZBOT_SRC:-$HOME/Code/Projects/ozbot-re}

[[ -w $ENGINES ]] || { echo "$ENGINES is not writable; run setup-ci-runner.sh first"; exit 1; }
mkdir -p "$BUILD"

export_commit() { # repo commit dest
	git -C "$1" fetch -q origin
	git -C "$1" cat-file -e "$2^{commit}"
	rm -rf "$3" && mkdir -p "$3"
	git -C "$1" archive "$2" | tar -x -C "$3"
}

q2red_dest=$ENGINES/q2red-$Q2RED_COMMIT
if [[ -x $q2red_dest/quake2ex_dedicated ]]; then
	echo "== q2red $Q2RED_COMMIT already installed"
else
	echo "== q2red $Q2RED_COMMIT"
	src=$BUILD/q2red-$Q2RED_COMMIT
	export_commit "$Q2RED_SRC" "$Q2RED_COMMIT" "$src/src"
	cmake -S "$src/src" -B "$src/build" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null
	cmake --build "$src/build" --target quake2ex_dedicated_link_probe playfab_multiplayer_reconstruction
	server=$(find "$src/build/bin" -name quake2ex_dedicated_link_probe -type f | head -1)
	playfab=$(find "$src/build/bin" -name 'PlayFabMultiplayerWin_reconstruction.so' -type f | head -1)
	[[ -n $server && -n $playfab ]] || { echo "q2red build outputs not found"; exit 1; }
	tmp=$q2red_dest.tmp && rm -rf "$tmp" && mkdir -p "$tmp"
	install -m 0755 "$server" "$tmp/quake2ex_dedicated"
	install -m 0644 "$playfab" "$tmp/PlayFabMultiplayerWin_reconstruction.so"
	echo "$Q2RED_COMMIT" >"$tmp/COMMIT"
	mv "$tmp" "$q2red_dest"
fi

ozbot_dest=$ENGINES/ozbot-$OZBOT_COMMIT
if [[ -f $ozbot_dest/ozbot_x64.so ]]; then
	echo "== ozbot $OZBOT_COMMIT already installed"
else
	echo "== ozbot $OZBOT_COMMIT"
	src=$BUILD/ozbot-$OZBOT_COMMIT
	export_commit "$OZBOT_SRC" "$OZBOT_COMMIT" "$src"
	(cd "$src" && ./build_q2red.sh)
	tmp=$ozbot_dest.tmp && rm -rf "$tmp" && mkdir -p "$tmp/nav"
	install -m 0644 "$src/dist/ozbot_x64.so" "$tmp/ozbot_x64.so"
	install -m 0644 "$src/data/botnames.txt" "$tmp/botnames.txt"
	# The deathmatch navs deployed to Bloody Blastard, lower-cased as the
	# engine expects (ozbot-re q2red/README.md).
	for nav in "$src"/baselines/nav_q2red_bloodyblastard/*.nav; do
		base=$(basename "$nav")
		install -m 0644 "$nav" "$tmp/nav/${base,,}"
	done
	echo "$OZBOT_COMMIT" >"$tmp/COMMIT"
	mv "$tmp" "$ozbot_dest"
fi

q2rex_dest=$ENGINES/q2rex-$Q2REX_VERSION
if [[ -f $q2rex_dest/quake2rex_server.exe ]]; then
	echo "== q2rex $Q2REX_VERSION already installed"
else
	echo "== q2rex $Q2REX_VERSION"
	RETAIL="$HOME/.local/share/Steam/steamapps/common/Quake 2/rerelease"
	tmp=$q2rex_dest.tmp && rm -rf "$tmp" && mkdir -p "$tmp/retail-dlls"
	gh release download "v$Q2REX_VERSION" -R DarkMatter-Productions/Q2REX \
		-p "q2rex-server-$Q2REX_VERSION-win64-wine.zip" -D "$tmp"
	(cd "$tmp" && unzip -q "q2rex-server-$Q2REX_VERSION-win64-wine.zip" && rm "q2rex-server-$Q2REX_VERSION-win64-wine.zip")
	# Retail runtime libraries the Windows server loads from its basedir.
	for f in PartyWin.dll PartyWin7.dll PlayFabMultiplayerWin.dll SDL2.dll steam_api64.dll xaudio2_9redist.dll; do
		install -m 0644 "$RETAIL/$f" "$tmp/retail-dlls/$f"
	done
	echo "$Q2REX_VERSION" >"$tmp/VERSION"
	mv "$tmp" "$q2rex_dest"
fi

chmod -R g+rX "$q2red_dest" "$ozbot_dest" "$q2rex_dest"
echo
echo "Installed:"
ls -la "$q2red_dest" "$ozbot_dest" "$q2rex_dest" | sed 's/^/  /'
