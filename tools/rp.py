#!/usr/bin/env python3
"""The replay DOS build's commands, one front end (docs/replay.md; PLAN-exact-port.md, Phase 1).

    python3 tools/rp.py build [--force]               build the replay DOS build (tools/rpbuild.py, through
                                                      Exhume's replay.py build); fails if DGROUP moved
    python3 tools/rp.py record OUT [--session NAME | --keys "KEYS"] [--wait S] [--pace S] [--timeout S]
                                                      record a session in headless DOSBox-X from the staged
                                                      saved game; F9, the stop key, is sent after the keys
    python3 tools/rp.py replay REC OUT                replay REC (a RECORD.OUT) in DOSBox-X (Exhume's replay.py dos)
    python3 tools/rp.py compare A B [--cross]         compare two STATE.OUT files (or run directories) with
                                                      exhume.toml's masks (replay.py compare --same-build;
                                                      --cross for DOS against the port)
    python3 tools/rp.py show STATE [PNGDIR]           list a dump's checkpoints, write each one's screen as a PNG
    python3 tools/rp.py log REC                       what a recording holds
    python3 tools/rp.py check OUT [--session NAME | --keys "KEYS" | --rec REC]
                                                      record (or take REC), replay it twice, compare the
                                                      recording with the first replay and the two replays,
                                                      write the screens of the second replay's dumps

Everything but record goes to exhume/tools/replay.py with --config exhume.toml (its [replay]
section: the masks, the segments, the stop key, the build command, the DOSBox-X configuration).
Recording is ours: Exhume records in js-dos (dos-mcp), and here the keys are typed by DOSBox-X's
AUTOTYPE, in the same DOSBox-X configuration as the replays (tests/replay/dosbox-x.conf; cycles=max, turbo off)
so that the program loads at the same segment in both.

The saved game is tests/replay/stage/ (a character; tests/replay/README.md), copied over the
user's game directory ([replay] data: $U6_GAMEDATA or gamedata/) in every run. Sessions are key
scripts in tests/replay/sessions.toml.

Environment: U6_DOSBOX or EXHUME_DOSBOX_X (a DOSBox-X; default tools/Dosbox-X on Windows, else
dosbox-x on PATH), U6_GAMEDATA. Needs Python 3.11+, and Node.js for the replays.
"""
import argparse, os, shutil, subprocess, sys, tempfile, time, tomllib
sys.dont_write_bytecode = True   # no __pycache__ in exhume/tools

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
EXHUME_TOOLS = os.path.join(ROOT, 'exhume', 'tools')
CONFIG = os.path.join(ROOT, 'exhume.toml')
STAGE = os.path.join(ROOT, 'tests', 'replay', 'stage')
SESSIONS = os.path.join(ROOT, 'tests', 'replay', 'sessions.toml')
sys.path.insert(0, EXHUME_TOOLS)
sys.path.insert(0, HERE)


def rc():
    import config, portcfg
    return portcfg.replay(config.load(CONFIG))


def dosbox_x():
    """the DOSBox-X to run: EXHUME_DOSBOX_X, U6_DOSBOX, tools/Dosbox-X (Windows), dosbox-x on PATH"""
    for p in (os.environ.get('EXHUME_DOSBOX_X'), os.environ.get('U6_DOSBOX'),
              os.path.join(HERE, 'Dosbox-X', 'dosbox-x.exe') if os.name == 'nt' else None,
              shutil.which('dosbox-x'), '/snap/bin/dosbox-x'):
        if p and os.path.exists(p):
            return os.path.abspath(p)
    sys.exit('rp: no DOSBox-X (set U6_DOSBOX or EXHUME_DOSBOX_X)')


def tmp_base():
    """where staged game directories go: under the home folder off Windows, where a snap DOSBox-X
    can see it (snap's /tmp is private), as tools/gate.py does"""
    if os.name == 'nt':
        return None
    d = os.path.expanduser('~/tmp')
    os.makedirs(d, exist_ok=True)
    return d


def env():
    e = dict(os.environ, PYTHONDONTWRITEBYTECODE='1')
    e['EXHUME_DOSBOX_X'] = dosbox_x()          # replaydos.mjs finds DOSBox-X by this
    if tmp_base():
        e['TMPDIR'] = tmp_base()               # replaydos.mjs stages under os.tmpdir()
    return e


