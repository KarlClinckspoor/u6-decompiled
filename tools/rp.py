#!/usr/bin/env python3
"""The replay DOS build's commands, one front end (docs/replay.md; PLAN-exact-port.md, Phase 1).

    python3 tools/rp.py build [--force]               build the replay DOS build (tools/rpbuild.py, through
                                                      Exhume's replay.py build); fails if DGROUP moved
    python3 tools/rp.py chargen [OUT] [--force]       the character: U.EXE run by [_chargen]'s keys, its OBJLIST
                                                      checked against objlist_sha1 (cached by its inputs)
    python3 tools/rp.py record OUT [--session NAME | --keys "KEYS"] [--feed N] [--wait S] [--pace S] [--timeout S]
                                                      record a session in headless DOSBox-X from that
                                                      character; F9, the stop key, is sent after the keys
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

The saved game is made by the real U.EXE from the user's game directory ([replay] data:
$U6_GAMEDATA or gamedata/), driven by [_chargen] of tests/replay/sessions.toml (tools/keyfeed.py,
`python3 tools/rp.py chargen [OUT]`), cached in build/dos/replay/stage/ and copied over the game in
every run. Sessions are key scripts in tests/replay/sessions.toml; one with `feed` is typed by
KEYFEED.COM at the game's own key polls (the same run every time), one without by AUTOTYPE.

Environment: U6_DOSBOX or EXHUME_DOSBOX_X (a DOSBox-X; default tools/Dosbox-X on Windows, else
dosbox-x on PATH), U6_GAMEDATA. Needs Python 3.11+, and Node.js for the replays.
"""
import argparse, os, shutil, subprocess, sys, tempfile, time, tomllib
sys.dont_write_bytecode = True   # no __pycache__ in exhume/tools

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
EXHUME_TOOLS = os.path.join(ROOT, 'exhume', 'tools')
CONFIG = os.path.join(ROOT, 'exhume.toml')
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


def session(name, private=False):
    s = tomllib.load(open(SESSIONS, 'rb'))
    if name not in s or (name.startswith('_') and not private):
        sys.exit('rp: no session %s in %s (there are: %s)' % (name, SESSIONS, ' '.join(n for n in s if not n.startswith('_'))))
    return s[name]


def game_dir(R, dst):
    """copies the user's game directory, less [replay] data_skip, to dst"""
    data = os.path.join(ROOT, R.data) if not os.path.isabs(R.data) else R.data
    if not os.path.isdir(data):
        sys.exit('rp: the game directory %s is missing (set U6_GAMEDATA)' % data)
    skip = {s.upper() for s in R.data_skip}
    shutil.copytree(data, dst, ignore=lambda d, names: [n for n in names if os.path.samefile(d, data) and n.upper() in skip])
    return data


