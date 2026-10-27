#!/usr/bin/env python3
"""Game-test harness: runs game modules headless and compares them with stock.

Two engines, same tiers:
  q2red   the native Linux server; loads game_x64.so      (--engine-kind q2red)
  q2rex   the Windows server under Wine; loads the MSVC
          game_x64.dll, the binary players actually run   (--engine-kind q2rex)

Tiers (plan Phase 1b):
  1. save-diff   Load coop base1, save, and compare the module's save JSON
                 (game.ssv, base1.sav) between the stock reference and the
                 candidate. Structure must match exactly; values must match
                 except fields listed as timing-volatile by calibration.
  2. load-stock  A save made by the stock reference must load in the candidate
                 (docs/decisions/0007), proven by the engine's SV_ReadLevelFile.
  3. dm-smoke    A deathmatch match with bots: no crash, frags happen, clean
                 shutdown, no new error lines.
  4. gametype-boot  Every registered gametype, and Rogue's stock gamerules
                 modes, boot q2dm1 with bots, log their "Gametype:" line and
                 shut down cleanly.
  5. match-flow  match_warmup on: warmup -> countdown -> live -> intermission
                 on the timelimit, counted from LIVE.
  6. duel        Three bots: two duel, one queues; the match ends, the loser
                 rotates to the back and a match record is written.
  7. rounds      match_rounds on: a frag ends the round, the next one starts
                 (Q2RED only: needs bots that fight).
  8. teams       TDM and CTF with the match flow: four bots on two teams, a
                 timelimit end, and a record with both teams.
  9. warmup      Bots fight in an endless warmup; every score stays 0
                 (Q2RED only).
 10. duel-rotation  After a level change the Duel loser waits and the queued
                 bot plays (Q2RED only).
 11. human-duel  Simulated players through the human path: the playtest's
                 match-flow and Duel rows, checked from the game tap (Q2RED).
 12. human-teams TDM and CTF: simulated players pick teams; no start with one
                 team; the record has everyone on their team (Q2RED).
 13. human-rounds  HoonyMode: each scripted suicide ends a round, the next
                 starts after "Next round in 3..1" (Q2RED).

    q2harness.py test      --stock S --candidate C   # tiers 1-3
    q2harness.py calibrate --stock S --runs 6        # refresh the volatile list

Engines, retail data and bots default to the pinned installs under /srv/q2ci
(scripts/harness/pins.env). Each run gets a fresh rig directory, so runs never
share state.
"""

import argparse
import collections
import concurrent.futures
import copy
import gzip
import json
import os
import pathlib
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
BASELINES = ROOT / "docs" / "baselines"
ALLOWED = BASELINES / "allowed-differences.txt"
SAVE_FILES = ("game.ssv", "base1.sav")
COOP = ["+set", "coop", "1", "+set", "deathmatch", "0", "+set", "maxclients", "4", "+map", "base1"]
ERROR_RE = re.compile(r"(?i)\b(error|fatal|assert|segmentation|unknown (field|pointer))\b")


def pins():
    out = {}
    for line in (ROOT / "scripts" / "harness" / "pins.env").read_text().splitlines():
        m = re.match(r"\s*([A-Z0-9_]+)=(\S+)", line)
        if m:
            out[m.group(1)] = m.group(2)
    return out


def volatile_file(kind):
    return BASELINES / f"volatile-save-fields.{kind}.txt"


class Rig:
    """A throwaway server directory holding one engine, retail data and one
    game module. Subclasses know each engine's layout and command line."""

    kind = None
    module_name = None
    expects_frags = True  # can a bot-only match on this engine produce frags?
    benign_re = None      # known-harmless lines that look like errors
    ready_re = None  # printed once the map has loaded and is running
    level_read_re = re.compile(r"SV_ReadLevelFile\(")

    def __init__(self, args, module, name):
        self.args = args
        self.dir = pathlib.Path(tempfile.mkdtemp(prefix=f"{name}-", dir=args.workdir))
        self.log_path = self.dir / "console.log"
        self.exited_early = False
        self.returncode = None
        self.setup(pathlib.Path(module))

    def run(self, startup, steps, timeout):
        """Start the server, then work through `steps` in order:
             ("wait", regex, seconds)  wait for the console log to match
             ("exists", path, seconds) wait for a file to appear
             ("sleep", seconds)
             ("cmd", text)             send a console command over stdin
             ("sim", script)           start Q2RED's simulated players on a
                                       script (q2red tools/sim-client) and go on
             ("simwait",)              wait for them to finish
        A server that exits early (a fatal error, say) ends the steps.
        Returns the console log text."""
        cmd, env, cwd = self.command(startup)
        log = open(self.log_path, "w")
        proc = subprocess.Popen(cmd, cwd=cwd, env=env, stdin=subprocess.PIPE,
                                stdout=log, stderr=subprocess.STDOUT, text=True)
        deadline = time.monotonic() + timeout
        self.sim = None
        try:
            for step in steps:
                if not self._step(proc, step, deadline):
                    break
            try:
                proc.stdin.close()
            except BrokenPipeError:
                pass
            proc.wait(timeout=max(5, deadline - time.monotonic()))
        except subprocess.TimeoutExpired:
            raise RuntimeError(f"server did not finish within {timeout}s ({self.dir})")
        finally:
            # Whatever went wrong, never leave a server or a sim running.
            if self.sim and self.sim.poll() is None:
                self.sim.kill()
                self.sim.wait()
            if proc.poll() is None:
                proc.kill()
                proc.wait()
            log.close()
        self.returncode = proc.returncode
        return self.log_path.read_text(errors="replace")

    def _step(self, proc, step, deadline):
        if proc.poll() is not None:
            self.exited_early = True
            return False
        if step[0] == "sleep":
            time.sleep(step[1])
        elif step[0] == "wait":
            regex, limit = re.compile(step[1]), time.monotonic() + step[2]
            while not regex.search(self.log_path.read_text(errors="replace")):
                if proc.poll() is not None:
                    self.exited_early = True
                    return False
                if time.monotonic() > min(limit, deadline):
                    raise RuntimeError(f"timed out waiting for /{step[1]}/ ({self.dir})")
                time.sleep(0.5)
        elif step[0] == "exists":
            limit = time.monotonic() + step[2]
            while not step[1].exists():
                if proc.poll() is not None:
                    self.exited_early = True
                    return False
                if time.monotonic() > min(limit, deadline):
                    raise RuntimeError(f"timed out waiting for {step[1]} ({self.dir})")
                time.sleep(0.5)
            time.sleep(1)  # let the writer finish
        elif step[0] == "sim":
            (self.dir / "sim.script").write_text(step[1])
            self.sim = subprocess.Popen(
                ["./q2red_sim_client", "127.0.0.1", str(self.args.lobby_port), "sim.script", "sim.jsonl"],
                cwd=self.dir, stdout=open(self.dir / "sim.out", "w"), stderr=subprocess.STDOUT)
        elif step[0] == "simwait":
            try:
                self.sim.wait(timeout=max(1, deadline - time.monotonic()))
            except subprocess.TimeoutExpired:
                raise RuntimeError(f"sim clients did not finish ({self.dir})")
        elif step[0] == "cmd":
            try:
                proc.stdin.write(step[1] + "\n")
                proc.stdin.flush()
            except BrokenPipeError:
                self.exited_early = True
                return False
        return True

    def wait_ready(self):
        return ("wait", self.ready_re, self.args.boot_timeout)