def exhume(*args):
    """exhume/tools/replay.py --config exhume.toml ARGS, from the repository's root"""
    cmd = [sys.executable, os.path.join(EXHUME_TOOLS, 'replay.py'), '--config', CONFIG] + [str(a) for a in args]
    return subprocess.run(cmd, cwd=ROOT, env=env()).returncode


def build(force=False):
    if force:
        stamp = os.path.join(rc().work, 'build.sha1')
        if os.path.exists(stamp):
            os.remove(stamp)
    return exhume('build')


def session(name):
    s = tomllib.load(open(SESSIONS, 'rb'))
    if name not in s:
        sys.exit('rp: no session %s in %s (there are: %s)' % (name, SESSIONS, ' '.join(s)))
    return s[name]


def record(out, keys, wait=25, pace=2, timeout=300):
    """records KEYS (AUTOTYPE's names) then the stop key into OUT/RECORD.OUT and STATE.OUT"""
    R = rc()
    if exhume('build'):
        return 1
    exe = os.path.join(R.work, R.exe_name)
    stop = (R.stop_key or 'F9').lower()
    keys = keys.split()
    if stop in [k.lower() for k in keys]:
        sys.exit('rp: the keys contain the stop key %s; it is sent after them' % stop)
    keys.append(stop)
    data = os.path.join(ROOT, R.data) if not os.path.isabs(R.data) else R.data
    if not os.path.isdir(data):
        sys.exit('rp: the game directory %s is missing (set U6_GAMEDATA)' % data)
    out = os.path.abspath(out)
    os.makedirs(out, exist_ok=True)
    for f in R.out_files + ['keys.txt']:
        if os.path.exists(os.path.join(out, f)):
            os.remove(os.path.join(out, f))
    tmp = tempfile.mkdtemp(prefix='u6rec-', dir=tmp_base())
    try:
        g = os.path.join(tmp, 'c')
        skip = {s.upper() for s in R.data_skip}
        shutil.copytree(data, g, ignore=lambda d, names: [n for n in names if os.path.samefile(d, data) and n.upper() in skip])
        shutil.copy(exe, os.path.join(g, R.exe_name))
        shutil.copytree(STAGE, g, dirs_exist_ok=True)
        # the same batch as replaydos.mjs's but for the AUTOTYPE line: another start-up (an
        # autoexec, a redirection) changed the environment's size and moved the program
        bat = ['autotype -w %g -p %g %s' % (wait, pace, ' '.join(keys)), R.exe_name, 'echo done > DONE.TXT', '']
        open(os.path.join(g, 'REPLAYRN.BAT'), 'w', newline='').write('\r\n'.join(bat))
        conf = open(R.dosbox_conf, encoding='latin-1').read() if R.dosbox_conf else '[cpu]\n'
        # the replays' configuration with the CPU as fast as the host can run it and turbo off,
        # so that AUTOTYPE's keys come in real time (tests/replay/README.md). The CPU settings
        # do not move the program; DOSBox-X's other settings and the batch do
        conf = '\n'.join(l for l in conf.splitlines() if not l.strip().startswith(('turbo', 'cycles')))
        conf = conf.replace('[cpu]', '[cpu]\ncycles=max\nturbo=false', 1)
        cf = os.path.join(tmp, 'record.conf')
        open(cf, 'w').write(conf + '\n')
        cmds = ['mount c "%s"' % g, 'c:', 'call REPLAYRN.BAT', 'exit']
        args = [dosbox_x(), '-defaultconf', '-conf', cf, '-silent', '-fastlaunch', '-nogui', '-nomenu']
        for c in cmds:
            args += ['-c', c]
        print('rp: recording %d keys (%s), %gs wait, one every %gs' % (len(keys), ' '.join(keys), wait, pace))
        t0 = time.time()
        try:
            subprocess.run(args, cwd=g, env=dict(env(), SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy'),
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=timeout)
        except subprocess.TimeoutExpired:
            print('rp: TIMEOUT after %d s: the game did not exit (killed)' % timeout)
        names = {n.upper(): n for n in os.listdir(g)}
        print('rp: DOSBox-X ran %.0f s; the game %s' % (time.time() - t0, 'exited' if 'DONE.TXT' in names else 'did NOT exit'))
        for f in R.out_files:
            if f in names:
                shutil.copy(os.path.join(g, names[f]), os.path.join(out, f))
                print('rp: %s %d bytes' % (f, os.path.getsize(os.path.join(out, f))))
        open(os.path.join(out, 'keys.txt'), 'w').write('wait %g pace %g\n%s\n' % (wait, pace, ' '.join(keys)))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    if not os.path.exists(os.path.join(out, 'RECORD.OUT')) or not os.path.getsize(os.path.join(out, 'RECORD.OUT')):
        print('rp: no RECORD.OUT: the recording did not reach the stop key')
        return 1
    exhume('log', os.path.join(out, 'RECORD.OUT'))
    return 0


def replay(rec, out):
    out = os.path.abspath(out)
    for f in ('STATE.OUT', 'TRACE.OUT'):
        if os.path.exists(os.path.join(out, f)):
            os.remove(os.path.join(out, f))
    r = exhume('--stage', STAGE, 'dos', os.path.abspath(rec), out)
    if not os.path.exists(os.path.join(out, 'STATE.OUT')):
        print('rp: the replay wrote no STATE.OUT')
        return r or 1
    return r


def keys_of(a):
    if a.keys:
        return a.keys, a.wait or 25, a.pace or 2
    s = session(a.session or 'smoke')
    return s['keys'], a.wait or s.get('wait', 25), a.pace or s.get('pace', 2)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('build'); p.add_argument('--force', action='store_true')
    for name in ('record', 'check'):
        p = sub.add_parser(name)
        p.add_argument('out')
        p.add_argument('--session', help='a key script of tests/replay/sessions.toml (default smoke)')
        p.add_argument('--keys', help='AUTOTYPE key names, space-separated (without the stop key)')
        p.add_argument('--wait', type=float); p.add_argument('--pace', type=float)
        p.add_argument('--timeout', type=float, default=300, help='seconds for the recording')
        if name == 'check':
            p.add_argument('--rec', help='a RECORD.OUT to replay instead of recording one')
    p = sub.add_parser('replay'); p.add_argument('rec'); p.add_argument('out')
    p = sub.add_parser('compare'); p.add_argument('a'); p.add_argument('b'); p.add_argument('--cross', action='store_true')
    p = sub.add_parser('show'); p.add_argument('state'); p.add_argument('pngdir', nargs='?')
    p = sub.add_parser('log'); p.add_argument('rec')
    a = ap.parse_args()
    sys.stdout.reconfigure(line_buffering=True)   # ours in order with replay.py's, into a log too
    if a.cmd == 'build':
        return build(a.force)
    if a.cmd == 'record':
        k, w, pc = keys_of(a)
        return record(a.out, k, w, pc, a.timeout)
    if a.cmd == 'replay':
        return replay(a.rec, a.out)
    if a.cmd == 'compare':
        return exhume('compare', a.a, a.b, *([] if a.cross else ['--same-build']))
    if a.cmd == 'show':
        return exhume('show', a.state, *([a.pngdir] if a.pngdir else []))
    if a.cmd == 'log':
        return exhume('log', a.rec)
    # check
    out = os.path.abspath(a.out)
    rec = a.rec
    if not rec:
        k, w, pc = keys_of(a)
        if record(os.path.join(out, 'rec'), k, w, pc, a.timeout):
            return 1
        rec = os.path.join(out, 'rec', 'RECORD.OUT')
    bad = 0
    for d in ('rep1', 'rep2'):
        print('\n== replay %s' % d)
        bad |= replay(rec, os.path.join(out, d)) != 0
    if bad:
        print('rp check: a replay failed')
        return 1
    res = {}
    if not a.rec:
        print('\n== the recording against the first replay')
        res['recording = replay 1'] = exhume('compare', os.path.join(out, 'rec'), os.path.join(out, 'rep1'), '--same-build')
    print('\n== the two replays')
    res['replay 1 = replay 2'] = exhume('compare', os.path.join(out, 'rep1'), os.path.join(out, 'rep2'), '--same-build')
    print('\n== the screens of replay 2 (%s)' % os.path.join(out, 'png'))
    exhume('show', os.path.join(out, 'rep2'), os.path.join(out, 'png'))
    print()
    for k, v in res.items():
        print('rp check: %-22s %s' % (k, 'identical apart from the masks' if v == 0 else 'DIFFERENT'))
    return 1 if any(res.values()) else 0


if __name__ == '__main__':
    sys.exit(main())