def run_dosbox(g, conf, cmds, timeout, log=None):
    """runs DOSBox-X headless on conf (text) with the -c commands cmds, in g; seconds taken, or None on a timeout"""
    cf = os.path.join(os.path.dirname(g), 'run.conf')
    open(cf, 'w').write(conf + '\n')
    args = [dosbox_x(), '-defaultconf', '-conf', cf, '-silent', '-fastlaunch', '-nogui', '-nomenu'] + os.environ.get('RP_DOSBOX_ARGS', '').split()
    for c in cmds:
        args += ['-c', c]
    t0 = time.time()
    try:
        subprocess.run(args, cwd=g, env=dict(env(), SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy'),
                       stdout=log or subprocess.DEVNULL, stderr=subprocess.STDOUT if log else subprocess.DEVNULL,
                       timeout=timeout)
    except subprocess.TimeoutExpired:
        return None
    return time.time() - t0


def sha1(path):
    import hashlib
    return hashlib.sha1(open(path, 'rb').read()).hexdigest()


# GAME.EXE while U.EXE runs: an MZ header (2 paragraphs, SS:SP 2:100h, 20h paragraphs more), then
# push cs; pop ds; mov ah,3Ch; xor cx,cx; mov dx,16h; int 21h; mov bx,ax; mov ah,3Eh; int 21h;
# mov ax,4C00h; int 21h; 'STUB.TXT',0
STUB_EXE = bytes.fromhex('4d5a3f000100000002002000ffff020000010000000000001c000000000000000e1fb43c31c9'
                         'ba1600cd2189c3b43ecd21b8004ccd21535455422e54585400')


def chargen(force=False, out=None, check=True):
    """the saved game every session starts from: U.EXE's character creation, run in DOSBox-X from a
    fresh copy of the game with the key script [_chargen] of tests/replay/sessions.toml, typed by
    tools/keyfeed.py's KEYFEED.COM (counted in the program's keyboard polls, with a fixed date and
    time, so that every run makes the same character). Returns the stage directory holding
    SAVEGAME/OBJLIST. Cached in build/dos/replay/stage by a sha1 over its inputs; fails if the
    OBJLIST's sha1 is not the script's objlist_sha1 (unless check is False)"""
    import hashlib, keyfeed
    R = rc()
    s = session('_chargen', private=True)
    d = [int(x) for x in s['date'].split('-')]
    import datetime
    dow = (datetime.date(*d).weekday() + 1) % 7                      # DOS: 0 is Sunday
    clock = tuple(int(x) for x in s['time'].split(':'))
    com = keyfeed.code(keyfeed.parse_keys(s['keys'], s.get('gap', 30)), (d[0], d[1], d[2], dow), clock)[1]
    data = os.path.join(ROOT, R.data) if not os.path.isabs(R.data) else R.data
    uexe = os.path.join(data, 'U.EXE')
    if not os.path.exists(uexe):
        sys.exit('rp: no U.EXE in the game directory %s' % data)
    conf = open(R.dosbox_conf, encoding='latin-1').read() if R.dosbox_conf else '[cpu]\n'
    h = hashlib.sha1()
    for part in (com, open(uexe, 'rb').read(), open(os.path.join(data, 'SAVEGAME', 'OBJLIST'), 'rb').read(),
                 conf.encode('latin-1'), open(dosbox_x(), 'rb').read(64 << 10)):
        h.update(hashlib.sha1(part).digest())
    key = h.hexdigest()
    stage = out or os.path.join(R.work, 'stage')
    objlist = os.path.join(stage, 'SAVEGAME', 'OBJLIST')
    stamp = os.path.abspath(stage) + '.sha1'      # beside it: the stage is copied into the game
    if not force and os.path.exists(objlist) and os.path.exists(stamp) and open(stamp).read().strip() == key:
        return stage
    if sha1(uexe) != s.get('u_sha1', sha1(uexe)):
        print('rp: warning: U.EXE is sha1 %s, not the %s the script was made with' % (sha1(uexe), s['u_sha1']))
    tmp = tempfile.mkdtemp(prefix='u6chr-', dir=tmp_base())
    try:
        g = os.path.join(tmp, 'c')
        game_dir(R, g)
        open(os.path.join(g, 'KEYFEED.COM'), 'wb').write(com)
        # U.EXE ends by running GAME.EXE (credits.c, StartGame: execl in a loop until it works):
        # here a 63-byte EXE of ours that creates STUB.TXT and exits, so that the batch goes on
        open(os.path.join(g, 'GAME.EXE'), 'wb').write(STUB_EXE)
        bat = ['KEYFEED.COM', 'U.EXE', 'echo done > DONE.TXT', '']
        open(os.path.join(g, 'CHARGEN.BAT'), 'w', newline='').write('\r\n'.join(bat))
        print('rp: character creation: U.EXE, %d keys from [_chargen]' % len(keyfeed.parse_keys(s['keys'])))
        secs = run_dosbox(g, conf, ['mount c "%s"' % g, 'c:', 'call CHARGEN.BAT', 'exit'], s.get('timeout', 300))
        names = {n.upper(): n for n in os.listdir(g)}
        if secs is None or 'DONE.TXT' not in names:
            sys.exit('rp: character creation did not finish (%s): the key script no longer fits U.EXE'
                     % ('timeout' if secs is None else 'U.EXE did not exit'))
        made = os.path.join(g, 'SAVEGAME', 'OBJLIST')
        got = sha1(made)
        os.makedirs(os.path.dirname(objlist), exist_ok=True)
        shutil.copy(made, objlist)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print('rp: character creation took %.0f s: OBJLIST %d bytes, sha1 %s' % (secs, os.path.getsize(objlist), got))
    if check and s.get('objlist_sha1') and got != s['objlist_sha1']:
        os.remove(objlist)
        sys.exit('rp: FAIL: character creation made OBJLIST sha1 %s, expected %s ([_chargen] objlist_sha1 in %s)'
                 % (got, s['objlist_sha1'], SESSIONS))
    open(stamp, 'w').write(key + '\n')
    return stage


def conf_set(conf, section, settings):
    """conf (a DOSBox-X configuration's text) with settings {key: value} in [section], replacing
    the section's own lines for those keys. Section-aware: a comment that mentions "[cpu]" is
    left alone (the str.replace this replaces put "cycles=max" into such a comment's line and
    dropped the real section's cycles, so a recording ran at DOSBox-X's default speed)"""
    out, cur, done = [], None, False
    for line in conf.splitlines():
        t = line.strip()
        if t.startswith('[') and t.endswith(']'):
            cur = t[1:-1].strip().lower()
            out.append(line)
            if cur == section and not done:
                out += ['%s=%s' % kv for kv in settings.items()]
                done = True
            continue
        if cur == section and not t.startswith(('#', ';')) and '=' in t and t.split('=', 1)[0].strip().lower() in settings:
            continue
        out.append(line)
    if not done:
        out += ['[%s]' % section] + ['%s=%s' % kv for kv in settings.items()]
    return '\n'.join(out) + '\n'


def record(out, keys, wait=25, pace=2, timeout=300, feed=None):
    """records KEYS then the stop key into OUT/RECORD.OUT and STATE.OUT (and the saved game).
    feed None: KEYS are AUTOTYPE's names, typed in real time, `wait` seconds after the start and
    `pace` apart. feed N: KEYS are tools/keyfeed.py's names, typed by KEYFEED.COM (mode bios,
    loaded high so that the program loads where it does in a replay) at the program's own key
    polls, N polls apart unless a key says KEY/GAP; the run is then the same on every host."""
    R = rc()
    if exhume('build'):
        return 1
    exe = os.path.join(R.work, R.exe_name)
    stop = (R.stop_key or 'F9').lower()
    keys = keys.split()
    if stop in [k.lower() for k in keys]:
        sys.exit('rp: the keys contain the stop key %s; it is sent after them' % stop)
    keys.append(stop)
    stage = chargen()
    out = os.path.abspath(out)
    os.makedirs(out, exist_ok=True)
    for f in R.out_files + ['keys.txt']:
        if os.path.exists(os.path.join(out, f)):
            os.remove(os.path.join(out, f))
    tmp = tempfile.mkdtemp(prefix='u6rec-', dir=tmp_base())
    try:
        g = os.path.join(tmp, 'c')
        game_dir(R, g)
        shutil.copy(exe, os.path.join(g, R.exe_name))
        shutil.copytree(stage, g, dirs_exist_ok=True)
        # the same batch as replaydos.mjs's but for the AUTOTYPE line: another start-up (an
        # autoexec, a redirection) changed the environment's size and moved the program
        # (and [replay] dos_env, then the replay build's switches, [replay] env, set as replaydos.mjs sets them)
        sets = ['set %s' % e for e in R.dos_env] + ['set %s=%s' % (k, os.environ[k]) for k in R.env if os.environ.get(k)]
        conf = open(R.dosbox_conf, encoding='latin-1').read() if R.dosbox_conf else '[cpu]\n'
        if feed is None:
            bat = sets + ['autotype -w %g -p %g %s' % (wait, pace, ' '.join(keys)), R.exe_name, 'echo done > DONE.TXT', '']
            # the replays' configuration with the CPU as fast as the host can run it and turbo off,
            # so that AUTOTYPE's keys come in real time (tests/replay/README.md). The CPU settings
            # do not move the program; DOSBox-X's other settings and the batch do
            conf = conf_set(conf, 'cpu', {'cycles': 'max', 'turbo': 'false'})
        else:
            import keyfeed
            com = keyfeed.code(keyfeed.parse_keys(' '.join(keys), feed, 'bios'), mode='bios')[1]
            open(os.path.join(g, 'KEYFEED.COM'), 'wb').write(com)
            # LH: in an upper memory block, so that conventional memory is as in a replay
            bat = sets + ['lh KEYFEED.COM', R.exe_name, 'echo done > DONE.TXT', '']
        open(os.path.join(g, 'REPLAYRN.BAT'), 'w', newline='').write('\r\n'.join(bat))
        cf = os.path.join(tmp, 'record.conf')
        open(cf, 'w').write(conf)
        cmds = ['mount c "%s"' % g, 'c:', 'call REPLAYRN.BAT', 'exit']
        args = [dosbox_x(), '-defaultconf', '-conf', cf, '-silent', '-fastlaunch', '-nogui', '-nomenu'] + os.environ.get('RP_DOSBOX_ARGS', '').split()
        for c in cmds:
            args += ['-c', c]
        if feed is None:
            print('rp: recording %d keys (%s), %gs wait, one every %gs' % (len(keys), ' '.join(keys), wait, pace))
        else:
            print('rp: recording %d keys typed by KEYFEED.COM, %d polls apart: %s' % (len(keys), feed, ' '.join(keys)))
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
        # the saved game the session left ([replay] saves), as replaydos.mjs copies it out of a replay
        for d in R.saves:
            if d.upper() in names and os.path.isdir(os.path.join(g, names[d.upper()])):
                if os.path.isdir(os.path.join(out, d)):
                    shutil.rmtree(os.path.join(out, d))
                os.makedirs(os.path.join(out, d))
                for f in os.listdir(os.path.join(g, names[d.upper()])):
                    shutil.copy(os.path.join(g, names[d.upper()], f), os.path.join(out, d, f.upper()))
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
    r = exhume('--stage', chargen(), 'dos', os.path.abspath(rec), out)
    if not os.path.exists(os.path.join(out, 'STATE.OUT')):
        print('rp: the replay wrote no STATE.OUT')
        return r or 1
    return r


def keys_of(a):
    """keys, wait, pace, feed: from --keys and the switches, or a session of sessions.toml (its
    feed, when it has one, says the keys are KEYFEED.COM's, that many polls apart)"""
    if a.keys:
        return a.keys, a.wait or 25, a.pace or 2, a.feed
    s = session(a.session or 'smoke')
    return s['keys'], a.wait or s.get('wait', 25), a.pace or s.get('pace', 2), a.feed or s.get('feed')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('build'); p.add_argument('--force', action='store_true')
    p = sub.add_parser('chargen'); p.add_argument('out', nargs='?', help='a stage directory (default build/dos/replay/stage, cached)')
    p.add_argument('--force', action='store_true'); p.add_argument('--no-check', action='store_true')
    for name in ('record', 'check'):
        p = sub.add_parser(name)
        p.add_argument('out')
        p.add_argument('--session', help='a key script of tests/replay/sessions.toml (default smoke)')
        p.add_argument('--keys', help='AUTOTYPE key names, space-separated (without the stop key)')
        p.add_argument('--wait', type=float); p.add_argument('--pace', type=float)
        p.add_argument('--feed', type=int, help="type the keys with KEYFEED.COM (tools/keyfeed.py's names), N key polls apart")
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
    if a.cmd == 'chargen':
        st = chargen(a.force or bool(a.out), a.out, not a.no_check)
        print('rp: %s' % os.path.join(st, 'SAVEGAME', 'OBJLIST'))
        return 0
    if a.cmd == 'record':
        k, w, pc, fd = keys_of(a)
        return record(a.out, k, w, pc, a.timeout, fd)
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
        k, w, pc, fd = keys_of(a)
        if record(os.path.join(out, 'rec'), k, w, pc, a.timeout, fd):
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
    if rc().saves:
        print('\n== the saved games ([replay] saves)')
        if not a.rec:
            res['saves: rec = replay 1'] = exhume('saves', os.path.join(out, 'rec'), os.path.join(out, 'rep1'))
        res['saves: replay 1 = 2'] = exhume('saves', os.path.join(out, 'rep1'), os.path.join(out, 'rep2'))
    print('\n== the screens of replay 2 (%s)' % os.path.join(out, 'png'))
    exhume('show', os.path.join(out, 'rep2'), os.path.join(out, 'png'))
    print()
    for k, v in res.items():
        print('rp check: %-22s %s' % (k, 'DIFFERENT' if v else 'identical' if k.startswith('saves') else
                                      'identical apart from the masks'))
    return 1 if any(res.values()) else 0


if __name__ == '__main__':
    sys.exit(main())