class Q2RedRig(Rig):
    kind = "q2red"
    module_name = "game_x64.so"
    ready_re = r"Nav Manager: Processing Nav Edicts"
    # Q2RED builds map paths in a MAX_OSPATH buffer from its own directory:
    # 1024 bytes since A61 (128 before: "Com_sprintf: overflow ... in 128",
    # then "Can't find maps/x.bsp"). Leave room for the rest of the path.
    max_path = 900

    def setup(self, module):
        if len(str(self.dir)) > self.max_path:
            raise RuntimeError(f"rig path is {len(str(self.dir))} chars, over {self.max_path}; "
                               "use a shorter --workdir (default /tmp/q2h-*)")
        a, base = self.args, self.dir / "baseq2"
        (self.dir / "userdata").mkdir()
        base.mkdir()
        engine, retail = pathlib.Path(a.engine), pathlib.Path(a.retail)
        # Copied, not symlinked: the engine resolves its own path and writes
        # CRASHLOG.TXT (and finds Q2Game.kpf) next to the real binary, so a
        # symlink would make test crashes overwrite files in the engine dir.
        for f in ("quake2ex_dedicated", "PlayFabMultiplayerWin_reconstruction.so"):
            shutil.copy2(engine / f, self.dir / f)
        # Q2RED's simulated players (tools/sim-client), for the scenario tiers.
        if (engine / "q2red_sim_client").exists():
            shutil.copy2(engine / "q2red_sim_client", self.dir / "q2red_sim_client")
        (self.dir / "Q2Game.kpf").symlink_to(retail / "Q2Game.kpf")
        (base / "pak0.pak").symlink_to(retail / "baseq2" / "pak0.pak")
        shutil.copy2(module, base / self.module_name)
        if a.bots:
            bots = pathlib.Path(a.bots)
            shutil.copy2(bots / "ozbot_x64.so", base / "ozbot_x64.so")
            shutil.copy2(bots / "botnames.txt", base / "botnames.txt")
            shutil.copytree(bots / "nav", base / "nav")

    def command(self, startup):
        a = self.args
        cmd = ["./quake2ex_dedicated", "-stdout", "+set", "dedicated", "1",
               "+set", "sv_playfab_signin", "0",
               "+set", "sv_lan_lobby_port", str(a.lobby_port),
               "+set", "port", str(a.port)] + startup
        env = dict(os.environ, COMPUTERNAME="q2harness", XDG_DATA_HOME=str(self.dir / "userdata"),
                   LD_LIBRARY_PATH=str(self.dir))
        return cmd, env, self.dir

    def bot_args(self):
        if not self.args.bots:
            raise RuntimeError("no --bots directory (ozbot build) given")
        return ["+set", "sv_bot_brain", "ozbot", "+set", "bot_count", "6", "+set", "bot_skill", "3"]

    # Q2RED keeps each save slot as a directory of loose files.
    def save_path(self, slot):
        return self.dir / "baseq2" / "save" / slot

    def saved_marker(self, slot):
        return self.save_path(slot) / "base1.sav"

    def read_save(self, slot):
        d = self.save_path(slot)
        if not d.is_dir():
            return None
        return {f: (d / f).read_text() for f in SAVE_FILES if (d / f).exists()}

    def copy_save_from(self, other, slot):
        shutil.copytree(other.save_path(slot), self.save_path(slot))


