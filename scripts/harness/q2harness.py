#!/usr/bin/env python3
"""Game-test harness: runs game modules headless on Q2RED and compares them.

Tiers (plan Phase 1b):
  1. save-diff   Load coop base1, save, and compare the module's save JSON
                 (game.ssv, base1.sav) between the stock reference and the
                 candidate. Structure must match exactly; values must match
                 except fields listed as timing-volatile by calibration.
  2. load-stock  A save made by the stock reference must load in the candidate
                 (docs/decisions/0007).
  3. dm-smoke    A deathmatch match with ozbot bots: no crash, frags happen,
                 clean shutdown, no new error lines.

    q2harness.py test      --stock S.so --candidate C.so   # tiers 1-3
    q2harness.py calibrate --stock S.so --runs 4           # refresh volatile list

Engine, retail data and bots come from --engine/--retail/--bots, defaulting to
the pinned installs under /srv/q2ci (scripts/harness/pins.env). Each run gets a
fresh rig directory, so runs never share state.
"""

import argparse
import collections
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
VOLATILE = ROOT / "docs" / "baselines" / "volatile-save-fields.txt"
ALLOWED = ROOT / "docs" / "baselines" / "allowed-differences.txt"
SAVE_FILES = ("game.ssv", "base1.sav")
MAX_RIG_PATH = 56  # leaves room for /baseq2/maps/<map>.bsp in Q2RED's 128-byte buffer


def pins():
    out = {}
    for line in (ROOT / "scripts" / "harness" / "pins.env").read_text().splitlines():
        m = re.match(r"\s*([A-Z0-9_]+)=(\S+)", line)
        if m:
            out[m.group(1)] = m.group(2)
    return out


class Rig:
    """A throwaway server directory: engine, retail data and one module."""

    def __init__(self, args, module, workdir, name):
        self.dir = pathlib.Path(tempfile.mkdtemp(prefix=f"{name}-", dir=workdir))
        # Q2RED builds map paths in a 128-byte buffer from its own directory
        # ("Com_sprintf: overflow ... in 128", then "Can't find maps/x.bsp").
        if len(str(self.dir)) > MAX_RIG_PATH:
            raise RuntimeError(f"rig path is {len(str(self.dir))} chars, over {MAX_RIG_PATH}; "
                               "use a shorter --workdir (default /tmp/q2h-*)")
        self.args = args
        base = self.dir / "baseq2"
        (self.dir / "userdata").mkdir()
        base.mkdir()
        engine = pathlib.Path(args.engine)
        retail = pathlib.Path(args.retail)
        # Copied, not symlinked: the engine resolves its own path and writes
        # CRASHLOG.TXT (and finds Q2Game.kpf) next to the real binary, so a
        # symlink would make test crashes overwrite files in the engine dir.
        for f in ("quake2ex_dedicated", "PlayFabMultiplayerWin_reconstruction.so"):
            shutil.copy2(engine / f, self.dir / f)
        (self.dir / "Q2Game.kpf").symlink_to(retail / "Q2Game.kpf")
        (base / "pak0.pak").symlink_to(retail / "baseq2" / "pak0.pak")
        shutil.copy2(module, base / "game_x64.so")
        if args.bots:
            bots = pathlib.Path(args.bots)
            shutil.copy2(bots / "ozbot_x64.so", base / "ozbot_x64.so")
            shutil.copy2(bots / "botnames.txt", base / "botnames.txt")
            shutil.copytree(bots / "nav", base / "nav")

    def run(self, startup, timeline, timeout):
        """Start the server with `startup` args, then send each (delay, command)
        in `timeline` over stdin. Returns the console log text."""
        a = self.args
        cmd = ["./quake2ex_dedicated", "-stdout", "+set", "dedicated", "1",
               "+set", "sv_playfab_signin", "0",
               "+set", "sv_lan_lobby_port", str(a.lobby_port),
               "+set", "port", str(a.port)] + startup
        env = dict(os.environ, COMPUTERNAME="q2harness", XDG_DATA_HOME=str(self.dir / "userdata"),
                   LD_LIBRARY_PATH=str(self.dir))
        log = open(self.dir / "console.log", "w")
        proc = subprocess.Popen(cmd, cwd=self.dir, env=env, stdin=subprocess.PIPE,
                                stdout=log, stderr=subprocess.STDOUT, text=True)
        try:
            self.exited_early = False
            for delay, line in timeline:
                time.sleep(delay)
                if proc.poll() is not None:
                    self.exited_early = True  # e.g. a fatal error on load
                    break
                try:
                    proc.stdin.write(line + "\n")
                    proc.stdin.flush()
                except BrokenPipeError:
                    self.exited_early = True
                    break
            try:
                proc.stdin.close()
            except BrokenPipeError:
                pass
            proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
            raise RuntimeError(f"server did not exit within {timeout}s ({self.dir})")
        finally:
            log.close()
        self.returncode = proc.returncode
        return (self.dir / "console.log").read_text(errors="replace")

    def save_dir(self, name):
        return self.dir / "baseq2" / "save" / name


