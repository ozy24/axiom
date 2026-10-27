#!/usr/bin/env bash
# Copies harness output into an artifact directory and removes the rigs.
# Text only: console logs, crash logs, the scenario tiers' game tap and sim
# client logs, and the module's own save JSON (game.ssv, <map>.sav).
# Retail data (paks, kpf) and the engine's binary save files never leave the box.
#   scripts/harness/collect-logs.sh <rig-root> <out-dir>
set -euo pipefail
root=$1 out=$2
mkdir -p "$out"
for d in "$root"/*/; do
	[[ -d $d ]] || continue
	n=$(basename "$d")
	mkdir -p "$out/$n"
	cp "$d/console.log" "$out/$n/" 2>/dev/null || true
	for f in CRASHLOG.TXT stderr.txt tap.jsonl sim.jsonl sim.out sim.script; do
		cp "$d/$f" "$out/$n/" 2>/dev/null || true
	done
	# Q2RED: a directory per slot.
	for s in "$d"/baseq2/save/coop_*; do
		[[ -d $s ]] || continue
		mkdir -p "$out/$n/$(basename "$s")"
		cp "$s"/game.ssv "$s"/*.sav "$out/$n/$(basename "$s")/" 2>/dev/null || true
	done
	# Q2REX: a gzipped PAK per slot; extract only the module's JSON.
	for s in "$d"/home/baseq2/save/coop_*.pak.gz; do
		[[ -f $s ]] || continue
		slot=$(basename "$s" .pak.gz)
		python3 -I - "$s" "$out/$n/$slot" <<'PY'
import gzip, os, struct, sys
data = gzip.decompress(open(sys.argv[1], "rb").read())
_, off, length = struct.unpack_from("<4sii", data, 0)
os.makedirs(sys.argv[2], exist_ok=True)
for i in range(length // 64):
    name, o, n = struct.unpack_from("<56sii", data, off + i * 64)
    name = name.split(b"\0")[0].decode()
    if name == "game.ssv" or name.endswith(".sav"):
        open(os.path.join(sys.argv[2], name), "wb").write(data[o:o + n])
PY
	done
done
rm -rf "$root"