class Q2RexRig(Rig):
    kind = "q2rex"
    module_name = "game_x64.dll"
    # Q2REX 0.15.1's built-in bots connect in a bot-only match but never enter
    # it (retail admits bots only once a human has spawned; Q2REX's resume path
    # after a map change didn't admit them either, checked 2026-10-26). So on
    # Q2REX tier 3 is a boot test: the module runs deathmatch with bots
    # connected and shuts down cleanly. Frags are checked on Q2RED.
    expects_frags = False
    # The offline namespace makes the startup sign-in fail; that's intended.
    benign_re = re.compile(r"Playfab::signin -- Login failed")
    ready_re = r"Map: base1 \(checksum|Map: \w+ \(checksum"
    # Retail runtime libraries the Windows server loads from basedir.
    RETAIL_DLLS = ("PartyWin.dll", "PartyWin7.dll", "PlayFabMultiplayerWin.dll", "SDL2.dll",
                   "steam_api64.dll", "xaudio2_9redist.dll")

    def setup(self, module):
        a = self.args
        # Wine creates the prefix itself, but only if its parent exists.
        pathlib.Path(a.wineprefix).parent.mkdir(parents=True, exist_ok=True)
        engine, retail = pathlib.Path(a.rex_engine), pathlib.Path(a.retail)
        self.server, self.base, self.home = self.dir / "server", self.dir / "base", self.dir / "home"
        for d in (self.server, self.base / "baseq2", self.home):
            d.mkdir(parents=True)
        for f in ("quake2rex_server.exe", "q2rex_server_lan.cfg", "steam_appid.txt"):
            shutil.copy2(engine / f, self.server / f)
        (self.base / "quake2rex.kpf").symlink_to(engine / "quake2rex.kpf")
        dll_dir = engine / "retail-dlls"
        for f in self.RETAIL_DLLS:
            (self.base / f).symlink_to(dll_dir / f)
        (self.base / "Q2Game.kpf").symlink_to(retail / "Q2Game.kpf")
        (self.base / "baseq2" / "pak0.pak").symlink_to(retail / "baseq2" / "pak0.pak")
        shutil.copy2(module, self.base / "baseq2" / self.module_name)

    @staticmethod
    def winpath(p):
        return "Z:" + str(p).replace("/", "\\")

    def command(self, startup):
        a = self.args
        # Offline: Q2REX signs in to PlayFab at startup even on the LAN route,
        # and CI must not talk to outside services. A rootless network
        # namespace leaves only loopback; the sign-in fails and LAN carries on.
        cmd = (["unshare", "-rn"] if a.offline else []) + [
               a.wine, "quake2rex_server.exe", "-servercfg", "lan",
               "-userpath", self.winpath(self.home),
               "+set", "basedir", self.winpath(self.base), "+set", "game", "baseq2",
               "+set", "dedicated", "1", "+set", "ip_hostport", str(a.port)] + startup
        env = dict(os.environ, WINEPREFIX=a.wineprefix, WINEARCH="win64", WINEDEBUG="-all",
                   WINEDLLOVERRIDES="winemenubuilder.exe=d", COMPUTERNAME="q2harness",
                   USERNAME="q2harness")
        return cmd, env, self.server

    def bot_args(self):
        return ["+set", "bot_minPlayers", "6"]

    # Q2REX keeps each save slot as a gzipped Quake PAK.
    def save_path(self, slot):
        return self.home / "baseq2" / "save" / f"{slot}.pak.gz"

    def saved_marker(self, slot):
        return self.save_path(slot)

    def read_save(self, slot):
        p = self.save_path(slot)
        if not p.exists():
            return None
        data = gzip.decompress(p.read_bytes())
        magic, off, length = struct.unpack_from("<4sii", data, 0)
        if magic != b"PACK":
            raise RuntimeError(f"{p} is not a PAK")
        out = {}
        for i in range(length // 64):
            name, o, n = struct.unpack_from("<56sii", data, off + i * 64)
            name = name.split(b"\0")[0].decode()
            if name in SAVE_FILES:
                out[name] = data[o:o + n].decode()
        return out

    def copy_save_from(self, other, slot):
        self.save_path(slot).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(other.save_path(slot), self.save_path(slot))


RIGS = {"q2red": Q2RedRig, "q2rex": Q2RexRig}


def coop_save(args, module, name):
    rig = RIGS[args.engine_kind](args, module, name)
    log = rig.run(COOP, [rig.wait_ready(), ("sleep", args.settle), ("cmd", "save coop_harness"),
                         ("exists", rig.saved_marker("coop_harness"), 30), ("cmd", "quit")],
                  timeout=args.boot_timeout + 90)
    files = rig.read_save("coop_harness") or {}
    if not all(f in files for f in SAVE_FILES):
        raise RuntimeError(f"{name}: save files missing ({rig.save_path('coop_harness')})\n{tail(log)}")
    return rig, {f: json.loads(files[f]) for f in SAVE_FILES}


def flatten(obj, path=""):
    if isinstance(obj, dict):
        for k, v in obj.items():
            yield from flatten(v, f"{path}.{k}" if path else k)
    elif isinstance(obj, list):
        for i, v in enumerate(obj):
            yield from flatten(v, f"{path}[{i}]")
    else:
        yield path, obj


def field_of(path, classnames=None):
    """The field a value lives under, by entity classname rather than index:
    entities.12.monsterinfo.nextframe -> entities[monster_soldier].monsterinfo.nextframe.
    Keying by classname keeps volatility narrow: a soldier's position may vary
    between runs, a pickup's may not."""
    p = re.sub(r"\[\d+\]", "", path)
    m = re.match(r"^entities\.(\d+)\.", p)
    if m:
        cls = (classnames or {}).get(m.group(1), "*")
        p = f"entities[{cls}]." + p[m.end():]
    p = re.sub(r"^clients\.\d+\.", "clients.*.", p)
    return p


def classnames_of(save):
    ents = save.get("entities", {}) if isinstance(save, dict) else {}
    return {k: v.get("classname", "?") for k, v in ents.items() if isinstance(v, dict)}


def read_list(path):
    if not path.exists():
        return set()
    return {l.split("#")[0].strip() for l in path.read_text().splitlines()} - {""}


def compare(stock, cand, volatile, allowed):
    problems = []
    for f in SAVE_FILES:
        a, b = dict(flatten(stock[f])), dict(flatten(cand[f]))
        cls = {**classnames_of(cand[f]), **classnames_of(stock[f])}
        for k in sorted(a.keys() ^ b.keys()):
            fld = f"{f}:{field_of(k, cls)}"
            # The save writer omits fields at their default value, so a
            # volatile field (an animation frame that happens to be 0) can
            # come and go between identical runs.
            if fld not in allowed and fld not in volatile:
                where = "stock only" if k in a else "candidate only"
                problems.append(f"{f}: {k} ({where})")
        for k in sorted(a.keys() & b.keys()):
            fld = f"{f}:{field_of(k, cls)}"
            if a[k] != b[k] and fld not in volatile and fld not in allowed:
                problems.append(f"{f}: {k}: stock={a[k]!r} candidate={b[k]!r}")
    return problems


def tail(text, n=25):
    return "\n".join(text.splitlines()[-n:])


def error_lines(log, rig=None):
    benign = rig.benign_re if rig else None
    return [l for l in log.splitlines() if ERROR_RE.search(l) and not (benign and benign.search(l))]


def tier_save_diff(args, results):
    volatile, allowed = read_list(volatile_file(args.engine_kind)), read_list(ALLOWED)
    if not volatile:
        raise RuntimeError(f"no calibration for {args.engine_kind}; run `calibrate` first")
    _, stock = coop_save(args, args.stock, "t1-stock")
    _, cand = coop_save(args, args.candidate, "t1-candidate")
    problems = compare(stock, cand, volatile, allowed)
    results.append(("1 save-diff", not problems, problems[:40]))


def tier_load_stock(args, results):
    stock_rig, _ = coop_save(args, args.stock, "t2-stock")
    rig = RIGS[args.engine_kind](args, args.candidate, "t2-candidate")
    rig.copy_save_from(stock_rig, "coop_harness")
    # developer 1 makes the engine print SV_ReadLevelFile, the proof that the
    # level really came from the save rather than a fresh spawn. Coop slots
    # must be named coop_*: Q2RED reads a coop load back from coop_current.
    startup = COOP[:-2] + ["+set", "developer", "1"] + COOP[-2:]
    log = rig.run(startup, [rig.wait_ready(), ("sleep", 2), ("cmd", "load coop_harness"),
                            ("wait", r"SV_ReadLevelFile\(", 60), ("sleep", args.settle / 2),
                            ("cmd", "save coop_reloaded"), ("exists", rig.saved_marker("coop_reloaded"), 30),
                            ("cmd", "quit")],
                  timeout=args.boot_timeout * 2 + 120)
    after_load = log.split("Loading game...", 1)[-1] if "Loading game..." in log else ""
    detail = error_lines(log, rig)[:20]
    if rig.exited_early:
        detail.append(f"the server exited during the load (code {rig.returncode})")
    if not rig.level_read_re.search(after_load):
        detail.append("the level was not read from the save (no SV_ReadLevelFile after the load)")
    if "base1.sav" not in (rig.read_save("coop_reloaded") or {}):
        detail.append("the reloaded game could not be saved again")
    results.append(("2 load-stock", not detail, detail))


def tier_dm_smoke(args, results):
    rig = RIGS[args.engine_kind](args, args.candidate, "t3-dm")
    startup = (["+set", "deathmatch", "1", "+set", "maxclients", "8", "+set", "timelimit", "0",
                "+set", "fraglimit", "0"] + rig.bot_args() + ["+map", "q2dm1"])
    log = rig.run(startup, [rig.wait_ready(), ("sleep", args.dm_seconds), ("cmd", "status"),
                            ("sleep", 2), ("cmd", "quit")],
                  timeout=args.boot_timeout + args.dm_seconds + 60)
    frags = [l for l in log.splitlines() if re.search(r"\b(killed|was |by |fragged)\b", l)
             and "Nav" not in l]
    detail = []
    if rig.expects_frags and not frags:
        detail.append("no frags in the console log")
    if not re.search(r" connected\.", log):
        detail.append("no bots connected")
    if rig.returncode != 0:
        detail.append(f"server exited with code {rig.returncode}")
    if "==== ShutdownGame ====" not in log:
        detail.append("no clean ShutdownGame")
    detail += error_lines(log, rig)[:20]
    results.append(("3 dm-smoke", not detail, detail + [f"({len(frags)} obituary lines)"]))


def tier_gametype_boot(args, results):
    """Every registered gametype, and Rogue's stock gamerules modes (still run
    as stock, decision 0010), boot q2dm1 with bots and shut down cleanly."""
    cases = [(gt, ["+set", "g_gametype", gt], rf"Gametype: .+ \({re.escape(gt)}\)")
             for gt in args.gametypes.split(",") if gt]
    cases += [(f"gamerules{n}", ["+set", "gamerules", n], rf"Gametype: stock gamerules {re.escape(n)}\b")
              for n in args.stock_gamerules.split(",") if n]
    for label, select, expect in cases:
        rig = RIGS[args.engine_kind](args, args.candidate, f"t4-{label}")
        startup = (["+set", "deathmatch", "1", "+set", "maxclients", "8"] + select
                   + rig.bot_args() + ["+map", GAMETYPE_MAP.get(label, "q2dm1")])
        log = rig.run(startup, [rig.wait_ready(), ("sleep", args.boot_seconds), ("cmd", "q2rex_lobby_mode"),
                                ("cmd", "q2rex_lobby_retail_mode"), ("sleep", 1), ("cmd", "quit")],
                      timeout=args.boot_timeout + args.boot_seconds + 60)
        detail = []
        if not re.search(expect, log):
            detail.append(f"no line matching /{expect}/")
        if not label.startswith("gamerules"):
            # Q2REX's lobby contract: the module publishes its mode.
            if not re.search(rf'"q2rex_lobby_mode" is "{re.escape(label)}"', log):
                detail.append(f"q2rex_lobby_mode is not {label}")
            retail = RETAIL_MODE.get(label, "deathmatch")
            if not re.search(rf'"q2rex_lobby_retail_mode" is "{retail}"', log):
                detail.append(f"q2rex_lobby_retail_mode is not {retail}")
        if rig.returncode != 0:
            detail.append(f"server exited with code {rig.returncode}")
        if "==== ShutdownGame ====" not in log:
            detail.append("no clean ShutdownGame")
        detail += error_lines(log, rig)[:20]
        results.append((f"4 gametype-boot {label}", not detail, detail))


# The stock mode each team gametype runs on, and the map it's tested on.
RETAIL_MODE = {"tdm": "teamplay", "ctf": "ctf"}
GAMETYPE_MAP = {"ctf": "q2ctf1"}

MATCH_SEQUENCE = ["Match: warmup", "Match: warmup -> countdown", "Match: countdown -> live",
                  "Match: live -> intermission", "Match: ended (timelimit)"]


def tier_match_flow(args, results):
    """Warmup on: bots (always ready) run warmup -> countdown -> live, and the
    timelimit, counted from LIVE, ends it (docs/design/match-flow.md)."""
    rig = RIGS[args.engine_kind](args, args.candidate, "t5-match")
    startup = (["+set", "deathmatch", "1", "+set", "maxclients", "8", "+set", "match_warmup", "1",
                "+set", "match_countdown", "3", "+set", "timelimit", "0.25"] + rig.bot_args() + ["+map", "q2dm1"])
    # On both engines the bots count as playing clients (on Q2REX they begin
    # but don't fight), so the whole match runs.
    steps = [rig.wait_ready(), ("wait", r"Match: ended", 90), ("sleep", 2), ("cmd", "quit")]
    log = rig.run(startup, steps, timeout=args.boot_timeout + 150)
    lines = [l.split("Match: ", 1)[1].strip() for l in log.splitlines() if "Match: " in l]
    seen = ["Match: " + l for l in lines]
    detail = []
    want = MATCH_SEQUENCE
    if seen != want:
        detail.append(f"transitions {seen}, expected {want}")
    if rig.returncode != 0:
        detail.append(f"server exited with code {rig.returncode}")
    if "==== ShutdownGame ====" not in log:
        detail.append("no clean ShutdownGame")
    detail += error_lines(log, rig)[:20]
    results.append(("5 match-flow", not detail, detail))


def tier_duel(args, results):
    """Duel with three bots: two duel, the third queues as a spectator; the match
    runs to its timelimit and the loser rotates to the back of the queue."""
    rig = RIGS[args.engine_kind](args, args.candidate, "t6-duel")
    bots = (["+set", "sv_bot_brain", "ozbot", "+set", "bot_count", "3", "+set", "bot_skill", "3"]
            if rig.kind == "q2red" else ["+set", "bot_minPlayers", "3"])
    startup = (["+set", "deathmatch", "1", "+set", "maxclients", "8", "+set", "g_gametype", "duel",
                "+set", "match_warmup", "1", "+set", "match_countdown", "3", "+set", "timelimit", "0.25",
                "+set", "g_matchlog_dir", "matches"] + bots + ["+map", "q2dm1"])
    log = rig.run(startup, [rig.wait_ready(), ("wait", r"Duel: .* next: ", 90), ("sleep", 2), ("cmd", "quit")],
                  timeout=args.boot_timeout + 150)
    detail = []
    # A client can be queued at ClientBegin or by the next frame's reconcile
    # (on Q2REX the bots begin before each other count as present), so count
    # distinct names, once each.
    waiting = set(re.findall(r"(\S.*?) waits in the duel queue", log))
    if rig.expects_frags and len(waiting) != 1:
        detail.append(f"{len(waiting)} clients queued ({sorted(waiting)}), expected 1 (three bots, two duel)")
    flaps = len(re.findall(r"waits in the duel queue", log))
    if flaps > 3:
        detail.append(f"{flaps} queue messages: a client is being moved in and out every frame")
    if not rig.expects_frags:
        # Q2REX: in a bot-only match only two of the three bots ever begin as
        # clients (seen in CI: the third connects and never begins), so there is
        # no third client to queue. Flow and rotation are still checked.
        detail_note = "queueing not checked: only two bots begin on this engine"
    seen = ["Match: " + l.split("Match: ", 1)[1].strip() for l in log.splitlines() if "Match: " in l]
    if seen != MATCH_SEQUENCE:
        detail.append(f"transitions {seen}, expected {MATCH_SEQUENCE}")
    if not re.search(r"Duel: .+ \d+ - \d+ .+; next: ", log):
        detail.append("no Duel result line")
    # The match record (g_matchlog_dir "matches"). Q2RED gives the module file
    # access (A62), so it lands in the game directory; elsewhere it's relative
    # to the server's working directory (decision 0011).
    records = sorted(rig.dir.rglob("matches/*.json"))
    expected_dir = rig.dir / "baseq2" / "matches" if rig.kind == "q2red" else rig.server / "matches"
    if len(records) != 1:
        detail.append(f"{len(records)} match records, expected 1")
    elif records[0].parent != expected_dir:
        detail.append(f"match record in {records[0].parent.relative_to(rig.dir)}, "
                      f"expected {expected_dir.relative_to(rig.dir)}")
    else:
        try:
            rec = json.loads(records[0].read_text())
            if rec.get("gametype") != "duel" or rec.get("reason") != "timelimit":
                detail.append(f"record gametype/reason {rec.get('gametype')}/{rec.get('reason')}")
            if len(rec.get("players", [])) < 2 or rec.get("map") != "q2dm1":
                detail.append(f"record players/map {len(rec.get('players', []))}/{rec.get('map')}")
            waiting_in_record = [p["name"] for p in rec.get("players", []) if not p.get("playing")]
            if rig.expects_frags and len(waiting_in_record) != 1:
                detail.append(f"record lists {len(waiting_in_record)} non-playing clients, expected 1")
        except ValueError as e:
            detail.append(f"match record is not JSON: {e}")
    if rig.returncode != 0:
        detail.append(f"server exited with code {rig.returncode}")
    detail += error_lines(log, rig)[:20]
    results.append(("6 duel", not detail, detail + ([detail_note] if not rig.expects_frags else [])))


def tier_duel_rotation(args, results):
    """The rotation survives the level change: three bots duel to the
    timelimit, the level changes (`gamemap`, as intermission exit does), and
    on the new level the loser waits while the bot that was queued steps in.
    At a level change every client re-enters one by one, which used to rebuild
    the queue in join order. Q2RED only: three bots must begin."""
    rig = RIGS[args.engine_kind](args, args.candidate, "t10-rotation")
    if not rig.expects_frags:
        results.append(("10 duel-rotation", True, ["skipped: only two bots begin on this engine"]))
        return
    startup = (["+set", "deathmatch", "1", "+set", "maxclients", "8", "+set", "g_gametype", "duel",
                "+set", "match_warmup", "1", "+set", "match_countdown", "3", "+set", "timelimit", "0.25",
                "+set", "sv_bot_brain", "ozbot", "+set", "bot_count", "3", "+set", "bot_skill", "3",
                "+map", "q2dm1"])
    log = rig.run(startup, [rig.wait_ready(), ("wait", r"Duel: .* next: ", 90), ("sleep", 2),
                            ("cmd", "gamemap q2dm1"), ("sleep", 20), ("cmd", "quit")],
                  timeout=args.boot_timeout + 180)
    detail = []
    m = re.search(r"Duel: .+ \d+ - \d+ .+; next: (.+)", log)
    if not m:
        results.append(("10 duel-rotation", False, ["no Duel result line"] + error_lines(log, rig)[:20]))
        return
    loser = m.group(1).strip()
    before, after = log[:m.start()], log[m.end():]
    queued = r"^(?:<\d+>)?(\S.*?) waits in the duel queue"  # without the console's <level> tag
    queued_before = set(re.findall(queued, before, re.M))
    queued_after = set(re.findall(queued, after, re.M))
    if len(queued_before) != 1:
        detail.append(f"{sorted(queued_before)} queued in the first match, expected one bot")
    if queued_after != {loser}:
        detail.append(f"{sorted(queued_after)} waiting after the level change, expected the loser {loser!r}")
    if rig.returncode != 0:
        detail.append(f"server exited with code {rig.returncode}")
    detail += error_lines(log, rig)[:20]
    results.append(("10 duel-rotation", not detail, detail))


# --- Scenario tiers: simulated players (docs/design/simulated-clients.md) -------

TOKEN_RE = re.compile(r"##P\d+")


def read_jsonl(path):
    if not path.exists():
        return []
    out = []
    for line in path.read_text(errors="replace").splitlines():
        try:
            out.append(json.loads(line))
        except ValueError:
            pass
    return out


def scenario_rig(args, name):
    """A Q2RED rig that can host simulated players, or None (and why)."""
    if args.engine_kind != "q2red":
        return None, "Q2RED only (simulated players are a Q2RED tool)"
    if not (pathlib.Path(args.engine) / "q2red_sim_client").exists():
        return None, f"no q2red_sim_client in {args.engine}"
    if not args.bots:
        return None, "no --bots (ozbot plays the simulated players)"
    return RIGS["q2red"](args, args.candidate, name), None


def scenario_startup(rig, extra):
    """The server's settings go in a cfg it execs: a scenario's `+set`s would
    pass the engine's command-line argument limit (MAX_NUM_ARGVS)."""
    sets = ["deathmatch", "1", "maxclients", "8", "sv_sim_puppets", "1",
            "sv_game_tap", str(rig.dir / "tap.jsonl"), "sv_bot_brain", "ozbot", "bot_count", "0"]
    rest = list(extra)
    while len(rest) >= 3 and rest[0] == "+set":
        sets += rest[1:3]
        rest = rest[3:]
    lines = [f'set {k} "{v}"' for k, v in zip(sets[0::2], sets[1::2])]
    (rig.dir / "baseq2" / "scenario.cfg").write_text("\n".join(lines) + "\n")
    return ["+exec", "scenario.cfg"] + rest


def scenario_steps(rig, script, during=()):
    """Map up, the LAN lobby created, the sim clients started; `during` runs
    while they play; then wait for them and quit."""
    return ([rig.wait_ready(), ("sleep", 2), ("cmd", "kexmultiplayer create lan"), ("sleep", 3),
             ("sim", script)] + list(during) + [("simwait",), ("sleep", 1), ("cmd", "quit")])


def embedded_tokens(tap):
    """Name tokens inside a larger argument: the client can't decode those
    (the `queue` bug, playtest row 7)."""
    bad = []
    for r in tap:
        for a in r.get("args", []) if r.get("type") == "locprint" else []:
            if "##P" in a and not TOKEN_RE.fullmatch(a):
                bad.append(f"{r['base']!r} arg {a!r}")
    return bad


def state_line(tap):
    """The HUD's match-state line, from the game tap: whether the status bar
    draws it, and each text it showed, in order (repeats collapsed)."""
    cs = [r for r in tap if r.get("type") == "configstring"]
    drawn = any("loc_stat_cstring" in r["text"] and "yb -78" in r["text"] for r in cs if "hnum" in r["text"])
    index = next((r["index"] for r in cs if r["text"].startswith("WARMUP")), None)
    texts = []
    for r in cs:
        if r["index"] == index and (not texts or texts[-1] != r["text"]):
            texts.append(r["text"])
    return drawn, texts


def tier_human_duel(args, results):
    """Duel through the human path: three simulated players (Alice, Bob,
    Carol), played by ozbot, join like retail clients and run the playtest's
    match-flow and Duel rows: warmup is free play with no clock, `ready`, the
    10-second countdown, `queue`, the timelimit, and the rotation across the
    map change. Q2RED only."""
    rig, why = scenario_rig(args, "t11-human-duel")
    if not rig:
        results.append(("11 human-duel", True, [f"skipped: {why}"]))
        return
    script = """seconds 95
client 0 name=Alice social=sim-alice puppet
client 1 name=Bob social=sim-bob join=1 puppet
client 2 name=Carol social=sim-carol join=2 puppet
at 8 0 cmd score
at 9 1 cmd kill
at 12 0 cmd ready
at 12 1 cmd ready
at 12 2 cmd ready
at 13 0 cmd queue
at 30 0 cmd score
"""
    # match_countdown is left at its default: the playtest asked for 10 s.
    startup = scenario_startup(rig, ["+set", "g_gametype", "duel", "+set", "match_warmup", "1",
                                     "+set", "timelimit", "0.5", "+map", "q2dm1"])
    # ozbot presses no button at intermission, so the level change that a
    # human's press would trigger is issued here (`gamemap`, as ExitLevel does).
    during = [("sleep", 11), ("cmd", "sv match"), ("wait", r"Duel: .* next: ", 80), ("sleep", 3),
              ("cmd", "gamemap q2dm1")]
    log = rig.run(startup, scenario_steps(rig, script, during), timeout=args.boot_timeout + 180)
    tap = read_jsonl(rig.dir / "tap.jsonl")
    detail = []
    humans = {r["name"]: r["slot"] for r in tap if r.get("type") == "connect" and not r.get("bot")}
    if set(humans) != {"Alice", "Bob", "Carol"}:
        results.append(("11 human-duel", False, [f"humans connected: {sorted(humans)}"] + error_lines(log, rig)[:20]))
        return
    socials = {r["name"]: r["social"] for r in tap if r.get("type") == "connect"}
    if socials.get("Alice") != "sim-alice":
        detail.append(f"social IDs {socials}")
    alice, carol = humans["Alice"], humans["Carol"]
    locs = [r for r in tap if r.get("type") == "locprint"]
    to = lambda slot: [r for r in locs if r["to"] in (slot, -1)]

    ready = [r for r in locs if r["base"] == "{} is ready.\n"]
    if len(ready) != 2 or not all(TOKEN_RE.fullmatch(a) for r in ready for a in r["args"]):
        detail.append(f"ready broadcasts {[r['args'] for r in ready]}, expected two with name tokens")
    if not any(r["base"] == "Join the game first.\n" for r in to(carol)):
        detail.append("the queued player's `ready` got no 'Join the game first.'")

    centre = [r["args"][0] for r in to(alice) if r["base"] == "{}" and r["args"]]
    want = ["Match starting"] + [f"Match starts in {n}" for n in range(10, 0, -1)] + ["FIGHT!"]
    got = [c for c in centre if c in want]
    if got[:len(want)] != want:
        detail.append(f"Alice's countdown {got[:14]}, expected {want}")

    queue_lines = [r["args"] for r in locs if r["to"] == alice and r["base"] == "{}. {} ({})\n"]
    if [q[2] for q in queue_lines] != ["duelling", "duelling", "waiting"] or \
            not all(TOKEN_RE.fullmatch(q[1]) for q in queue_lines):
        detail.append(f"`queue` showed {queue_lines}")
    detail += [f"name token inside an argument: {b}" for b in embedded_tokens(tap)[:5]]

    # Warmup: no clock until FIGHT!; then the clock is there.
    fight = next((r["t"] for r in locs if r["to"] == alice and r["args"] == ["FIGHT!"]), None)
    layouts = [r for r in tap if r.get("type") == "layout" and r["to"] == alice]
    before = [r for r in layouts if fight and r["t"] < fight]
    # FIGHT! respawns everyone, which closes the scoreboard; Alice reopens it
    # while live (the intermission scoreboard rightly has no clock).
    after = [r for r in layouts if fight and r["t"] > fight and "m_eou_press_button" not in r["text"]]
    if not before:
        detail.append("no scoreboard layout in warmup, so the clock wasn't checked")
    elif any("time_limit" in r["text"] for r in before):
        detail.append("the warmup scoreboard shows a clock")
    if not after or not any("time_limit" in r["text"] for r in after):
        detail.append("no clock on the scoreboard once the match was live")

    # The HUD's state line: warmup with the ready count, the countdown, the clock.
    drawn, texts = state_line(tap)
    if not drawn:
        detail.append("the status bar doesn't draw the match-state line")
    want_line = ["MATCH STARTS IN 10", "MATCH STARTS IN 1"]
    if not any(re.fullmatch(r"WARMUP - \d+/\d+ READY", t) for t in texts) or \
            not all(w in texts for w in want_line) or not any(re.fullmatch(r"\d+:\d\d", t) for t in texts):
        detail.append(f"state line texts {texts[:4]}...{texts[-3:]}, expected warmup with a ready count, "
                      "MATCH STARTS IN 10..1, then the clock")

    # A warmup suicide doesn't score (`sv match` during warmup).
    warmup_scores = [int(m) for m in re.findall(r"^\S*\s+(-?\d+) (?:Alice|Bob|Carol)", log, re.M)][:3]
    if warmup_scores and any(warmup_scores):
        detail.append(f"warmup scores {warmup_scores} after a suicide, expected 0")
    elif not warmup_scores:
        detail.append("no `sv match` scores in warmup")

    # The rotation survives the map change: the loser waits, the queued plays.
    m = re.search(r"Duel: .+ -?\d+ - -?\d+ .+; next: (\S+)", log)
    if not m:
        detail.append("no Duel result line")
    else:
        loser = m.group(1)
        ended = next((r["t"] for r in tap if r.get("type") == "layout" and "m_eou_press_button" in r["text"]), 0)
        later = [r for r in tap if r.get("type") == "stufftext" and r["t"] > ended and "spectator" in r["text"]]
        spec = {r["to"]: r["text"].strip() for r in later}
        if spec.get(humans.get(loser)) != "spectator 1" or spec.get(carol) != "spectator 0":
            detail.append(f"after the map change: {spec} (slots {humans}), expected {loser} spectating "
                          "and Carol playing")
    if rig.returncode != 0:
        detail.append(f"server exited with code {rig.returncode}")
    detail += error_lines(log, rig)[:20]
    results.append(("11 human-duel", not detail, detail))


def tier_human_teams(args, results):
    """TDM and CTF through the human path: four simulated players pick teams
    with `team red` / `team blue`; the match must not start while only one
    team is staffed; then it runs to its timelimit, and the record has each
    player on the team they chose. Q2RED only."""
    for gt in ("tdm", "ctf"):
        label = f"12 human-teams {gt}"
        rig, why = scenario_rig(args, f"t12-{gt}")
        if not rig:
            results.append((label, True, [f"skipped: {why}"]))
            continue
        script = """seconds 75
client 0 name=Alice social=sim-alice puppet
client 1 name=Bob social=sim-bob puppet
client 2 name=Carol social=sim-carol puppet
client 3 name=Dave social=sim-dave puppet
at 6 0 cmd team red
at 6 1 cmd team red
at 8 0 cmd ready
at 8 1 cmd ready
at 14 2 cmd team blue
at 14 3 cmd team blue
at 16 2 cmd ready
at 16 3 cmd ready
"""
        startup = scenario_startup(rig, ["+set", "g_gametype", gt, "+set", "match_warmup", "1",
                                         "+set", "timelimit", "0.5", "+set", "g_matchlog_dir", "matches",
                                         "+map", GAMETYPE_MAP.get(gt, "q2dm1")])
        log = rig.run(startup, scenario_steps(rig, script), timeout=args.boot_timeout + 160)
        tap = read_jsonl(rig.dir / "tap.jsonl")
        detail = []
        if not re.search(rf"Gametype: .+ \({gt}\)", log):
            detail.append(f"gametype {gt} not selected")
        locs = [r for r in tap if r.get("type") == "locprint"]
        joins = [r for r in locs if r["base"] == "$g_joined_team"]
        starting = next((r["t"] for r in locs if r["args"] == ["Match starting"]), None)
        if len(joins) < 4:
            detail.append(f"{len(joins)} team joins, expected 4")
        elif starting is None:
            detail.append("the match never started")
        elif starting < joins[2]["t"]:
            detail.append("the countdown started with only one team staffed")
        seen = ["Match: " + l.split("Match: ", 1)[1].strip() for l in log.splitlines() if "Match: " in l]
        if seen != MATCH_SEQUENCE:
            detail.append(f"transitions {seen}, expected {MATCH_SEQUENCE}")
        detail += [f"name token inside an argument: {b}" for b in embedded_tokens(tap)[:5]]
        records = sorted(rig.dir.rglob("matches/*.json"))
        if len(records) != 1:
            detail.append(f"{len(records)} match records, expected 1")
        else:
            rec = json.loads(records[0].read_text())
            teams = {p["name"]: p.get("team") for p in rec.get("players", [])}
            want = {"Alice": "red", "Bob": "red", "Carol": "blue", "Dave": "blue"}
            if teams != want:
                detail.append(f"record teams {teams}, expected {want}")
            if [t.get("name") for t in rec.get("teams", [])] != ["red", "blue"]:
                detail.append(f"record team scores {rec.get('teams')}")
        if rig.returncode != 0:
            detail.append(f"server exited with code {rig.returncode}")
        detail += error_lines(log, rig)[:20]
        results.append((label, not detail, detail))


def tier_human_rounds(args, results):
    """HoonyMode's rounds through the human path: two simulated players duel
    with match_rounds 1; each scripted `kill` (a score change, like a frag)
    must end the round - both players held, "Next round in 3..1", FIGHT! -
    and play resume, with the match still ended by its timelimit. Q2RED only."""
    rig, why = scenario_rig(args, "t13-rounds")
    if not rig:
        results.append(("13 human-rounds", True, [f"skipped: {why}"]))
        return
    script = """seconds 85
client 0 name=Alice social=sim-alice puppet
client 1 name=Bob social=sim-bob puppet
at 6 0 cmd ready
at 6 1 cmd ready
at 26 0 cmd kill
at 40 1 cmd kill
"""
    startup = scenario_startup(rig, ["+set", "g_gametype", "duel", "+set", "match_warmup", "1",
                                     "+set", "match_rounds", "1", "+set", "match_round_countdown", "3",
                                     "+set", "match_overtime", "0", "+set", "fraglimit", "0",
                                     "+set", "timelimit", "1", "+map", "q2dm1"])
    log = rig.run(startup, scenario_steps(rig, script), timeout=args.boot_timeout + 160)
    tap = read_jsonl(rig.dir / "tap.jsonl")
    detail = []
    humans = {r["name"]: r["slot"] for r in tap if r.get("type") == "connect" and not r.get("bot")}
    alice = humans.get("Alice")
    centre = [r["args"][0] for r in tap if r.get("type") == "locprint" and r["to"] == alice
              and r["base"] == "{}" and r["args"]]
    breaks = sum(1 for i in range(len(centre) - 3)
                 if centre[i:i + 4] == ["Next round in 3", "Next round in 2", "Next round in 1", "FIGHT!"])
    seen = ["Match: " + l.split("Match: ", 1)[1].strip() for l in log.splitlines() if "Match: " in l]
    round_breaks = seen.count("Match: live -> round over")
    # Bots fight too, so frags can add rounds; the two suicides make at least two.
    if breaks < 2 or round_breaks < 2:
        detail.append(f"{breaks} round countdowns shown, {round_breaks} round breaks logged; expected two "
                      "or more (two scripted suicides)")
    if seen[-1:] != ["Match: ended (timelimit)"]:
        detail.append(f"the match ended {seen[-1:]}, expected by the timelimit")
    drawn, texts = state_line(tap)
    for want in ("ROUND OVER", "NEXT ROUND IN 3"):
        if want not in texts:
            detail.append(f"the state line never showed {want!r}")
    if not any(t.startswith("ROUND 2 - ") for t in texts):
        detail.append("the state line never showed round 2")
    detail += [f"name token inside an argument: {b}" for b in embedded_tokens(tap)[:5]]
    if rig.returncode != 0:
        detail.append(f"server exited with code {rig.returncode}")
    detail += error_lines(log, rig)[:20]
    results.append(("13 human-rounds", not detail, detail))


def tier_teams(args, results):
    """TDM and CTF with the match flow: four bots on two teams play warmup ->
    live -> timelimit, and the record has both teams and every player on one."""
    for gt in ("tdm", "ctf"):
        rig = RIGS[args.engine_kind](args, args.candidate, f"t8-{gt}")
        bots = (["+set", "sv_bot_brain", "ozbot", "+set", "bot_count", "4", "+set", "bot_skill", "3"]
                if rig.kind == "q2red" else ["+set", "bot_minPlayers", "4"])
        startup = (["+set", "deathmatch", "1", "+set", "maxclients", "8", "+set", "g_gametype", gt,
                    "+set", "match_warmup", "1", "+set", "match_countdown", "3", "+set", "timelimit", "0.25",
                    "+set", "g_matchlog_dir", "matches"] + bots + ["+map", GAMETYPE_MAP.get(gt, "q2dm1")])
        log = rig.run(startup, [rig.wait_ready(), ("wait", r"Match: ended", 90), ("sleep", 2), ("cmd", "quit")],
                      timeout=args.boot_timeout + 150)
        detail = []
        if not re.search(rf"Gametype: .+ \({gt}\)", log):
            detail.append(f"gametype {gt} not selected")
        seen = ["Match: " + l.split("Match: ", 1)[1].strip() for l in log.splitlines() if "Match: " in l]
        if seen != MATCH_SEQUENCE:
            detail.append(f"transitions {seen}, expected {MATCH_SEQUENCE}")
        records = sorted(rig.dir.rglob("matches/*.json"))
        if len(records) != 1:
            detail.append(f"{len(records)} match records, expected 1")
        else:
            try:
                rec = json.loads(records[0].read_text())
                if rec.get("gametype") != gt or rec.get("reason") != "timelimit":
                    detail.append(f"record gametype/reason {rec.get('gametype')}/{rec.get('reason')}")
                if [t.get("name") for t in rec.get("teams", [])] != ["red", "blue"]:
                    detail.append(f"record teams {rec.get('teams')}")
                playing = [p for p in rec.get("players", []) if p.get("playing")]
                if {p.get("team") for p in playing} != {"red", "blue"}:
                    detail.append(f"playing clients' teams {[p.get('team') for p in playing]}, expected red and blue")
            except ValueError as e:
                detail.append(f"match record is not JSON: {e}")
        if rig.returncode != 0:
            detail.append(f"server exited with code {rig.returncode}")
        detail += error_lines(log, rig)[:20]
        results.append((f"8 teams {gt}", not detail, detail))


def tier_warmup(args, results):
    """Warmup is free play: six bots fight in a warmup that never ends
    (match_minplayers 99), and `sv match` must show every score still 0 -
    frags and suicides alike. Q2RED only: needs bots that fight."""
    rig = RIGS[args.engine_kind](args, args.candidate, "t9-warmup")
    if not rig.expects_frags:
        results.append(("9 warmup", True, ["skipped: bots don't fight in a bot-only match on this engine"]))
        return
    startup = (["+set", "deathmatch", "1", "+set", "maxclients", "8", "+set", "match_warmup", "1",
                "+set", "match_minplayers", "99", "+set", "timelimit", "10"] + rig.bot_args() + ["+map", "q2dm1"])
    log = rig.run(startup, [rig.wait_ready(), ("sleep", args.dm_seconds), ("cmd", "sv match"),
                            ("sleep", 2), ("cmd", "quit")],
                  timeout=args.boot_timeout + args.dm_seconds + 60)
    frags = [l for l in log.splitlines() if re.search(r"\b(killed|was |by |fragged)\b", l) and "Nav" not in l]
    report = log.split("Match: warmup\n", 2)[-1] if "Match: warmup" in log else ""
    scores = [int(m) for m in re.findall(r"^\S*\s+(-?\d+) \S", report, re.M)]
    detail = []
    if not frags:
        detail.append("no frags in warmup, so nothing was tested")
    if not scores:
        detail.append("no scores in the `sv match` report")
    elif any(scores):
        detail.append(f"scores {scores} in warmup, expected all 0")
    if rig.returncode != 0:
        detail.append(f"server exited with code {rig.returncode}")
    detail += error_lines(log, rig)[:20]
    results.append(("9 warmup", not detail, detail + [f"({len(frags)} obituary lines)"]))


def tier_rounds(args, results):
    """match_rounds on (HoonyMode's mechanics): a frag ends the round, a pause,
    everyone respawns frozen for the countdown, the next round starts and the
    bots fight on. Needs bots that
    fight, so Q2RED only."""
    rig = RIGS[args.engine_kind](args, args.candidate, "t7-rounds")
    if not rig.expects_frags:
        results.append(("7 rounds", True, ["skipped: bots don't fight in a bot-only match on this engine"]))
        return
    startup = (["+set", "deathmatch", "1", "+set", "maxclients", "8", "+set", "match_warmup", "1",
                "+set", "match_countdown", "3", "+set", "match_rounds", "1", "+set", "match_round_countdown", "2",
                "+set", "timelimit", "3"] + rig.bot_args() + ["+map", "q2dm1"])
    # A frag ends the round (the pause), everyone respawns frozen (the
    # countdown), the round starts - and a second frag ends that one too: the
    # bots still play after a round reset (the old hold wiped their SVF_BOT).
    log = rig.run(startup, [rig.wait_ready(), ("wait", r"Match: live -> round over", 150),
                            ("wait", r"Match: round countdown -> live", 20),
                            ("wait", r"(?s)round countdown -> live.*Match: live -> round over", 150),
                            ("sleep", 1), ("cmd", "quit")],
                  timeout=args.boot_timeout + 360)
    detail = []
    seen = ["Match: " + l.split("Match: ", 1)[1].strip() for l in log.splitlines() if "Match: " in l]
    cycle = ["Match: live -> round over", "Match: round over -> round countdown", "Match: round countdown -> live"]
    if not any(seen[i:i + 3] == cycle for i in range(len(seen))):
        detail.append(f"no full round cycle in {seen}")
    if seen.count("Match: live -> round over") < 2:
        detail.append("no second round ended by a frag: the bots stopped playing after the round reset")
    if rig.returncode != 0:
        detail.append(f"server exited with code {rig.returncode}")
    if "==== ShutdownGame ====" not in log:
        detail.append("no clean ShutdownGame")
    detail += error_lines(log, rig)[:20]
    results.append(("7 rounds", not detail, detail))


TIERS = (tier_save_diff, tier_load_stock, tier_dm_smoke, tier_gametype_boot, tier_match_flow, tier_duel,
         tier_rounds, tier_teams, tier_warmup, tier_duel_rotation, tier_human_duel, tier_human_teams,
         tier_human_rounds)


def run_tier(tier, args):
    results = []
    try:
        tier(args, results)
    except Exception as e:  # a crashed tier is a failed tier, not a crashed harness
        results.append((tier.__name__[5:], False, [str(e)]))
    return results


def cmd_test(args):
    tiers = [t for t in TIERS if not args.only or t.__name__ == f"tier_{args.only}"]
    if args.jobs <= 1:
        per_tier = [run_tier(t, args) for t in tiers]
    else:
        # Each tier is its own servers in its own rig directories; in parallel
        # each also gets its own ports. Results print in tier order.
        def tier_args(i):
            a = copy.copy(args)
            a.port, a.lobby_port = args.port + 20 * i, args.lobby_port + 20 * i
            return a
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            futures = [pool.submit(run_tier, t, tier_args(i)) for i, t in enumerate(tiers)]
            per_tier = [f.result() for f in futures]
    results = [r for rs in per_tier for r in rs]
    failed = 0
    print(f"engine: {args.engine_kind}")
    for name, ok, detail in results:
        print(f"{'PASS' if ok else 'FAIL'}  tier {name}")
        for d in detail:
            print(f"      {d}")
        failed += not ok
    return 1 if failed else 0


def cmd_calibrate(args):
    """Run the stock module against itself and record every field that differs
    between identical runs. Those differences are timing noise, not changes."""
    saves = [coop_save(args, args.stock, f"cal{i}")[1] for i in range(args.runs)]
    path = volatile_file(args.engine_kind)
    fields = read_list(path)
    before = set(fields)
    counts = collections.Counter()
    for s in saves[1:]:
        for f in SAVE_FILES:
            a, b = dict(flatten(saves[0][f])), dict(flatten(s[f]))
            cls = {**classnames_of(s[f]), **classnames_of(saves[0][f])}
            for k in a.keys() | b.keys():
                if a.get(k) != b.get(k):
                    fields.add(f"{f}:{field_of(k, cls)}")
                    counts[f"{f}:{field_of(k, cls)}"] += 1
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        f"# Save fields whose values differ between identical runs of the stock module on {args.engine_kind}\n"
        "# (timing noise: the save happens after a wall-clock delay, not on a fixed frame).\n"
        "# Generated by `q2harness.py calibrate`; tier 1 ignores their values and lets them\n"
        "# come and go. Only ever grows; review additions in the PR.\n"
        + "\n".join(sorted(fields)) + "\n")
    print(f"calibrate: {len(fields)} volatile fields ({len(fields - before)} new) from {args.runs} runs")
    for k, n in counts.most_common():
        print(f"  {n:4d}  {k}")
    return 0