COOP = ["+set", "coop", "1", "+set", "deathmatch", "0", "+set", "maxclients", "4", "+map", "base1"]


def coop_save(args, module, name):
    rig = Rig(args, module, args.workdir, name)
    log = rig.run(COOP, [(args.settle, "save coop_harness"), (4, "quit")], timeout=60)
    sd = rig.save_dir("coop_harness")
    if not all((sd / f).exists() for f in SAVE_FILES):
        raise RuntimeError(f"{name}: save files missing in {sd}\n{tail(log)}")
    return rig, {f: json.loads((sd / f).read_text()) for f in SAVE_FILES}


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


ERROR_RE = re.compile(r"(?i)\b(error|fatal|assert|segmentation|unknown (field|pointer))\b")


def error_lines(log):
    return [l for l in log.splitlines() if ERROR_RE.search(l)]


def tier_save_diff(args, results):
    volatile, allowed = read_list(VOLATILE), read_list(ALLOWED)
    _, stock = coop_save(args, args.stock, "t1-stock")
    _, cand = coop_save(args, args.candidate, "t1-candidate")
    problems = compare(stock, cand, volatile, allowed)
    results.append(("1 save-diff", not problems, problems[:40]))


def tier_load_stock(args, results):
    stock_rig, _ = coop_save(args, args.stock, "t2-stock")
    rig = Rig(args, args.candidate, args.workdir, "t2-candidate")
    shutil.copytree(stock_rig.save_dir("coop_harness"), rig.save_dir("coop_harness"))
    # developer 1 makes the engine print SV_ReadLevelFile(), the proof that the
    # level really came from the save rather than a fresh spawn. Coop slots
    # must be named coop_*: Q2RED reads a coop load back from coop_current.
    startup = COOP[:-2] + ["+set", "developer", "1"] + COOP[-2:]
    log = rig.run(startup, [(args.settle, "load coop_harness"), (6, "save coop_reloaded"), (4, "quit")],
                  timeout=60)
    after_load = log.split("Loading game...", 1)[-1] if "Loading game..." in log else ""
    detail = error_lines(log)[:20]
    if rig.exited_early:
        detail.append(f"the server exited during the load (code {rig.returncode})")
    if "SV_ReadLevelFile()" not in after_load:
        detail.append("the level was not read from the save (no SV_ReadLevelFile after the load)")
    if not (rig.save_dir("coop_reloaded") / "base1.sav").exists():
        detail.append("the reloaded game could not be saved again")
    results.append(("2 load-stock", not detail, detail))


