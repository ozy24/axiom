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
                 on the timelimit, counted from LIVE (Q2REX: warmup holds).

    q2harness.py test      --stock S --candidate C   # tiers 1-3
    q2harness.py calibrate --stock S --runs 6        # refresh the volatile list

Engines, retail data and bots default to the pinned installs under /srv/q2ci
(scripts/harness/pins.env). Each run gets a fresh rig directory, so runs never
share state.
"""

import argparse
import collections
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
        A server that exits early (a fatal error, say) ends the steps.
        Returns the console log text."""
        cmd, env, cwd = self.command(startup)
        log = open(self.log_path, "w")
        proc = subprocess.Popen(cmd, cwd=cwd, env=env, stdin=subprocess.PIPE,
                                stdout=log, stderr=subprocess.STDOUT, text=True)
        deadline = time.monotonic() + timeout
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
            # Whatever went wrong, never leave a server running.
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
    # Q2RED builds map paths in a 128-byte buffer from its own directory
    # ("Com_sprintf: overflow ... in 128", then "Can't find maps/x.bsp").
    max_path = 56

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
                   + rig.bot_args() + ["+map", "q2dm1"])
        log = rig.run(startup, [rig.wait_ready(), ("sleep", args.boot_seconds), ("cmd", "quit")],
                      timeout=args.boot_timeout + args.boot_seconds + 60)
        detail = []
        if not re.search(expect, log):
            detail.append(f"no line matching /{expect}/")
        if rig.returncode != 0:
            detail.append(f"server exited with code {rig.returncode}")
        if "==== ShutdownGame ====" not in log:
            detail.append("no clean ShutdownGame")
        detail += error_lines(log, rig)[:20]
        results.append((f"4 gametype-boot {label}", not detail, detail))


MATCH_SEQUENCE = ["Match: warmup", "Match: warmup -> countdown", "Match: countdown -> live",
                  "Match: live -> intermission", "Match: ended (timelimit)"]


def tier_match_flow(args, results):
    """Warmup on: bots (always ready) run warmup -> countdown -> live, and the
    timelimit, counted from LIVE, ends it (docs/design/match-flow.md)."""
    rig = RIGS[args.engine_kind](args, args.candidate, "t5-match")
    startup = (["+set", "deathmatch", "1", "+set", "maxclients", "8", "+set", "match_warmup", "1",
                "+set", "match_countdown", "3", "+set", "timelimit", "0.25"] + rig.bot_args() + ["+map", "q2dm1"])
    if rig.expects_frags:  # bots enter the game: the whole match runs
        steps = [rig.wait_ready(), ("wait", r"Match: ended", 90), ("sleep", 2), ("cmd", "quit")]
    else:  # Q2REX: bots never begin, so the match must hold in warmup, safely
        steps = [rig.wait_ready(), ("sleep", 20), ("cmd", "quit")]
    log = rig.run(startup, steps, timeout=args.boot_timeout + 150)
    lines = [l.split("Match: ", 1)[1].strip() for l in log.splitlines() if "Match: " in l]
    seen = ["Match: " + l for l in lines]
    detail = []
    want = MATCH_SEQUENCE if rig.expects_frags else MATCH_SEQUENCE[:1]
    if seen != want:
        detail.append(f"transitions {seen}, expected {want}")
    if rig.returncode != 0:
        detail.append(f"server exited with code {rig.returncode}")
    if "==== ShutdownGame ====" not in log:
        detail.append("no clean ShutdownGame")
    detail += error_lines(log, rig)[:20]
    results.append(("5 match-flow", not detail, detail))


def cmd_test(args):
    results = []
    for tier in (tier_save_diff, tier_load_stock, tier_dm_smoke, tier_gametype_boot, tier_match_flow):
        if args.only and tier.__name__ != f"tier_{args.only}":
            continue
        try:
            tier(args, results)
        except Exception as e:  # a crashed tier is a failed tier, not a crashed harness
            results.append((tier.__name__[5:], False, [str(e)]))
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
    ap.add_argument("--port", type=int, default=28010)
    ap.add_argument("--lobby-port", type=int, default=5150)
    ap.add_argument("--boot-timeout", type=float, default=None,
                    help="seconds to wait for the map to load (default 30 q2red, 120 q2rex)")
    ap.add_argument("--settle", type=float, default=6, help="seconds after the map loads before saving")
    ap.add_argument("--dm-seconds", type=float, default=60)
    ap.add_argument("--runs", type=int, default=6, help="calibration runs")
    ap.add_argument("--gametypes", default="ffa",
                    help="gametype ids tier 4 boots (the registry's, GAME_GAMETYPES)")
    ap.add_argument("--stock-gamerules", default="2,3",
                    help="Rogue's stock gamerules values tier 4 also boots (Tag, DeathBall)")
    ap.add_argument("--boot-seconds", type=float, default=20, help="tier 4 run time per gametype")
    ap.add_argument("--only", choices=["save_diff", "load_stock", "dm_smoke", "gametype_boot", "match_flow"])
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
