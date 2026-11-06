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
  5. match-flow  match_flow on: warmup -> countdown -> live -> intermission
                 (bot-only tiers set match_allow_no_humans 1: a match needs a human)
                 on the timelimit, counted from LIVE.
  6. duel        Three bots: two duel, one queues; the match ends, the loser
                 rotates to the back and a match record is written.
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
 14. human-vote  Map votes through simulated players (Q2RED).
 15. maps        The shuffled map rotation: off-list start, a whole pass,
                 no repeat at the reshuffle, g_map_list kept as written.
 16. human-settings  Setting votes: a time limit applied in a live match, the
                 refusals, a powerups vote reloading with the time kept (Q2RED).
 17. human-teamlock  Team locks and captains in TDM: locked at LIVE, a
                 newcomer refused, a dropped player back in, unlock, kick,
                 the captaincy passed on, an emptied team unlocked (Q2RED).
 18. human-join  Arriving spectating with the menu; `join` and the menu;
                 players kept across `gamemap` and `map`; Duel's queue by
                 asking; TDM teams kept (Q2RED).
 19. human-readyvote  The ready votes in a TDM ready-up: readyteam voted on by
                 the caller's team alone, readyall readying everyone (Q2RED).
 20. human-shuffle  The shuffle vote in a TDM warmup: a new even split, team
                 skins to match, the locks off; refused once live (Q2RED).
 22. entities    A .ent override with MuffMode's mode keys, ammo and jump
                 pad: each mode spawns what its keys say; sv saveents.

    q2harness.py test      --stock S --candidate C   # tiers 1-3
    q2harness.py calibrate --stock S --runs 6        # refresh the volatile list

Engines, retail data and bots default to the pinned installs under /srv/q2ci
(scripts/harness/pins.env), or, off the jumpbox, to the local kit that
scripts/harness/setup-local-kit.sh builds in ~/.cache/q2harness-kit (which
also supplies --stock). Each run gets a fresh rig directory, so runs never
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