def main():
    p = pins()
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=["test", "calibrate"])
    ap.add_argument("--engine-kind", choices=sorted(RIGS), default="q2red")
    ap.add_argument("--stock", required=True, help="stock reference module (.so for q2red, .dll for q2rex)")
    ap.add_argument("--candidate", help="module under test (test only)")
    ap.add_argument("--engine", default=f"/srv/q2ci/engines/q2red-{p.get('Q2RED_COMMIT')}")
    ap.add_argument("--rex-engine", default=f"/srv/q2ci/engines/q2rex-{p.get('Q2REX_VERSION')}")
    ap.add_argument("--bots", default=f"/srv/q2ci/engines/ozbot-{p.get('OZBOT_COMMIT')}")
    ap.add_argument("--retail", default="/srv/q2ci/retail")
    ap.add_argument("--wine", default=shutil.which("wine") or "wine")
    ap.add_argument("--online", dest="offline", action="store_false",
                    help="let Q2REX reach the network (default: offline namespace)")
    ap.add_argument("--wineprefix", default=os.path.expanduser("~/.cache/q2harness-wine"),
                    help="shared Wine prefix, created on first use")
    ap.add_argument("--workdir", default=None, help="where rigs are created (default: a temp dir)")
    ap.add_argument("--keep", action="store_true", help="keep rig directories for inspection")
    ap.add_argument("--jobs", type=int, default=1,
                    help="tiers run at once, each on its own ports (Q2RED; keep 1 for Q2REX's shared Wine prefix)")
    ap.add_argument("--port", type=int, default=28010)
    ap.add_argument("--lobby-port", type=int, default=5150)
    ap.add_argument("--boot-timeout", type=float, default=None,
                    help="seconds to wait for the map to load (default 30 q2red, 120 q2rex)")
    ap.add_argument("--settle", type=float, default=6, help="seconds after the map loads before saving")
    ap.add_argument("--dm-seconds", type=float, default=60)
    ap.add_argument("--runs", type=int, default=6, help="calibration runs")
    ap.add_argument("--gametypes", default="ffa,duel,tdm,ctf",
                    help="gametype ids tier 4 boots (the registry's, GAME_GAMETYPES)")
    ap.add_argument("--stock-gamerules", default="2,3",
                    help="Rogue's stock gamerules values tier 4 also boots (Tag, DeathBall)")
    ap.add_argument("--boot-seconds", type=float, default=20, help="tier 4 run time per gametype")
    ap.add_argument("--only", choices=["save_diff", "load_stock", "dm_smoke", "gametype_boot", "match_flow", "duel", "rounds", "teams", "warmup", "duel_rotation", "human_duel", "human_teams", "human_rounds"])
    args = ap.parse_args()
    if args.command == "test" and not args.candidate:
        ap.error("test needs --candidate")
    if args.bots and not pathlib.Path(args.bots).is_dir():
        args.bots = None
    if args.boot_timeout is None:
        args.boot_timeout = 120 if args.engine_kind == "q2rex" else 30
    own_workdir = args.workdir is None
    args.workdir = args.workdir or tempfile.mkdtemp(prefix="q2h-", dir="/tmp")
    try:
        return cmd_test(args) if args.command == "test" else cmd_calibrate(args)
    finally:
        if own_workdir and not args.keep:
            shutil.rmtree(args.workdir, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
