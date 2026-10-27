#!/usr/bin/env bash
# Builds a local playtest server: Q2RED + this mod + ozbot, joinable from retail
# Quake II on the same machine. Modelled on the RA2 rig (~/q2ra2).
#
#   scripts/playtest/setup-local-rig.sh [RIG] [ENGINE] [BOTS]
#     RIG     where to put it       (default ~/q2playtest)
#     ENGINE  dir with quake2ex_dedicated + PlayFabMultiplayerWin_reconstruction.so
#             (default ~/q2ra2, the RA2 rig's Q2RED)
#     BOTS    ozbot-re checkout     (default ~/Code/Projects/ozbot-re; dist/ built)
#
# Then:  RIG/run.sh   start (LAN lobby on UDP 5073; log RIG/server.out)
#        RIG/cmd.sh "sv ruleset hoony"; RIG/cmd.sh "map q2dm1"   (sv ruleset only
#        works once a map is up; server.cfg starts in a mode with `set g_ruleset`)
#        RIG/update.sh  copy in the latest build/linux-release/game_x64.so
#        RIG/stop.sh
# Join from retail: console, connect 127.0.0.1:5073
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
rig=${1:-$HOME/q2playtest}
engine=${2:-$HOME/q2ra2}
bots=${3:-$HOME/Code/Projects/ozbot-re}
retail="$HOME/.local/share/Steam/steamapps/common/Quake 2/rerelease"

mkdir -p "$rig/baseq2/nav" "$rig/userdata"
cp "$engine/quake2ex_dedicated" "$engine/PlayFabMultiplayerWin_reconstruction.so" "$rig/"
ln -sfn "$retail/Q2Game.kpf" "$rig/Q2Game.kpf"
ln -sfn "$retail/baseq2/pak0.pak" "$rig/baseq2/pak0.pak"
cp "$bots/dist/ozbot_x64.so" "$bots/data/botnames.txt" "$rig/baseq2/"
for nav in "$bots"/baselines/nav_q2red_bloodyblastard/*.nav; do
	b=$(basename "$nav"); cp "$nav" "$rig/baseq2/nav/${b,,}"
done
cp "$repo/build/linux-release/game_x64.so" "$rig/baseq2/game_x64.so"

cat >"$rig/baseq2/server.cfg" <<'CFG'
// Playtest server (docs/admin.md). The server's own settings; the gameplay
// comes from the ruleset: change modes with ./cmd.sh "sv ruleset <name>" then
// ./cmd.sh "map <map>". Change a gametype's settings in rulesets/<gametype>.cfg.
set hostname "Playtest"
set deathmatch 1
set maxclients 8
set g_ruleset duel
// one bot to duel against; bot_count 0 for human-only
set sv_bot_brain ozbot
set bot_count 1
set bot_skill 2
map q2dm1
CFG

cat >"$rig/run.sh" <<'RUN'
#!/bin/sh
# start the playtest server in the background (log: server.out), LAN lobby on UDP 5073
cd "$(dirname "$0")"
[ -f server.pid ] && kill -0 "$(cat server.pid)" 2>/dev/null && { echo "already running (pid $(cat server.pid))"; exit 1; }
rm -f stdin; mkfifo stdin
tail -f /dev/null > stdin &   # keep the fifo open so the server never sees EOF
echo $! > keeper.pid
COMPUTERNAME=playtest XDG_DATA_HOME="$PWD/userdata" LD_LIBRARY_PATH="$PWD" nohup ./quake2ex_dedicated -stdout \
	+set dedicated 1 +set logfile 3 +set sv_playfab_signin 0 +set sv_lan_lobby_port 5073 \
	+exec server.cfg < stdin > server.out 2>&1 &
echo $! > server.pid
i=0   # create the LAN lobby exactly once, after the map is up (twice orphans players)
until grep -q 'Gametype:' server.out 2>/dev/null || [ $i -ge 30 ]; do i=$((i+1)); sleep 1; done
sleep 3
echo "kexmultiplayer create lan" > stdin
echo "started (pid $(cat server.pid)); join from retail: connect 127.0.0.1:5073; log: $PWD/server.out"
RUN
cat >"$rig/cmd.sh" <<'CMD'
#!/bin/sh
# one console command to the running server: ./cmd.sh "status"
cd "$(dirname "$0")"
[ -p stdin ] && [ -f server.pid ] && kill -0 "$(cat server.pid)" 2>/dev/null || { echo "not running"; exit 1; }
echo "$*" > stdin
CMD
cat >"$rig/stop.sh" <<'STOP'
#!/bin/sh
# quit cleanly, then stop the fifo keeper (by PID, never by process name)
cd "$(dirname "$0")"
[ -p stdin ] && [ -f server.pid ] && kill -0 "$(cat server.pid)" 2>/dev/null && echo quit > stdin
sleep 3
for f in server.pid keeper.pid; do [ -f $f ] && kill "$(cat $f)" 2>/dev/null; rm -f $f; done
rm -f stdin
echo stopped
STOP
cat >"$rig/update.sh" <<UPD
#!/bin/sh
# copy in the latest local build of the mod (restart or change map to load it).
# Copied beside it and renamed over: writing into the .so a running server has
# loaded crashes it (SIGBUS).
cp "$repo/build/linux-release/game_x64.so" "\$(dirname "\$0")/baseq2/game_x64.so.new" && mv "\$(dirname "\$0")/baseq2/game_x64.so.new" "\$(dirname "\$0")/baseq2/game_x64.so" && strings "\$(dirname "\$0")/baseq2/game_x64.so" | grep mod-identity
UPD
chmod +x "$rig"/*.sh
echo "rig ready: $rig"