def install_root():
    """Where the pinned engines, bots and retail data live: the jumpbox's
    /srv/q2ci, else a developer's local kit (scripts/harness/setup-local-kit.sh,
    ~/.cache/q2harness-kit or $KIT)."""
    if pathlib.Path("/srv/q2ci/engines").is_dir():
        return pathlib.Path("/srv/q2ci")
    cache = pathlib.Path(os.environ.get("XDG_CACHE_HOME") or pathlib.Path.home() / ".cache")
    return pathlib.Path(os.environ.get("KIT") or cache / "q2harness-kit")


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
             ("wait", regex, seconds, n) ... to match n times
             ("exists", path, seconds) wait for a file to appear
             ("sleep", seconds)
             ("cmd", text)             send a console command over stdin
             ("cmdf", fn)              send fn(console log text)
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
            count = step[3] if len(step) > 3 else 1
            while len(regex.findall(self.log_path.read_text(errors="replace"))) < count:
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
        elif step[0] in ("cmd", "cmdf"):
            text = step[1] if step[0] == "cmd" else step[1](self.log_path.read_text(errors="replace"))
            try:
                proc.stdin.write(text + "\n")
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
    # Inside it the session's /run/user/<uid> isn't ours, and Wine sometimes
    # says so (CI, PR #39). Giving the rig its own XDG_RUNTIME_DIR silenced
    # it but stopped Q2REX writing saves, so it's listed here instead.
    benign_re = re.compile(r"Playfab::signin -- Login failed|error: XDG_RUNTIME_DIR is invalid or not set")
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
    timelimit, counted from LIVE, ends it (docs/design/match-flow.md); going
    live puts the movers back (q2dm1's plats)."""
    rig = RIGS[args.engine_kind](args, args.candidate, "t5-match")
    startup = (["+set", "deathmatch", "1", "+set", "maxclients", "8", "+set", "match_flow", "1", "+set", "match_allow_no_humans", "1",
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
    # The arena reset at LIVE puts the movers back (q2dm1: its two plats).
    resets = [int(n) for n in re.findall(r"Movers: (\d+) reset", log)]
    if not resets or resets[0] < 2:
        detail.append(f"movers reset at LIVE: {resets}, expected q2dm1's two plats")
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
                "+set", "match_flow", "1", "+set", "match_allow_no_humans", "1", "+set", "match_countdown", "3", "+set", "timelimit", "0.25",
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
                "+set", "match_flow", "1", "+set", "match_allow_no_humans", "1", "+set", "match_countdown", "3", "+set", "timelimit", "0.25",
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


def tier_maps(args, results):
    """The map rotation (docs/decisions/0014): a server started on a map
    outside its shuffled g_map_list goes to the list next; each level end
    leads to the announced map (the harness loads it with `gamemap`, as the
    intermission's exit would: bots press no button); the first pass plays
    every listed map once, the next pass doesn't open with the map that just
    played, and g_map_list stays as written. The play order (g_map_order) is
    internal: the console can't set it."""
    rig = RIGS[args.engine_kind](args, args.candidate, "t15-maps")
    rotation = ["q2dm3", "q2dm4", "q2dm5"]
    startup = (["+set", "deathmatch", "1", "+set", "maxclients", "8", "+set", "match_flow", "0",
                "+set", "timelimit", "0.25", "+set", "g_map_list", " ".join(rotation),
                "+set", "g_map_list_shuffle", "1"] + rig.bot_args() + ["+map", "q2dm1"])
    announced = r"Maps: next (\S+)"

    def go_next(log):
        return "gamemap " + re.findall(announced, log)[-1]

    steps = [rig.wait_ready()]
    for n in range(1, 5):
        steps += [("wait", announced, 90, n), ("sleep", 2)]
        if n == 1:
            steps += [("cmd", "set g_map_order junk"), ("cmd", "g_map_order")]
        if n < 4:
            steps += [("cmdf", go_next)]
    steps += [("cmd", "sv maplist"), ("cmd", "g_map_list"), ("sleep", 2), ("cmd", "quit")]
    log = rig.run(startup, steps, timeout=args.boot_timeout + 420)
    seen = re.findall(announced, log)
    detail = []
    if len(seen) < 4:
        detail.append(f"announced {seen}, expected four level ends")
    else:
        if sorted(seen[:3]) != rotation:
            detail.append(f"first pass {seen[:3]}, expected each of {rotation} once (from off-list q2dm1)")
        if seen[3] == seen[2]:
            detail.append(f"the next pass opened with {seen[3]}, the map that just played")
    if "Map rotation (shuffled): " not in log:
        detail.append("no `sv maplist` rotation line")
    if not re.search(r'"g_map_list" is "%s' % " ".join(rotation), log):
        detail.append("g_map_list was rewritten")
    if '"g_map_order" is "junk' in log or not re.search(r'"g_map_order" is "q2dm', log):
        detail.append("the console set g_map_order (it's internal: CVAR_NOSET)")
    if rig.returncode != 0:
        detail.append(f"server exited with code {rig.returncode}")
    detail += error_lines(log, rig)[:20]
    results.append(("15 maps", not detail, detail))


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
    """The HUD's match status and clock, from the game tap: whether the status
    bar draws both (status top middle, clock bottom middle), and every text either showed, in order
    (repeats collapsed).
    The status is the configstring that first shows a warmup text (WAITING
    FOR ..., READY UP n/m, WARMUP); the clock is the next one
    (docs/design/hud.md)."""
    cs = [r for r in tap if r.get("type") == "configstring"]
    bars = [r["text"] for r in cs if "hnum" in r["text"]]
    drawn = any("yt 43 loc_stat_cstring2 " in t and "yb -61 loc_stat_cstring2 " in t for t in bars)
    status = next((r["index"] for r in cs
                   if r["text"].startswith(("WARMUP", "READY UP ", "WAITING FOR "))), None)
    texts = []
    for r in cs:
        if status is not None and r["index"] in (status, status + 1) and r["text"] and \
                (not texts or texts[-1] != r["text"]):
            texts.append(r["text"])
    return drawn, texts


def tier_human_duel(args, results):
    """Duel through the human path: three simulated players (Alice, Bob,
    Carol), played by ozbot, join like retail clients and run the playtest's
    match-flow and Duel rows: warmup is free play with no clock, `ready`, the
    10-second countdown, `queue`, the timelimit, and the rotation across the
    map change. Duel's own scoreboard (g_scoreboard 1): the queue's place
    and ping in warmup, the winner (and no record) at intermission. Q2RED only."""
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
    startup = scenario_startup(rig, ["+set", "g_auto_join", "1", "+set", "g_scoreboard", "1", "+set", "g_gametype", "duel", "+set", "match_flow", "1",
                                     "+set", "timelimit", "0.5", "+map", "q2dm1"])
    # ozbot presses no button at intermission, so the level change that a
    # human's press would trigger is issued here (`gamemap`, as ExitLevel does).
    during = [("sleep", 11), ("cmd", "sv match"), ("wait", r"Duel: .* next: ", 80), ("sleep", 3),
              ("cmd", "gamemap q2dm1"), ("sleep", 12), ("cmd", "sv match")]
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

    # The countdown's seconds are the HUD's big digits now; the centre prints
    # bracket it.
    centre = [r["args"][0] for r in to(alice) if r["base"] == "{}" and r["args"]]
    if centre[:2] != ["Match starting", "FIGHT!"]:
        detail.append(f"Alice's centre prints {centre[:4]}, expected 'Match starting' then 'FIGHT!'")

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

    # Duel's board: in warmup the two duellists' blocks and Carol first in
    # the queue (place 1, then her ping); at intermission the winner marked,
    # no win/loss record (Chris, 2026-10-30).
    if not before or "Duel on " not in before[0]["text"] or before[0]["text"].count(" client ") != 2 \
            or not re.search(rf"ctf -40 \d+ {carol} 1 \d+ \"\" ", before[0]["text"]):
        detail.append(f"Alice's warmup board: {before[0]['text'] if before else None}")
    # (The first goes out a frame before the result: the last is the one shown.)
    final = [r["text"] for r in layouts if "m_eou_press_button" in r["text"]]
    if not final or "string2 WINNER" not in final[-1] or '"W 1' in final[-1] \
            or "Match time " not in final[-1]:
        detail.append(f"Alice's intermission board: {final[-1] if final else None}")

    # The HUD's state line: warmup, the countdown, the clock.
    drawn, texts = state_line(tap)
    if not drawn:
        detail.append("the status bar doesn't draw the match status and clock")
    # Ready-up shows how many of the two duellists are ready (Chris, 2026-10-29).
    if not any(re.fullmatch(r"READY UP [0-2]/2", t) for t in texts) or "COUNTDOWN" not in texts \
            or not any(re.fullmatch(r"\d+:\d\d", t) for t in texts):
        detail.append(f"state line texts {texts[:4]}...{texts[-3:]}, expected READY UP n/2, "
                      "COUNTDOWN, then the clock")

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
        # `sv match` after the map change: who plays (the KEX client gets no
        # stuffed `spectator`, so the server's report is what shows it).
        dumps = sv_match_dumps(log)
        after = dumps[-1] if len(dumps) >= 2 else {}
        if after.get(loser) is not False or after.get("Carol") is not True:
            detail.append(f"after the map change: playing {after}, expected {loser} spectating and Carol playing")
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
        startup = scenario_startup(rig, ["+set", "g_gametype", gt, "+set", "match_flow", "1",
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


def tier_teams(args, results):
    """TDM and CTF with the match flow: four bots on two teams play warmup ->
    live -> timelimit, and the record has both teams and every player on one."""
    for gt in ("tdm", "ctf"):
        rig = RIGS[args.engine_kind](args, args.candidate, f"t8-{gt}")
        bots = (["+set", "sv_bot_brain", "ozbot", "+set", "bot_count", "4", "+set", "bot_skill", "3"]
                if rig.kind == "q2red" else ["+set", "bot_minPlayers", "4"])
        startup = (["+set", "deathmatch", "1", "+set", "maxclients", "8", "+set", "g_gametype", gt,
                    "+set", "match_flow", "1", "+set", "match_allow_no_humans", "1", "+set", "match_countdown", "3", "+set", "timelimit", "0.25",
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
    startup = (["+set", "deathmatch", "1", "+set", "maxclients", "8", "+set", "match_flow", "1",
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
    # The warmup loadout (match_warmup_weapons, on by default): every weapon
    # q2dm1 places - not the BFG, which it doesn't.
    loadout = next((l for l in log.splitlines() if "Warmup: loadout " in l), "")
    if "weapon_railgun" not in loadout or "weapon_bfg" in loadout:
        detail.append(f"warmup loadout line {loadout!r}, expected q2dm1's weapons with the railgun")
    if rig.returncode != 0:
        detail.append(f"server exited with code {rig.returncode}")
    detail += error_lines(log, rig)[:20]
    results.append(("9 warmup", not detail, detail + [f"({len(frags)} obituary lines)"]))


def tier_human_vote(args, results):
    """Voting through the human path (docs/design/voting.md): three simulated
    players in an FFA warmup, started the admin's way (g_ruleset, with a
    rulesets/ffa.cfg that holds the warmup open). A map vote two of them refuse fails; its caller
    can't call again at once; a map off the list is refused with the list;
    then a vote one other player answers with stock's `yes` passes, shows on
    the HUD's status line, and loads the map. In the warmup Carol has the
    map's railgun (the warmup loadout) but not the BFG it doesn't place.
    Q2RED only."""
    rig, why = scenario_rig(args, "t14-vote")
    if not rig:
        results.append(("14 human-vote", True, [f"skipped: {why}"]))
        return
    script = """seconds 40
client 0 name=Alice social=sim-alice puppet
client 1 name=Bob social=sim-bob puppet
client 2 name=Carol social=sim-carol puppet
at 8 0 cmd callvote map q2dm2
at 9 1 cmd vote no
at 9 2 cmd vote no
at 12 0 cmd callvote map q2dm3
at 10 2 cmd use railgun
at 11 2 cmd use bfg10k
at 13 1 cmd callvote map q2dm9
at 14 1 cmd callvote map q2dm3
at 15 2 cmd yes
"""
    # The server starts its mode the admin's way (docs/admin.md): server.cfg
    # names it, and rulesets/ffa.cfg changes it - match_minplayers 99, so the
    # warmup never ends and votes stay open.
    rulesets = rig.dir / "baseq2" / "rulesets"
    rulesets.mkdir(parents=True, exist_ok=True)
    (rulesets / "ffa.cfg").write_text("set match_minplayers 99\n")
    startup = scenario_startup(rig, ["+set", "g_auto_join", "1", "+set", "g_ruleset", "ffa", "+map", "q2dm1"])
    log = rig.run(startup, scenario_steps(rig, script), timeout=args.boot_timeout + 120)
    tap = read_jsonl(rig.dir / "tap.jsonl")
    detail = []
    humans = {r["name"]: r["slot"] for r in tap if r.get("type") == "connect" and not r.get("bot")}
    if set(humans) != {"Alice", "Bob", "Carol"}:
        results.append(("14 human-vote", False, [f"humans connected: {sorted(humans)}"] + error_lines(log, rig)[:20]))
        return
    if "Ruleset: starting ffa (Free For All)" not in log:
        detail.append("server.cfg's g_ruleset ffa was never applied at startup")
    seen = [l.split("Vote: ", 1)[1].strip() for l in log.splitlines() if "Vote: " in l]
    want = ["Alice called map q2dm2", "map q2dm2 failed", "Bob called map q2dm3", "map q2dm3 passed",
            "running map q2dm3"]
    if seen != want:
        detail.append(f"vote log {seen}, expected {want}")
    locs = [r for r in tap if r.get("type") == "locprint"]
    to = lambda name: [" ".join([r["base"]] + r.get("args", [])) for r in locs if r["to"] == humans[name]]
    if not any("wait a little" in t for t in to("Alice")):
        detail.append("Alice's second call wasn't refused for the cooldown")
    if not any("Maps: q2dm1 q2dm2" in t for t in to("Bob")):
        detail.append("Bob's call for q2dm9 wasn't answered with the map list")
    out_of = [r["args"] for r in locs if r["to"] == humans["Carol"] and r["base"] == "$g_out_of_item"]
    if out_of != [["$item_bfg10k"]]:
        detail.append(f"Carol's `use` refusals {out_of}, expected the BFG's alone (the railgun is the loadout's)")
    texts = [r["text"] for r in tap if r.get("type") == "configstring" and r["text"].startswith("VOTE")]
    for line in ("VOTE: MAP Q2DM2 - YES 1/3 - ", "VOTE: MAP Q2DM3 - YES 1/3 - ", "VOTE PASSED"):
        if not any(t.startswith(line) for t in texts):
            detail.append(f"the status line never showed {line!r} (saw {sorted(set(texts))})")
    if not re.search(r"Vote: running map q2dm3[\s\S]*InitGame[\s\S]*Gametype:", log):
        detail.append("q2dm3 never loaded after the vote")
    detail += [f"name token inside an argument: {b}" for b in embedded_tokens(tap)[:5]]
    if rig.returncode != 0:
        detail.append(f"server exited with code {rig.returncode}")
    detail += error_lines(log, rig)[:20]
    results.append(("14 human-vote", not detail, detail))


def tier_human_settings(args, results):
    """Setting votes (decision 0015) through the human path: three simulated
    players in a live FFA match (10 minutes). A time limit vote passes and the
    clock moves without a restart; friendly fire is refused outside team
    modes and a time out of range with the usage; a powerups vote reloads the
    map, and the voted time limit is still there after it. The built-in
    ruleset's column scoreboard (decision 0018) shows Bob's suicide as a
    death, and its frag messages tell both players of each frag, and the
    clients are told classic jumps are on. Q2RED only."""
    rig, why = scenario_rig(args, "t16-settings")
    if not rig:
        results.append(("16 human-settings", True, [f"skipped: {why}"]))
        return
    # Live at about 20 s (the join grace, then a 3 s countdown): the votes come after.
    script = """seconds 55
client 0 name=Alice social=sim-alice puppet
client 1 name=Bob social=sim-bob puppet
client 2 name=Carol social=sim-carol puppet
at 26 1 cmd kill
at 28 0 cmd score
at 30 0 cmd callvote timelimit 7
at 31 1 cmd vote yes
at 36 1 cmd callvote friendlyfire on
at 37 1 cmd callvote timelimit 99
at 39 2 cmd callvote powerups off
at 40 0 cmd vote yes
"""
    # No ready-up, a short countdown.
    rulesets = rig.dir / "baseq2" / "rulesets"
    rulesets.mkdir(parents=True, exist_ok=True)
    (rulesets / "ffa.cfg").write_text("set match_ready 0\nset match_countdown 3\n")
    startup = scenario_startup(rig, ["+set", "g_auto_join", "1", "+set", "g_ruleset", "ffa", "+map", "q2dm1"])
    log = rig.run(startup, scenario_steps(rig, script), timeout=args.boot_timeout + 120)
    tap = read_jsonl(rig.dir / "tap.jsonl")
    detail = []
    humans = {r["name"]: r["slot"] for r in tap if r.get("type") == "connect" and not r.get("bot")}
    if set(humans) != {"Alice", "Bob", "Carol"}:
        results.append(("16 human-settings", False,
                        [f"humans connected: {sorted(humans)}"] + error_lines(log, rig)[:20]))
        return
    seen = [l.split("Vote: ", 1)[1].strip() for l in log.splitlines() if "Vote: " in l]
    want = ["Alice called time limit 7 minutes", "time limit 7 minutes passed", "setting time limit 7 minutes",
            "Carol called powerups off", "powerups off passed", "setting powerups off"]
    if seen != want:
        detail.append(f"vote log {seen}, expected {want}")
    # The time limit applies in place: no reload between it and the powerups vote, which reloads.
    at_time, at_powerups = log.find("Vote: setting time limit"), log.find("Vote: setting powerups")
    if at_time < 0 or at_powerups < 0 or "InitGame" in log[at_time:at_powerups]:
        detail.append("the time limit vote reloaded the map (or never ran)")
    if at_powerups < 0 or "InitGame" not in log[at_powerups:]:
        detail.append("the powerups vote didn't reload the map")
    # The column board: Alice's, opened live after Bob's suicide.
    boards = [r["text"] for r in tap if r.get("type") == "layout" and r["to"] == humans["Alice"]
              and " string2 FPH " in r["text"]]
    if not boards:
        detail.append("Alice was never shown the column scoreboard")
    else:
        board = boards[0]
        rows = {int(m[0]): (int(m[1]), int(m[2])) for m in re.findall(r"ctf 16 \d+ (\d+) (-?\d+) (\d+) ", board)}
        if set(rows) != set(humans.values()):
            detail.append(f"board rows for slots {sorted(rows)}, expected {sorted(humans.values())}")
        elif rows[humans["Bob"]] != (-1, 1):
            detail.append(f"Bob's row shows frags/deaths {rows[humans['Bob']]}, expected (-1, 1) after his suicide")
        if len(board) > 1023:
            detail.append(f"the board is {len(board)} bytes, over the client's 1024")
    if "-> live" not in log[:max(at_time, 0)]:
        detail.append("the match wasn't live when the time limit vote ran")
    locs = [r for r in tap if r.get("type") == "locprint"]
    to = lambda name: [" ".join([r["base"]] + r.get("args", [])) for r in locs if r["to"] == humans[name]]
    if not any("only for team gametypes" in t for t in to("Bob")):
        detail.append("Bob's friendly fire vote in FFA wasn't refused")
    if not any("Usage: callvote timelimit 1-60" in t for t in to("Bob")):
        detail.append("Bob's time limit of 99 wasn't answered with the usage")
    # The clock: 9:xx before the vote, 6:xx after it (7 minutes from LIVE),
    # and 7:00 again in the reloaded level's warmup and countdown.
    clocks = [r["text"] for r in tap if r.get("type") == "configstring" and re.fullmatch(r"\d+:\d\d", r["text"])]
    for pattern, what in ((r"9:\d\d", "the 10-minute match's clock"), (r"6:\d\d", "the clock moved to 7 minutes"),
                          (r"7:00", "7:00 after the reload")):
        if not any(re.fullmatch(pattern, c) for c in clocks):
            detail.append(f"never saw {what} (clock texts {sorted(set(clocks))[:12]})")
    # Frag messages (g_frag_messages, on in the built-in ruleset): each frag
    # between the players tells both, the other's name a whole argument, and
    # the killer's place while live. Puppets don't always frag: none is noted.
    victims = [r for r in locs if r["base"] == "You were fragged by {}"]
    killers = [r for r in locs if r["base"].startswith("You fragged {}")]
    notes = []
    if not victims and not killers:
        notes.append("no frag between the players: frag messages unchecked")
    elif len(victims) != len(killers):
        detail.append(f"{len(killers)} 'You fragged' for {len(victims)} 'You were fragged by'")
    # Live: the place line; warmup (after the powerups reload): the name only.
    odd = [r["base"] for r in killers if not re.fullmatch(
        r"You fragged \{\}(\n(Tied for )?\d+(st|nd|rd|th) place with -?\d+)?", r["base"])]
    if odd:
        detail.append(f"killer messages not in FFA's form: {odd[:3]}")
    if any(len(r.get("args", [])) != 1 for r in victims + killers):
        detail.append("a frag message without the other player's name as its one argument")
    # Classic jumps (decision 0024) are on in deathmatch by default, and the
    # clients are told, so their prediction jumps as the server does.
    classic = [r["text"] for r in tap if r.get("type") == "configstring" and r["index"] == CONFIG_CLASSIC_JUMPS]
    if not classic or classic[-1] != "1":
        detail.append(f"classic jumps' configstring {CONFIG_CLASSIC_JUMPS} was {classic}, expected '1'")
    detail += [f"name token inside an argument: {b}" for b in embedded_tokens(tap)[:5]]
    if rig.returncode != 0:
        detail.append(f"server exited with code {rig.returncode}")
    detail += error_lines(log, rig)[:20]
    results.append(("16 human-settings", not detail, detail or notes))


def tier_human_teamlock(args, results):
    """Team locks and captains (decision 0016) through the human path: TDM
    on the built-in ruleset (match_lock 1), five simulated players. The
    first on each team is captain; a player changes team from the Tab menu
    (playtest 2026-10-29: it did nothing); both teams lock as the match goes live; a
    newcomer is refused (told alone), a rostered player who drops gets back
    in; the captain unlocks for the newcomer, relocks and kicks them (with no join menu left open: a jump would
    select it), and
    they're refused again; the captain leaves and the next teammate takes
    over; a team whose players all leave unlocks itself. Q2RED only."""
    label = "17 human-teamlock"
    rig, why = scenario_rig(args, "t17-teamlock")
    if not rig:
        results.append((label, True, [f"skipped: {why}"]))
        return
    # Live at about 20 s (the join grace, then a 3 s countdown).
    script = """seconds 66
client 0 name=Alice social=sim-alice puppet
client 1 name=Bob social=sim-bob puppet
client 2 name=Carol social=sim-carol puppet
client 3 name=Dave social=sim-dave puppet
client 4 name=Eve social=sim-eve puppet
at 6 0 cmd team red
at 6 2 cmd team blue
at 8 1 cmd team red
at 8 3 cmd team blue
at 11 3 cmd inven
at 12 3 cmd invuse
at 14 3 cmd team blue
at 24 2 cmd score
at 27 4 cmd team red
at 29 1 disconnect
at 32 1 rejoin
at 37 1 cmd team red
at 40 0 cmd unlockteam
at 42 4 cmd team red
at 45 0 cmd lockteam
at 47 0 cmd teamkick Eve
at 50 4 cmd team red
at 54 0 disconnect
at 58 2 disconnect
at 58 3 disconnect
"""
    rulesets = rig.dir / "baseq2" / "rulesets"
    rulesets.mkdir(parents=True, exist_ok=True)
    (rulesets / "tdm.cfg").write_text("set match_ready 0\nset match_countdown 3\nset timelimit 5\n")
    startup = scenario_startup(rig, ["+set", "g_ruleset", "tdm", "+map", "q2dm1"])
    log = rig.run(startup, scenario_steps(rig, script), timeout=args.boot_timeout + 130)
    tap = read_jsonl(rig.dir / "tap.jsonl")
    detail = []
    humans = {r["name"]: r["slot"] for r in tap if r.get("type") == "connect" and not r.get("bot")}
    if not {"Alice", "Bob", "Carol", "Dave", "Eve"} <= set(humans):
        results.append((label, False, [f"humans connected: {sorted(humans)}"] + error_lines(log, rig)[:20]))
        return
    teams = [l.split("Teams: ", 1)[1].strip() for l in log.splitlines() if "Teams: " in l]
    # In this order (the captains of the two teams may come either way round).
    chain = ["RED locked", "RED unlocked by Alice", "RED locked by Alice", "Eve kicked from RED by Alice",
             "RED captain Bob", "BLUE unlocked (empty)"]
    for want in ("RED captain Alice", "BLUE captain Carol", "BLUE locked"):
        if want not in teams:
            detail.append(f"no 'Teams: {want}' (saw {teams})")
    at = 0
    for want in chain:
        if want not in teams[at:]:
            detail.append(f"no 'Teams: {want}' after {chain[:chain.index(want)]} (saw {teams})")
            break
        at = teams.index(want, at) + 1
    if "-> live" not in log:
        detail.append("the match never went live")
    locs = [r for r in tap if r.get("type") == "locprint"]
    refusals = [r for r in locs if r["base"] == "{} is locked.\n" and r["to"] != -1]
    eve = humans["Eve"]
    if [r["to"] for r in refusals] != [eve, eve]:
        detail.append(f"refusals to slots {[r['to'] for r in refusals]}, expected Eve's ({eve}) twice")
    joins = [r for r in locs if r["base"] == "$g_joined_team"]
    # Alice, Bob, Carol, Dave; Bob back after his drop; Eve after the unlock.
    if len(joins) != 6:
        detail.append(f"{len(joins)} team joins, expected 6")
    # The kick leaves Eve no menu: stock's join menu, with its cursor on a
    # team, took the next jump of a player kicked mid-fight as a join
    # (playtest 2026-10-30: kicked to the other team). Its open, in the
    # kick's frame, is closed again in that frame; an open menu redraws.
    # By frame: the open's layout can be stamped a millisecond after the kick.
    kicked = [r["frame"] for r in locs if r["base"] == "{} was removed from {}.\n"]
    menus = [r for r in tap if r.get("type") == "layout" and r["to"] == eve and kicked
             and kicked[0] < r["frame"] < kicked[0] + 160 and '"Join ' in r["text"]]
    if not kicked:
        detail.append("Eve was never kicked")
    elif menus:
        detail.append(f"Eve was shown the join menu {len(menus)} times after her kick")
    # A playing client who changes team: Dave to RED from the Tab menu (its
    # first choice for him), then back with `team blue`.
    dave = [r["args"][1] for r in locs if r["base"] == "$g_changed_team" and r["args"][0] == f"##P{humans['Dave']}"]
    if dave != ["RED", "BLUE"]:
        detail.append(f"Dave changed to {dave}, expected RED from the menu, then BLUE")
    # The scoreboard marks each team's captain with a `*` (playtest
    # 2026-10-29): Carol's, opened once live, has one per column.
    boards = [r["text"] for r in tap if r.get("type") == "layout" and r["to"] == humans["Carol"]
              and "ctf -40" in r["text"]]
    if not boards:
        detail.append("Carol was never shown the team scoreboard")
    else:
        marks = re.findall(r'xv (-?\d+) yv \d+ string2 "\*"', boards[0])
        if sorted(marks) != ["-64", "174"]:
            detail.append(f"captain marks at x {marks}, expected one per team (-64, 174)")
    detail += [f"name token inside an argument: {b}" for b in embedded_tokens(tap)[:5]]
    if rig.returncode != 0:
        detail.append(f"server exited with code {rig.returncode}")
    detail += error_lines(log, rig)[:20]
    results.append((label, not detail, detail))


def tier_human_readyvote(args, results):
    """The ready votes (playtest 2026-10-29) in a TDM ready-up warmup, four
    simulated players: Alice calls readyteam, which only RED votes on (Carol,
    on BLUE, is refused), and Bob's yes readies RED; then Carol calls
    readyall, BLUE and Alice say yes, everyone is ready and the match goes
    live. Between the two, Carol's team board marks RED's two rows ready (a
    green R) and nobody on BLUE. Dave suicides in the countdown and must be
    back in before LIVE (playtest 2026-10-29: the countdown's fire hold also
    held the respawn button). Q2RED only."""
    label = "19 human-readyvote"
    rig, why = scenario_rig(args, "t19-readyvote")
    if not rig:
        results.append((label, True, [f"skipped: {why}"]))
        return
    script = """seconds 62
client 0 name=Alice social=sim-alice puppet
client 1 name=Bob social=sim-bob puppet
client 2 name=Carol social=sim-carol puppet
client 3 name=Dave social=sim-dave puppet
at 6 0 cmd team red
at 6 1 cmd team red
at 6 2 cmd team blue
at 6 3 cmd team blue
at 12 0 cmd callvote readyteam
at 14 2 cmd vote yes
at 15 1 cmd vote yes
at 20 2 cmd score
at 22 2 cmd callvote readyall
at 24 3 cmd vote yes
at 25 0 cmd vote yes
at 30 3 cmd kill
at 36 3 cmd kill
at 42 3 cmd kill
"""
    rulesets = rig.dir / "baseq2" / "rulesets"
    rulesets.mkdir(parents=True, exist_ok=True)
    (rulesets / "tdm.cfg").write_text("set match_ready 1\nset match_countdown 20\nset timelimit 5\n")
    startup = scenario_startup(rig, ["+set", "g_auto_join", "1", "+set", "g_vote_cooldown", "0",
                                     "+set", "g_ruleset", "tdm", "+map", "q2dm1"])
    # Dave suicides every 6 s from 30 s (a `kill` within 5 s of a respawn is
    # ignored), so at least one lands in the 20 s countdown whatever CI's
    # timing; 3 s after the last, still before LIVE, he must be playing again.
    during = [("wait", r"Match: warmup -> countdown", 60), ("sleep", 17), ("cmd", "sv match")]
    log = rig.run(startup, scenario_steps(rig, script, during), timeout=args.boot_timeout + 110)
    tap = read_jsonl(rig.dir / "tap.jsonl")
    detail = []
    humans = {r["name"]: r["slot"] for r in tap if r.get("type") == "connect" and not r.get("bot")}
    if not {"Alice", "Bob", "Carol", "Dave"} <= set(humans):
        results.append((label, False, [f"humans connected: {sorted(humans)}"] + error_lines(log, rig)[:20]))
        return
    votes = [l.split("Vote: ", 1)[1].strip() for l in log.splitlines() if "Vote: " in l]
    want = ["Alice called ready red", "ready red passed", "ready red",
            "Carol called ready everyone", "ready everyone passed", "ready everyone"]
    if votes != want:
        detail.append(f"votes {votes}, expected {want}")
    locs = [r for r in tap if r.get("type") == "locprint"]
    if not any(r["to"] == humans["Carol"] and r["args"] == ["Only RED votes on readying RED."] for r in locs):
        detail.append("Carol (BLUE) wasn't refused a vote on RED's readyteam")
    ready = [r["args"][0] for r in locs if r["base"] == "{} is ready.\n" and r["to"] == -1]
    if ready != ["RED"]:
        detail.append(f"ready broadcasts {ready}, expected RED's alone (one line for the team)")
    # Carol's team board once RED is ready: an R beside each RED row
    # (x -56), none on BLUE's side (182).
    boards = [r["text"] for r in tap if r.get("type") == "layout" and r["to"] == humans["Carol"]
              and "ctf -40" in r["text"] and "string2 R " in r["text"]]
    marks = re.findall(r"xv (-?\d+) yv -?\d+ string2 R ", boards[0]) if boards else []
    if marks != ["-56", "-56"]:
        detail.append(f"ready marks at x {marks} on Carol's board, expected RED's two (-56)")
    countdown = log.find("Match: warmup -> countdown")
    dumps_at = [m.start() for m in re.finditer(r"Match: countdown\n", log)]
    if countdown < 0 or not re.search(r"Dave [^\n]*(suicide|kill)", log[countdown:]):
        detail.append("Dave didn't die in the countdown")
    elif not dumps_at:
        detail.append("no `sv match` during the countdown (it went live too soon?)")
    elif re.search(r"^\S*\s+-?\d+ Dave \(dead\)$", log[dumps_at[-1]:dumps_at[-1] + 400], re.M):
        detail.append("Dave was still dead late in the countdown: he couldn't respawn")
    live = log.find("-> live")
    if live < 0:
        detail.append("the match never went live")
    elif log.find("Vote: ready everyone\n") < 0 or log.find("Vote: ready everyone\n") > live:
        detail.append("the match went live before readyall passed")
    if rig.returncode != 0:
        detail.append(f"server exited with code {rig.returncode}")
    detail += error_lines(log, rig)[:20]
    results.append((label, not detail, detail))


def tier_human_shuffle(args, results):
    """The shuffle vote (2026-10-29: a random split, warmup only) in a TDM
    ready-up warmup, five simulated players, three RED and two BLUE. The
    server locks RED (`sv lockteam`: who captains it depends on arrival
    order); Dave's shuffle passes on Alice's and Bob's yes. The new teams are three and two, not the old groups, everyone's
    team skin follows their new team, and RED is unlocked. Then readyall
    starts the match and a shuffle called live is refused. Q2RED only."""
    label = "20 human-shuffle"
    rig, why = scenario_rig(args, "t20-shuffle")
    if not rig:
        results.append((label, True, [f"skipped: {why}"]))
        return
    script = """seconds 44
client 0 name=Alice social=sim-alice puppet
client 1 name=Bob social=sim-bob puppet
client 2 name=Carol social=sim-carol puppet
client 3 name=Dave social=sim-dave puppet
client 4 name=Eve social=sim-eve puppet
at 6 0 cmd team red
at 7 1 cmd team red
at 7 2 cmd team red
at 7 3 cmd team blue
at 7 4 cmd team blue
at 12 3 cmd callvote shuffle
at 14 0 cmd vote yes
at 14 1 cmd vote yes
at 22 2 cmd callvote readyall
at 24 0 cmd vote yes
at 24 1 cmd vote yes
at 24 3 cmd vote yes
at 36 4 cmd callvote shuffle
"""
    rulesets = rig.dir / "baseq2" / "rulesets"
    rulesets.mkdir(parents=True, exist_ok=True)
    (rulesets / "tdm.cfg").write_text("set match_ready 1\nset match_countdown 3\nset timelimit 5\n")
    startup = scenario_startup(rig, ["+set", "g_auto_join", "1", "+set", "g_vote_cooldown", "0",
                                     "+set", "g_ruleset", "tdm", "+map", "q2dm1"])
    during = [("sleep", 10), ("cmd", "sv lockteam red"), ("wait", r"Teams: shuffled", 60), ("sleep", 1), ("cmd", "sv teams")]
    log = rig.run(startup, scenario_steps(rig, script, during), timeout=args.boot_timeout + 110)
    tap = read_jsonl(rig.dir / "tap.jsonl")
    detail = []
    humans = {r["name"]: r["slot"] for r in tap if r.get("type") == "connect" and not r.get("bot")}
    names = ["Alice", "Bob", "Carol", "Dave", "Eve"]
    if not set(names) <= set(humans):
        results.append((label, False, [f"humans connected: {sorted(humans)}"] + error_lines(log, rig)[:20]))
        return
    m = re.search(r"Teams: shuffled: RED: (.*); BLUE: (.*)", log)
    if not m:
        detail.append("no 'Teams: shuffled' line")
    else:
        red, blue = (set(g.split(", ")) for g in m.groups())
        if red | blue != set(names) or red & blue or sorted([len(red), len(blue)]) != [2, 3]:
            detail.append(f"shuffled to RED {sorted(red)}, BLUE {sorted(blue)}: not a 3/2 split of all five")
        if {frozenset(red), frozenset(blue)} == {frozenset(names[:3]), frozenset(names[3:])}:
            detail.append("the shuffle gave the same teams back")
        # Each player's skin configstring (CTFAssignSkin): their new team's.
        skin = {}
        for r in tap:
            sm = re.fullmatch(r"(.*)\\[^\\]*/ctf_(r|b)\\default", r.get("text", "")) \
                if r.get("type") == "configstring" else None
            if sm:
                who = next((n for n in names if sm.group(1) in (n, f"##P{humans[n]}")), None)
                if who:
                    skin[who] = "RED" if sm.group(2) == "r" else "BLUE"
        want = {n: "RED" if n in red else "BLUE" for n in names}
        if skin != want:
            detail.append(f"team skins {skin}, expected {want}")
    if "Teams: RED locked by the server" not in log.split("Teams: shuffled", 1)[0]:
        detail.append("RED wasn't locked before the shuffle")
    elif not re.search(r"RED: unlocked", log.split("Teams: shuffled", 1)[-1]):
        detail.append("RED was still locked after the shuffle (`sv teams`)")
    votes = [l.split("Vote: ", 1)[1].strip() for l in log.splitlines() if "Vote: " in l]
    want_votes = ["Dave called shuffle teams", "shuffle teams passed", "shuffle teams",
                  "Carol called ready everyone", "ready everyone passed", "ready everyone"]
    if votes != want_votes:
        detail.append(f"votes {votes}, expected {want_votes}")
    if "-> live" not in log:
        detail.append("the match never went live")
    locs = [r for r in tap if r.get("type") == "locprint"]
    if not any(r["to"] == humans["Eve"] and r["args"] == ["Teams can only be shuffled in warmup."] for r in locs):
        detail.append("Eve's shuffle in the live match wasn't refused")
    if not any(r["base"] == "Teams shuffled.\n" and r["to"] == -1 for r in locs):
        detail.append("no 'Teams shuffled.' broadcast")
    detail += [f"name token inside an argument: {b}" for b in embedded_tokens(tap)[:5]]
    if rig.returncode != 0:
        detail.append(f"server exited with code {rig.returncode}")
    detail += error_lines(log, rig)[:20]
    results.append((label, not detail, detail))


def sv_match_dumps(log):
    """Each `sv match` in the log: {name: playing}. A dump is a `Match:` line
    followed by indented lines ("  <score> <name>[ (not playing)]")."""
    dumps, cur = [], None
    for raw in log.splitlines():
        line = re.sub(r"^<\d+>", "", raw)
        if line.startswith("Match: "):
            cur = {}
            dumps.append(cur)
            continue
        m = re.match(r"^\s+-?\d+ (.+?)( \(not playing\))?$", line)
        if cur is not None and m:
            cur[m.group(1)] = not m.group(2)
        elif cur is not None and not line.startswith("  "):
            cur = None
    return [d for d in dumps if d]


def tier_human_join(args, results):
    """Arriving (g_auto_join 0, the default): a human new to the server
    arrives spectating with the Tab menu open, and joins with `join` or the
    menu; after a map change - `gamemap` and `map`, which reloads the module -
    the last level's players are back as they were and a watcher still
    watches. Duel: only those who ask are queued. TDM: back on their own
    team. Q2RED only."""
    cases = {
        "ffa": ("""seconds 70
client 0 name=Alice social=sim-alice puppet
client 1 name=Bob social=sim-bob puppet
client 2 name=Dave social=sim-dave puppet
at 6 0 cmd join
at 7 1 cmd invuse
at 8 2 cmd spectator 1
""", [("sleep", 14), ("cmd", "sv match"), ("cmd", "gamemap q2dm2"), ("sleep", 20), ("cmd", "sv match"),
      ("cmd", "map q2dm3"), ("sleep", 20), ("cmd", "sv match")]),
        "duel": ("""seconds 50
client 0 name=Alice social=sim-alice puppet
client 1 name=Bob social=sim-bob puppet
client 2 name=Dave social=sim-dave puppet
at 6 0 cmd join
at 7 1 cmd invuse
""", [("sleep", 14), ("cmd", "sv match"), ("cmd", "map q2dm1"), ("sleep", 20), ("cmd", "sv match")]),
        "tdm": ("""seconds 45
client 0 name=Alice social=sim-alice puppet
client 1 name=Bob social=sim-bob puppet
at 6 0 cmd team red
at 6 1 cmd team blue
""", [("sleep", 12), ("cmd", "map q2dm1"), ("sleep", 20), ("cmd", "sv teams")]),
    }
    for gt, (script, during) in cases.items():
        label = f"18 human-join {gt}"
        rig, why = scenario_rig(args, f"t18-{gt}")
        if not rig:
            results.append((label, True, [f"skipped: {why}"]))
            continue
        rulesets = rig.dir / "baseq2" / "rulesets"
        rulesets.mkdir(parents=True, exist_ok=True)
        (rulesets / f"{gt}.cfg").write_text("set match_ready 0\nset match_countdown 3\n")
        startup = scenario_startup(rig, ["+set", "g_ruleset", gt, "+map", "q2dm1"])
        log = rig.run(startup, scenario_steps(rig, script, during), timeout=args.boot_timeout + 150)
        tap = read_jsonl(rig.dir / "tap.jsonl")
        detail = []
        humans = {r["name"]: r["slot"] for r in tap if r.get("type") == "connect" and not r.get("bot")}
        if gt == "tdm":
            captains = [l.split("Teams: ", 1)[1].strip() for l in log.splitlines() if "Teams: " in l]
            for want in ("RED captain Alice", "BLUE captain Bob"):
                if captains.count(want) != 2:
                    detail.append(f"'{want}' {captains.count(want)} times, expected once a level (saw {captains})")
            # Back on their team without choosing again.
            rejoined = [r for r in tap if r.get("type") == "locprint" and r["base"] == "$g_joined_team"]
            if len(rejoined) != 2:
                detail.append(f"{len(rejoined)} team joins, expected 2 (the first level's only)")
        else:
            dumps = sv_match_dumps(log)
            want = {"Alice": True, "Bob": True, "Dave": False}
            expected = 3 if gt == "ffa" else 2
            if len(dumps) != expected:
                detail.append(f"{len(dumps)} `sv match` dumps, expected {expected}: {dumps}")
            for i, d in enumerate(dumps):
                if d != want:
                    detail.append(f"level {i + 1}: playing {d}, expected {want}")
            # The newcomer gets the menu on arriving, on every level.
            menus = {r.get("map") for r in tap if r.get("type") == "layout" and r.get("to") == humans.get("Dave")
                     and ("Join the Match" in r["text"] or "Join Queue" in r["text"])}
            levels = {"ffa": {"q2dm1", "q2dm2", "q2dm3"}, "duel": {"q2dm1"}}[gt]
            if not levels <= menus:
                detail.append(f"Dave's menu opened on {sorted(menus)}, expected on {sorted(levels)}")
            if gt == "duel" and "Bob joins the duel queue" not in log:
                detail.append("Bob didn't join the queue from the menu")
            # The KEX client sends a stuffed `spectator 1` straight back as a
            # command (playtest 2026-10-29: "invalid game command"): the
            # watcher is sent none, and the echo is answered with nothing.
            dave = humans.get("Dave")
            stuffed = [r["text"].strip() for r in tap if r.get("type") == "stufftext" and r.get("to") == dave
                       and "spectator" in r["text"]]
            if stuffed:
                detail.append(f"Dave (watching) was sent {stuffed}")
            said = [" ".join([r.get("base", r.get("text", ""))] + r.get("args", [])) for r in tap
                    if r.get("type") in ("locprint", "print") and r.get("to") == dave]
            if gt == "ffa" and any("invalid game command" in t for t in said):
                detail.append("Dave's `spectator 1` was answered with 'invalid game command'")
        detail += [f"name token inside an argument: {b}" for b in embedded_tokens(tap)[:5]]
        if rig.returncode != 0:
            detail.append(f"server exited with code {rig.returncode}")
        detail += error_lines(log, rig)[:20]
        results.append((label, not detail, detail))


def bsp_entities(retail, mapname):
    """The entity string of maps/<mapname>.bsp in retail's pak0."""
    data = (pathlib.Path(retail) / "baseq2" / "pak0.pak").read_bytes()
    _, offset, length = struct.unpack("<4sii", data[:12])
    for i in range(length // 64):
        name, start, size = struct.unpack("<56sii", data[offset + i * 64:offset + i * 64 + 64])
        if name.split(b"\0")[0].decode() == f"maps/{mapname}.bsp":
            bsp = data[start:start + size]
            ents_at, ents_len = struct.unpack("<ii", bsp[8:16])
            return bsp[ents_at:ents_at + ents_len].split(b"\0")[0].decode("latin1")
    raise RuntimeError(f"maps/{mapname}.bsp not in pak0")


# MuffMode's map entities (docs/design/entities.md), added to q2dm1's own in
# an override file: what each mode leaves out, and what it can't spawn.
# bg_local.h's CONFIG_CLASSIC_JUMPS (pinned by core/movement_register.cpp).
CONFIG_CLASSIC_JUMPS = 12106

ENTITY_ADDITIONS = """
{ "classname" "item_quad" "origin" "-80 432 676" "gametype" "duel" }
{ "classname" "item_health_mega" "origin" "-80 400 676" "gametype" "tournament" }
{ "classname" "item_armor_combat" "origin" "-80 464 676" "not_gametype" "duel" }
{ "classname" "item_adrenaline" "origin" "-112 432 676" "powerups_off" "1" }
{ "classname" "item_health" "origin" "-112 464 676" "notq2" "1" "notarena" "1" }
{ "classname" "ammo_slugs_small" "origin" "-48 432 676" }
{ "classname" "target_position" "targetname" "t_apex" "origin" "200 432 900" }
{ "classname" "trigger_push" "origin" "-48 464 660" "target" "t_apex" }
{ "classname" "target_remove_powerups" "targetname" "t_strip" "origin" "0 0 0" }
{ "classname" "trigger_multiple" "origin" "0 0 0" "mins" "-4096 -4096 -4096" "maxs" "4096 4096 4096" "target" "t_strip" }
{ "classname" "trigger_teleport" "model" "*1" "target" "t_apex" }
"""


def tier_entities(args, results):
    """A .ent override for q2dm1 with MuffMode's mode keys, ammo, jump pad and
    targets: it replaces the BSP's entities in deathmatch, each mode leaves
    out what its keys say (FFA: the Duel-only quad and mega, the
    no-powerups adrenaline, the not-Quake-II health; Duel: the combat armour
    in their place), nothing goes unknown, and `sv saveents` writes a
    starting file for a map without one but never over an existing one.
    A map-sized trigger fires target_remove_powerups at the bots (it once
    crashed on its first player: playtest 2026-10-30), and a
    trigger_teleport's brush is hidden (stock draws it)."""
    world, rest = bsp_entities(args.retail, "q2dm1").split("}", 1)
    text = world + '"author" "harness"\n"no_dm_spawnpads" "1"\n}' + rest + ENTITY_ADDITIONS
    for gametype, left_out in (("ffa", 4), ("duel", 3)):
        rig = RIGS[args.engine_kind](args, args.candidate, f"t22-{gametype}")
        game_dir = rig.dir / "baseq2" if args.engine_kind == "q2red" else rig.base / "baseq2"
        (game_dir / "maps").mkdir(parents=True, exist_ok=True)
        (game_dir / "maps" / "q2dm1.ent").write_text(text, encoding="latin1")
        startup = (["+set", "deathmatch", "1", "+set", "maxclients", "8", "+set", "g_gametype", gametype]
                   + rig.bot_args() + ["+map", "q2dm1"])
        # Q2REX's bots begin but may not announce it in a bot-only match.
        joined = ("wait", r"entered the game", 30) if rig.expects_frags else ("sleep", 10)
        steps = [rig.wait_ready(), joined, ("sleep", 5), ("cmd", "sv saveents"), ("sleep", 1)]
        if gametype == "ffa":
            steps += [("cmd", "gamemap q2dm2"), ("wait", rig.ready_re, 60, 2),
                      ("sleep", 3), ("cmd", "sv saveents"), ("wait", r"Entities written to", 20)]
        steps += [("sleep", 1), ("cmd", "quit")]
        log = rig.run(startup, steps, timeout=args.boot_timeout + 120)
        detail = []
        if not re.search(r"Entities: \S*maps/q2dm1\.ent\n", log):
            detail.append("no 'Entities: maps/q2dm1.ent' line: the override wasn't used")
        want = f"Entities: {left_out} left out of {gametype} by their keys"
        if want not in log:
            found = re.findall(r"Entities: \d+ left out.*", log)
            detail.append(f"expected '{want}', got {found}")
        if "Entities: 1 jump pads aimed" not in log:
            detail.append("the jump pad wasn't aimed at its target_position")
        # Stock draws its brush, trigger texture and all (playtest 2026-10-30).
        if "Entities: 1 teleport triggers hidden" not in log:
            detail.append("the trigger_teleport wasn't hidden")
        for unknown in re.findall(r".* is not a valid field|.*doesn't have a spawn function", log):
            if any(k in unknown for k in ("gametype", "powerups", "notq2", "notarena", "author", "no_dm_spawnpads",
                                          "ammo_slugs_small", "target_position", "target_remove_powerups")):
                detail.append(f"unknown to the module: {unknown.strip()}")
        if not re.search(r"q2dm1\.ent already exists", log):
            detail.append("sv saveents didn't refuse the existing q2dm1.ent")
        if gametype == "ffa":
            written = [p for p in rig.dir.rglob("q2dm2.ent")]
            if not written:
                detail.append("sv saveents wrote no q2dm2.ent")
            elif not written[0].read_text(errors="replace").lstrip().startswith("{"):
                detail.append(f"{written[0]} isn't an entity list")
            if len(re.findall(r"Entities: \S*\.ent\n", log)) != 1:
                detail.append("an override was used for q2dm2, which has none")
        if rig.returncode != 0:
            detail.append(f"server exited with code {rig.returncode}")
        if "==== ShutdownGame ====" not in log:
            detail.append("no clean ShutdownGame")
        detail += error_lines(log, rig)[:20]
        results.append((f"22 entities {gametype}", not detail, detail))


def tier_startup_ruleset(args, results):
    """server.cfg's starting mode on an empty server: with `set g_ruleset ffa`
    at startup, rulesets/ffa.cfg's settings (and a file it execs) are in
    force as the first map spawns, with nobody connecting. The engine holds
    what's queued while a map loads until a player arrives, and runs no
    frames before then, so Bloody Blastard came up after every restart with
    its first map spawned without them (2026-10-30). A spawn-time setting
    shows it: the file points the .ent overrides at a folder with q2dm1's."""
    rig = RIGS[args.engine_kind](args, args.candidate, "t23-startup")
    game_dir = rig.dir / "baseq2" if args.engine_kind == "q2red" else rig.base / "baseq2"
    (game_dir / "rulesets").mkdir(parents=True, exist_ok=True)
    (game_dir / "rulesets" / "ffa.cfg").write_text(
        'set fraglimit 17 // a; comment\nset g_entity_override_dir "t23ents"\nexec rulesets/t23more.cfg\n')
    (game_dir / "rulesets" / "t23more.cfg").write_text('set g_map_list "q2dm3 q2dm8"\n')
    (game_dir / "t23ents").mkdir(parents=True, exist_ok=True)
    (game_dir / "t23ents" / "q2dm1.ent").write_text(bsp_entities(args.retail, "q2dm1"), encoding="latin1")
    startup = ["+set", "deathmatch", "1", "+set", "maxclients", "8", "+set", "g_ruleset", "ffa", "+map", "q2dm1"]
    log = rig.run(startup, [rig.wait_ready(), ("sleep", 4), ("cmd", "fraglimit"), ("cmd", "g_map_list"),
                            ("sleep", 1), ("cmd", "quit")], timeout=args.boot_timeout + 60)
    detail = []
    if "Ruleset: starting ffa" not in log:
        detail.append("no 'Ruleset: starting ffa' line")
    if not re.search(r"Entities: \S*t23ents/q2dm1\.ent\n", log):
        detail.append("the first map didn't spawn from t23ents/q2dm1.ent: the file's settings came after it")
    if not re.search(r'"fraglimit" is "17"', log):
        detail.append(f"fraglimit isn't the file's 17: {re.findall(r'.fraglimit. is .*', log)}")
    if not re.search(r'"g_map_list" is "q2dm3 q2dm8"', log):
        detail.append("g_map_list isn't the exec'd file's")
    if rig.returncode != 0:
        detail.append(f"server exited with code {rig.returncode}")
    detail += error_lines(log, rig)[:20]
    results.append(("23 startup-ruleset", not detail, detail))

def tier_admin_commands(args, results):
    """The server's console commands for a match (docs/admin.md). TDM, four
    simulated players in a ready-up warmup: `sv readyteam red` readies RED
    alone (no countdown), `sv readyall` starts the match, `sv lockteam` with
    no team locks both, `sv spectate Bob` takes him off RED and its roster,
    so the lock keeps him out. Duel, three: `sv spectate` on a duellist in a
    match under way is no forfeit: back to warmup, the queued player steps
    up (Chris, 2026-10-31). Q2RED only."""
    cases = {
        "tdm": ("""seconds 62
client 0 name=Alice social=sim-alice puppet
client 1 name=Bob social=sim-bob puppet
client 2 name=Carol social=sim-carol puppet
client 3 name=Dave social=sim-dave puppet
at 6 0 cmd team red
at 6 1 cmd team red
at 6 2 cmd team blue
at 6 3 cmd team blue
at 44 1 cmd team red
""", "set match_ready 1\nset match_countdown 3\nset match_lock 0\nset timelimit 5\n",
            [("sleep", 14), ("cmd", "sv readyteam red"), ("sleep", 3), ("cmd", "sv readyall"),
             ("wait", r"Match: countdown -> live", 30), ("sleep", 1), ("cmd", "sv lockteam"), ("sleep", 1),
             ("cmd", "sv spectate Bob"), ("sleep", 1), ("cmd", "sv spectate Bob"), ("cmd", "sv spectate Nobody"),
             ("sleep", 26), ("cmd", "sv match"), ("cmd", "sv teams")]),
        "duel": ("""seconds 50
client 0 name=Alice social=sim-alice puppet
client 1 name=Bob social=sim-bob join=1 puppet
client 2 name=Carol social=sim-carol join=2 puppet
""", "set match_ready 0\nset match_countdown 3\nset timelimit 5\nset g_auto_join 1\n",
            [("wait", r"Match: countdown -> live", 60), ("sleep", 2), ("cmd", "sv match"), ("cmd", "sv spectate Alice"),
             ("sleep", 10), ("cmd", "sv match")]),
    }
    for gt, (script, cfg, during) in cases.items():
        label = f"24 admin-commands {gt}"
        rig, why = scenario_rig(args, f"t24-{gt}")
        if not rig:
            results.append((label, True, [f"skipped: {why}"]))
            continue
        rulesets = rig.dir / "baseq2" / "rulesets"
        rulesets.mkdir(parents=True, exist_ok=True)
        (rulesets / f"{gt}.cfg").write_text(cfg)
        startup = scenario_startup(rig, ["+set", "g_ruleset", gt, "+map", "q2dm1"])
        log = rig.run(startup, scenario_steps(rig, script, during), timeout=args.boot_timeout + 150)
        tap = read_jsonl(rig.dir / "tap.jsonl")
        detail = []
        humans = {r["name"]: r["slot"] for r in tap if r.get("type") == "connect" and not r.get("bot")}
        want = {"Alice", "Bob", "Carol", "Dave"} if gt == "tdm" else {"Alice", "Bob", "Carol"}
        if not want <= set(humans):
            results.append((label, False, [f"humans connected: {sorted(humans)}"] + error_lines(log, rig)[:20]))
            continue
        locs = [r for r in tap if r.get("type") == "locprint"]
        dumps = sv_match_dumps(log)
        if gt == "tdm":
            at = lambda line: log.find(line)
            red, everyone = at("Match: RED readied by the server"), at("Match: everyone readied by the server")
            start = at("Match: warmup -> countdown")
            if red < 0 or everyone < 0:
                detail.append(f"readied lines: RED {red >= 0}, everyone {everyone >= 0}")
            elif not everyone < start:
                detail.append("the countdown started before `sv readyall` (RED alone isn't everyone)")
            if not any(r["base"] == "{} is ready.\n" and r["args"] == ["RED"] for r in locs) or \
                    not any(r["base"] == "Everyone is ready.\n" for r in locs):
                detail.append("no 'RED is ready.' / 'Everyone is ready.' broadcast")
            teams = [l.split("Teams: ", 1)[1].strip() for l in log.splitlines() if "Teams: " in l]
            for line in ("RED locked by the server", "BLUE locked by the server"):
                if line not in teams:
                    detail.append(f"no 'Teams: {line}' (saw {teams})")
            if log.count("Join: Bob moved to spectate by the server") != 1:
                detail.append("`sv spectate Bob` didn't log once")
            for line in ("Bob is already spectating.", "No one player called 'Nobody'."):
                if line not in log:
                    detail.append(f"no '{line}'")
            bob = humans["Bob"]
            removed = [r for r in locs if r["to"] == bob and r["base"].startswith("You were removed from {}.")]
            if not removed:
                detail.append("Bob wasn't told he was removed")
            elif not any(r["to"] == bob and r["base"] == "{} is locked.\n" and r["t"] > removed[0]["t"] for r in locs):
                detail.append("Bob's `team red` after his removal wasn't refused by the lock")
            final = dumps[-1] if dumps else {}
            if final.get("Bob") is not False or final.get("Alice") is not True:
                detail.append(f"the last `sv match`: {final}, expected Bob not playing, Alice playing")
            roster = re.search(r"^\S*RED: locked, captain .*\n((?:\s+.*\n)*)", log, re.M)
            if roster and "Bob" in roster.group(1):
                detail.append(f"Bob still on RED's list: {roster.group(1)!r}")
        else:
            # Without it the match played on, Carol taking over Alice's half (a broken build, 2026-10-31).
            removed = log.find("Join: Alice moved to spectate by the server")
            if "Match: ended" in log or "forfeit" in log:
                detail.append("the duellist's removal ended the match (a forfeit)")
            if "Match: back to warmup (a player removed)" not in log:
                detail.append("no 'Match: back to warmup (a player removed)'")
            if removed < 0 or "Match: warmup -> countdown" not in log[removed:]:
                detail.append("the match didn't start again from warmup after the removal")
            before, after = (dumps[0], dumps[-1]) if len(dumps) >= 2 else ({}, {})
            if before.get("Alice") is not True or before.get("Carol") is not False:
                detail.append(f"before: playing {before}, expected Alice duelling and Carol waiting")
            if after.get("Alice") is not False or after.get("Bob") is not True or after.get("Carol") is not True:
                detail.append(f"after: playing {after}, expected Alice spectating, Bob and Carol duelling")
        if rig.returncode != 0:
            detail.append(f"server exited with code {rig.returncode}")
        detail += error_lines(log, rig)[:20]
        results.append((label, not detail, detail))


def tier_human_timeout(args, results):
    """Match timeouts (decision 0025). Duel, two simulated players in a match
    under way: Alice's `time-out` stops the world - the scores and the HUD
    clock stand still, the status line counts the timeout down - while
    Bob's own call, his `time-in`, `kill` and `callvote` are refused; it
    runs out into the resume countdown and play goes on. Alice's second
    call is refused (one a match); Bob calls his and ends it early with
    `time-in`. Q2RED only."""
    label = "25 human-timeout duel"
    script = """seconds 72
client 0 name=Alice social=sim-alice puppet
client 1 name=Bob social=sim-bob join=1 puppet
at 34 0 cmd time-out
at 35 1 cmd time-out
at 36 1 cmd time-in
at 37 1 cmd kill
at 38 1 cmd callvote timelimit 15
at 56 0 cmd time-out
at 58 1 cmd time-out
at 60 1 cmd time-in
"""
    cfg = ("set match_ready 0\nset match_countdown 3\nset timelimit 5\nset g_auto_join 1\n"
           "set match_timeout 8\nset match_timeout_resume 3\n")
    rig, why = scenario_rig(args, "t25")
    if not rig:
        results.append((label, True, [f"skipped: {why}"]))
        return
    rulesets = rig.dir / "baseq2" / "rulesets"
    rulesets.mkdir(parents=True, exist_ok=True)
    (rulesets / "duel.cfg").write_text(cfg)
    during = [("wait", r"Timeout: called by Alice", 60), ("cmd", "sv match"), ("sleep", 4), ("cmd", "sv match"),
              ("wait", r"Timeout: over, play resumes", 30)]
    startup = scenario_startup(rig, ["+set", "g_ruleset", "duel", "+map", "q2dm1"])
    log = rig.run(startup, scenario_steps(rig, script, during), timeout=args.boot_timeout + 150)
    tap = read_jsonl(rig.dir / "tap.jsonl")
    detail = []
    humans = {r["name"]: r["slot"] for r in tap if r.get("type") == "connect" and not r.get("bot")}
    if not {"Alice", "Bob"} <= set(humans):
        results.append((label, False, [f"humans connected: {sorted(humans)}"] + error_lines(log, rig)[:20]))
        return
    alice, bob = humans["Alice"], humans["Bob"]
    locs = [r for r in tap if r.get("type") == "locprint"]
    told = lambda slot, text: any(r["to"] == slot and r["args"] == [text] for r in locs)
    for line in ("Timeout: called by Alice, 8 s", "Timeout: over, play resumes", "Timeout: called by Bob, 8 s",
                 "Timeout: ended early by Bob"):
        if line not in log:
            detail.append(f"no '{line}'")
    if log.count("Timeout: called by") != 2:
        detail.append(f"{log.count('Timeout: called by')} timeouts called, expected 2 (Alice's second refused)")
    for slot, who, text in ((bob, "Bob", "A timeout is already under way."),
                            (bob, "Bob", "Only the side that called the timeout can end it."),
                            (bob, "Bob", "Wait for the timeout to end."),
                            (alice, "Alice", "Your timeout for this match has been used.")):
        if not told(slot, text):
            detail.append(f"{who} wasn't told {text!r}")
    if not any(r["to"] == bob and r["base"] == "Not during a timeout.\n" for r in locs):
        detail.append("Bob's `kill` in the timeout wasn't refused")
    # The world stood still: the two `sv match` four seconds apart in the
    # timeout show the same scores, and the timeout under way.
    dumps = [m.group(1) for m in re.finditer(r"^(?:<\d+>)?Match: live\n((?:(?:<\d+>)?  .*\n)*)", log, re.M)]
    if len(dumps) < 2:
        detail.append(f"{len(dumps)} `sv match` while live, expected 2")
    else:
        scores = [re.findall(r"^(?:<\d+>)?\s+(-?\d+) (Alice|Bob)", d, re.M) for d in dumps[:2]]
        if scores[0] != scores[1]:
            detail.append(f"the scores moved in the timeout: {scores[0]} -> {scores[1]}")
        if not all("timeout: TIMEOUT " in d for d in dumps[:2]):
            detail.append(f"`sv match` didn't show the timeout: {dumps[:2]}")
        # ... and nobody moved (a broken build that let them: 2026-11-01).
        spots = [re.findall(r"^(?:<\d+>)?\s+(Alice|Bob) at (-?\d+ -?\d+ -?\d+)$", d, re.M) for d in dumps[:2]]
        if len(spots[0]) != 2 or spots[0] != spots[1]:
            detail.append(f"players moved in the timeout: {spots[0]} -> {spots[1]}")
    drawn, texts = state_line(tap)
    for want in ("TIMEOUT 0:08", "TIMEOUT 0:01", "RESUMING"):
        if want not in texts:
            detail.append(f"the status line never read {want!r}")
    # The clock stands still from the call to the end of the resume countdown.
    cs = [r for r in tap if r.get("type") == "configstring"]
    status = next((r["index"] for r in cs if r["text"].startswith(("WARMUP", "READY UP ", "WAITING FOR "))), None)
    if status is not None:
        # Alice's: from TIMEOUT 0:08 until the status line clears again.
        seq = [(r["index"], r["text"]) for r in cs if r["index"] in (status, status + 1)]
        start = next((i for i, (_, t) in enumerate(seq) if t == "TIMEOUT 0:08"), None)
        if start is None:
            detail.append("no TIMEOUT 0:08 on the status line")
        else:
            ticking = []
            for index, text in seq[start + 1:]:
                if index == status and not text.startswith(("TIMEOUT ", "RESUMING")):
                    break
                if index == status + 1:
                    ticking.append(text)
            if ticking:
                detail.append(f"the clock moved in the timeout: {ticking[:5]}")
    centre = [r["args"][0] for r in locs if r["base"] == "{}" and r["to"] == alice and r["args"]]
    if "Match resumes" not in centre or centre.count("FIGHT!") < 2:
        detail.append(f"Alice's centre prints: {centre[-8:]}, expected 'Match resumes' and FIGHT! again")
    if rig.returncode != 0:
        detail.append(f"server exited with code {rig.returncode}")
    detail += error_lines(log, rig)[:20]
    results.append((label, not detail, detail))

def tier_human_loc(args, results):
    """Location callouts (`loc`, decision 0027). TDM with a q2dm1 .loc file
    of one place and five simulated players: a callout reaches the caller's
    team only, as team chat, the name a whole argument; MuffMode's status
    bind expands; text with no token is refused; a spectator's reaches the
    spectators; a sixth callout in a second is flooded. Then FFA with no
    file: `g_loc 2` names the nearest item to everyone, and `g_loc 0` turns
    it off. Q2RED only."""
    place = "Harness Hall"
    loc_file = f"// tier 27\n0 0 0 {place}\n"
    # TDM: Alice and Bob red, Carol and Dave blue, Eve watching.
    label = "27 human-loc tdm"
    rig, why = scenario_rig(args, "t27-tdm")
    if not rig:
        results.append((label, True, [f"skipped: {why}"]))
        results.append(("27 human-loc ffa", True, [f"skipped: {why}"]))
        return
    locs_dir = rig.dir / "baseq2" / "locs"
    locs_dir.mkdir(parents=True, exist_ok=True)
    (locs_dir / "q2dm1.loc").write_text(loc_file)
    script = """seconds 40
client 0 name=Alice social=sim-alice puppet
client 1 name=Bob social=sim-bob puppet
client 2 name=Carol social=sim-carol puppet
client 3 name=Dave social=sim-dave puppet
client 4 name=Eve social=sim-eve puppet
at 6 0 cmd team red
at 6 1 cmd team red
at 6 2 cmd team blue
at 6 3 cmd team blue
at 12 0 cmd loc ENEMY - %l
at 14 1 cmd loc STATUS %h %a %w %l
at 16 0 cmd loc hello
at 18 4 cmd loc SPEC %l
at 22 2 cmd loc F1 %l
at 22.1 2 cmd loc F2 %l
at 22.2 2 cmd loc F3 %l
at 22.3 2 cmd loc F4 %l
at 22.4 2 cmd loc F5 %l
at 22.5 2 cmd loc F6 %l
"""
    startup = scenario_startup(rig, ["+set", "g_gametype", "tdm", "+set", "g_loc", "1", "+map", "q2dm1"])
    log = rig.run(startup, scenario_steps(rig, script), timeout=args.boot_timeout + 120)
    tap = read_jsonl(rig.dir / "tap.jsonl")
    detail = []
    humans = {r["name"]: r["slot"] for r in tap if r.get("type") == "connect" and not r.get("bot")}
    if not {"Alice", "Bob", "Carol", "Dave", "Eve"} <= set(humans):
        results.append((label, False, [f"humans connected: {sorted(humans)}"] + error_lines(log, rig)[:20]))
    else:
        if not re.search(r"Loc: 1 places from \S*locs/q2dm1\.loc\n", log):
            detail.append("no 'Loc: 1 places from locs/q2dm1.loc': the file wasn't read")
        locs = [r for r in tap if r.get("type") == "locprint"]
        team = [r for r in locs if r["base"] == "({}): {}\n"]
        heard = lambda who, text: sorted(n for n, s in humans.items()
                                         if any(r["to"] == s and r["args"] == [f"##P{humans[who]}", text] for r in team))
        for who, text, want in (("Alice", f"ENEMY - [{place}]", ["Alice", "Bob"]),
                                ("Eve", f"SPEC [{place}]", ["Eve"])):
            if heard(who, text) != want:
                detail.append(f"{who}'s {text!r} reached {heard(who, text)}, expected {want}")
        status = [r["args"][1] for r in team if r["args"][0] == f"##P{humans['Bob']}" and r["to"] == humans["Alice"]]
        # %w is a short name ("GRENADES:15"), never a $loc key inside the text.
        if not any(re.fullmatch(rf"STATUS H:\d+ A:\d+ [A-Z0-9]+(:\d+)? \[{place}\]", t) for t in status):
            detail.append(f"Bob's status callout: {status}")
        if not any(r["to"] == humans["Alice"] and r["base"].startswith("Usage: loc") for r in locs):
            detail.append("Alice's `loc hello` wasn't refused")
        if any(r["args"][1] == "hello" for r in team if len(r.get("args", [])) > 1):
            detail.append("`loc hello` was sent")
        # Two callouts from Alice (ENEMY) and none more: the sim client runs any
        # "cmd ..." it is printed, so the refusal must not print one.
        alice = [r for r in team if r["args"][0] == f"##P{humans['Alice']}"]
        if len(alice) != 2:
            detail.append(f"Alice's callouts delivered {len(alice)} times, expected 2: {[r['args'][1] for r in alice]}")
        flood = [r["args"][1] for r in team if r["to"] == humans["Dave"] and r["args"][0] == f"##P{humans['Carol']}"]
        if [t.split()[0] for t in flood] != ["F1", "F2", "F3", "F4"]:
            detail.append(f"Dave heard Carol's {flood}, expected F1-F4 (flood_msgs 4)")
        if not any(r["to"] == humans["Carol"] and r["base"] == "$g_flood_cant_talk" for r in locs):
            detail.append("Carol wasn't told she was flooding")
        if f"Loc: (Alice): ENEMY - [{place}]" not in log:
            detail.append("no console line for Alice's callout")
        detail += [f"name token inside an argument: {b}" for b in embedded_tokens(tap)[:5]]
        if rig.returncode != 0:
            detail.append(f"server exited with code {rig.returncode}")
        detail += error_lines(log, rig)[:20]
        results.append((label, not detail, detail))

    # FFA, no .loc file: the nearest item, to everyone; then switched off.
    label = "27 human-loc ffa"
    rig = RIGS["q2red"](args, args.candidate, "t27-ffa")
    script = """seconds 30
client 0 name=Alice social=sim-alice puppet
client 1 name=Bob social=sim-bob puppet
client 2 name=Carol social=sim-carol puppet
at 10 0 cmd loc at %l
at 22 1 cmd loc at %l
"""
    during = [("wait", r"Loc: Alice: at \[", 40), ("cmd", "set g_loc 0")]
    startup = scenario_startup(rig, ["+set", "g_gametype", "ffa", "+set", "g_auto_join", "1", "+set", "g_loc", "2",
                                     "+map", "q2dm1"])
    log = rig.run(startup, scenario_steps(rig, script, during), timeout=args.boot_timeout + 100)
    tap = read_jsonl(rig.dir / "tap.jsonl")
    detail = []
    humans = {r["name"]: r["slot"] for r in tap if r.get("type") == "connect" and not r.get("bot")}
    if not {"Alice", "Bob", "Carol"} <= set(humans):
        results.append((label, False, [f"humans connected: {sorted(humans)}"] + error_lines(log, rig)[:20]))
        return
    locs = [r for r in tap if r.get("type") == "locprint"]
    said = [r for r in locs if r["base"] == "{}: {}\n" and r["args"][0] == f"##P{humans['Alice']}"]
    to = sorted(n for n, s in humans.items() if any(r["to"] == s for r in said))
    if to != ["Alice", "Bob", "Carol"]:
        detail.append(f"Alice's callout reached {to}, expected everyone")
    if not all(re.fullmatch(r"at \[.+\]", r["args"][1]) for r in said):
        detail.append(f"Alice's callout: {[r['args'][1] for r in said]}")
    if "Loc: 0 places" in log or re.search(r"Loc: \d+ places from", log):
        detail.append("a .loc file was read for a map without one")
    if any(r["base"] == "{}: {}\n" and r["args"][0] == f"##P{humans['Bob']}" for r in locs):
        detail.append("Bob's callout went out with g_loc 0")
    if not any(r["to"] == humans["Bob"] and r["base"] == "Location callouts are off on this server.\n" for r in locs):
        detail.append("Bob wasn't told callouts are off")
    if rig.returncode != 0:
        detail.append(f"server exited with code {rig.returncode}")
    detail += error_lines(log, rig)[:20]
    results.append((label, not detail, detail))


TIERS = (tier_save_diff, tier_load_stock, tier_dm_smoke, tier_gametype_boot, tier_match_flow, tier_duel,
         tier_teams, tier_warmup, tier_duel_rotation, tier_human_duel, tier_human_teams,
         tier_human_vote, tier_maps, tier_human_settings, tier_human_teamlock,
         tier_human_join, tier_human_readyvote, tier_human_shuffle, tier_entities, tier_startup_ruleset,
         tier_admin_commands, tier_human_timeout, tier_human_loc)


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
    root = install_root()
    local_stock = root / "stock-reference" / p.get("STOCK_REFERENCE_COMMIT", "")[:8] / "game_x64.so"
    ap.add_argument("--stock", default=str(local_stock) if local_stock.exists() else None,
                    required=not local_stock.exists(),
                    help="stock reference module (.so for q2red, .dll for q2rex); default: the local kit's")
    ap.add_argument("--candidate", help="module under test (test only)")
    ap.add_argument("--engine", default=str(root / "engines" / f"q2red-{p.get('Q2RED_COMMIT')}"))
    ap.add_argument("--rex-engine", default=str(root / "engines" / f"q2rex-{p.get('Q2REX_VERSION')}"))
    ap.add_argument("--bots", default=str(root / "engines" / f"ozbot-{p.get('OZBOT_COMMIT')}"))
    ap.add_argument("--retail", default=str(root / "retail"))
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
    ap.add_argument("--only", choices=["save_diff", "load_stock", "dm_smoke", "gametype_boot", "match_flow", "duel", "teams", "warmup", "duel_rotation", "human_duel", "human_teams", "human_vote", "maps", "human_settings", "human_teamlock", "human_join", "human_readyvote", "human_shuffle", "entities", "startup_ruleset", "admin_commands", "human_timeout", "human_loc"])
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