def tier_dm_smoke(args, results):
    if not args.bots:
        results.append(("3 dm-smoke", False, ["no --bots directory (ozbot build) given"]))
        return
    rig = Rig(args, args.candidate, args.workdir, "t3-dm")
    startup = ["+set", "deathmatch", "1", "+set", "maxclients", "8", "+set", "timelimit", "0",
               "+set", "fraglimit", "0", "+set", "sv_bot_brain", "ozbot", "+set", "bot_count", "6",
               "+set", "bot_skill", "3", "+map", "q2dm1"]
    log = rig.run(startup, [(args.dm_seconds, "status"), (2, "quit")], timeout=args.dm_seconds + 40)
    frags = [l for l in log.splitlines() if re.search(r"\b(killed|was |by |fragged)\b", l)
             and "Nav" not in l]
    errs = error_lines(log)
    detail = []
    if not frags:
        detail.append("no frags in the console log")
    if rig.returncode != 0:
        detail.append(f"server exited with code {rig.returncode}")
    if "==== ShutdownGame ====" not in log:
        detail.append("no clean ShutdownGame")
    detail += errs[:20]
    detail.append(f"({len(frags)} obituary lines)")
    results.append(("3 dm-smoke", not detail[:-1], detail))


def cmd_test(args):
    results = []
    for tier in (tier_save_diff, tier_load_stock, tier_dm_smoke):
        if args.only and tier.__name__ != f"tier_{args.only}":
            continue
        try:
            tier(args, results)
        except Exception as e:  # a crashed tier is a failed tier, not a crashed harness
            results.append((tier.__name__[5:], False, [str(e)]))
    failed = 0
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
    fields = read_list(VOLATILE)
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
    VOLATILE.parent.mkdir(parents=True, exist_ok=True)
    VOLATILE.write_text(
        "# Save fields whose values differ between identical runs of the stock module\n"
        "# (timing noise: the save happens after a wall-clock delay, not on a fixed frame).\n"
        "# Generated by `q2harness.py calibrate`; tier 1 ignores their values, never\n"
        "# their presence. Only ever grows; review additions in the PR.\n"
        + "\n".join(sorted(fields)) + "\n")
    print(f"calibrate: {len(fields)} volatile fields ({len(fields - before)} new) from {args.runs} runs")
    for k, n in counts.most_common():
        print(f"  {n:4d}  {k}")
    return 0


def main():
    p = pins()
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=["test", "calibrate"])
    ap.add_argument("--stock", required=True, help="stock reference game_x64.so")
    ap.add_argument("--candidate", help="game_x64.so under test (test only)")
    ap.add_argument("--engine", default=f"/srv/q2ci/engines/q2red-{p.get('Q2RED_COMMIT')}")
    ap.add_argument("--bots", default=f"/srv/q2ci/engines/ozbot-{p.get('OZBOT_COMMIT')}")
    ap.add_argument("--retail", default="/srv/q2ci/retail")
    ap.add_argument("--workdir", default=None, help="where rigs are created (default: a temp dir)")
    ap.add_argument("--keep", action="store_true", help="keep rig directories for inspection")
    ap.add_argument("--port", type=int, default=27960 + 50)
    ap.add_argument("--lobby-port", type=int, default=5150)
    ap.add_argument("--settle", type=float, default=12, help="seconds before saving")
    ap.add_argument("--dm-seconds", type=float, default=60)
    ap.add_argument("--runs", type=int, default=4, help="calibration runs")
    ap.add_argument("--only", choices=["save_diff", "load_stock", "dm_smoke"])
    args = ap.parse_args()
    if args.command == "test" and not args.candidate:
        ap.error("test needs --candidate")
    if args.bots and not pathlib.Path(args.bots).is_dir():
        args.bots = None
    own_workdir = args.workdir is None
    args.workdir = args.workdir or tempfile.mkdtemp(prefix="q2h-", dir="/tmp")
    try:
        return cmd_test(args) if args.command == "test" else cmd_calibrate(args)
    finally:
        if own_workdir and not args.keep:
            shutil.rmtree(args.workdir, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
