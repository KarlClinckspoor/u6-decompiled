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
    python3 tools/rp.py port REC OUT [--variant V]    replay REC in the port (--dos-replay; V one of linux-gcc,
                                                      the default, linux-clang, win-gcc, win-clang: PORT_VARIANTS)
    python3 tools/rp.py port-build [--variant V ... | --all]
    python3 tools/rp.py port-check OUT [--rec REC] [--variant V ... | --all]
                                                      REC (default tests/replay/first.rec) in DOS and in each
                                                      port build, the dumps compared across the builds and the
                                                      saved games
    python3 tools/rp.py port-self OUT [--variant V ... | --all] [--keys SCRIPT]
                                                      the port against itself: a --record session replayed
                                                      (dumps and saves compared), and a black box session
                                                      replayed (saves compared)
    python3 tools/rp.py port-dgroup                   regenerate the port's DGROUP table and the masks from a
                                                      port build's symbols (port/tools/gen_dgroup.py)
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
`python3 tools/rp.py chargen [OUT]`), cached in build/dos/stage/ and copied over the game in
every run. Sessions are key scripts in tests/replay/sessions.toml; one with `feed` is typed by
KEYFEED.COM at the game's own key polls (the same run every time), one without by AUTOTYPE.

Environment: U6_DOSBOX or EXHUME_DOSBOX_X (a DOSBox-X; default tools/Dosbox-X on Windows, else
dosbox-x on PATH), U6_GAMEDATA. Needs Python 3.11+, and Node.js for the replays.
"""
import argparse, shlex, os, shutil, subprocess, sys, tempfile, time, tomllib
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
    SAVEGAME/OBJLIST. Cached in build/dos/stage by a sha1 over its inputs; fails if the
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
    # beside the replay build, not in it: rpbuild empties R.work when it builds again, which a
    # replay does after chargen() has returned (once seen as replaydos.mjs's cpSync failing)
    stage = out or os.path.join(os.path.dirname(os.path.abspath(R.work)), 'stage')
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


# ---- the port against DOS (PLAN-exact-port.md, Phase 2) ----

# The port's builds that rp.py makes and checks against DOS. Phase 2's exit asks for Linux and
# Windows, gcc and clang:
#   linux-gcc, linux-clang  CMake with gcc or clang. On Windows they are built and run in WSL, on
#                           WSL's own file system (~/u6port-replay, ~/u6port-replay-clang), which
#                           builds far faster than /mnt/c; elsewhere in build/port-linux-CC.
#   win-gcc, win-clang      native Windows builds with MSYS2 (U6_MSYS2, default C:\msys64):
#                           MINGW64's gcc and SDL2, or CLANG64's clang with MINGW64's SDL2
#                           (build/port-win-CC). They run with MINGW64's bin on PATH, for SDL2.dll.
# U6_PORT, if set, names an executable that is used as it is, run natively.
PORT_VARIANTS = ('linux-gcc', 'linux-clang', 'win-gcc', 'win-clang')
PORT_WSL_BUILD = {'linux-gcc': '~/u6port-replay', 'linux-clang': '~/u6port-replay-clang'}


def msys2(*p):
    return os.path.join(os.environ.get('U6_MSYS2', r'C:\msys64'), *p)


def wsl_path(p):
    """a Windows path as WSL sees it (C:/x/y is /mnt/c/x/y)"""
    p = os.path.abspath(p).replace('\\', '/')
    return '/mnt/' + p[0].lower() + p[2:] if len(p) > 1 and p[1] == ':' else p


def on_wsl_side(variant='linux-gcc'):
    """whether the port runs through WSL: a Linux build on Windows, unless U6_PORT names an executable"""
    return os.name == 'nt' and variant.startswith('linux') and not os.environ.get('U6_PORT')


def port_env(variant):
    """the environment a native run of the port needs: MSYS2's DLLs (SDL2.dll, the C runtime's) on
    PATH for a Windows build"""
    e = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    if variant.startswith('win'):
        dirs = [msys2('mingw64', 'bin')] + ([msys2('clang64', 'bin')] if variant == 'win-clang' else [])
        e['PATH'] = os.pathsep.join(dirs + [e.get('PATH', '')])
    return e


def port_build(variant='linux-gcc'):
    """builds the port (port/, CMake, Release) as VARIANT (PORT_VARIANTS). Returns the executable's
    path as the side that runs it sees it."""
    if os.environ.get('U6_PORT'):
        return os.environ['U6_PORT']
    if variant not in PORT_VARIANTS:
        sys.exit('rp: no port variant %s (there are: %s)' % (variant, ' '.join(PORT_VARIANTS)))
    cc = variant.split('-')[1]
    src = os.path.join(ROOT, 'port')
    if on_wsl_side(variant):
        b = PORT_WSL_BUILD[variant]
        cmd = ('cmake -S %s -B %s -DCMAKE_C_COMPILER=%s -DCMAKE_BUILD_TYPE=Release >/dev/null && cmake --build %s -j8 2>&1 | '
               'grep -E "error|undefined" ; test -x %s/u6' % (shlex.quote(wsl_path(src)), b, cc, b, b))
        if subprocess.run(['wsl', '-e', 'bash', '-lc', cmd]).returncode:
            sys.exit('rp: the port did not build (in WSL, %s)' % b)
        return b + '/u6'
    b = os.path.join(ROOT, 'build', 'port-' + variant)
    if variant.startswith('linux'):
        if os.name == 'nt':
            sys.exit('rp: %s is built in WSL (unset U6_PORT)' % variant)
        cfg = ['cmake', '-S', src, '-B', b, '-DCMAKE_C_COMPILER=' + cc, '-DCMAKE_BUILD_TYPE=Release']
        bld, e = ['cmake', '--build', b, '-j8'], None
    else:
        if os.name != 'nt':
            sys.exit('rp: %s is built on Windows, with MSYS2' % variant)
        env_dir = 'clang64' if cc == 'clang' else 'ucrt64'      # where cmake and ninja are
        cmake = msys2(env_dir, 'bin', 'cmake.exe')
        cfg = [cmake, '-S', src, '-B', b, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
               '-DCMAKE_MAKE_PROGRAM=' + msys2(env_dir, 'bin', 'ninja.exe'),
               '-DCMAKE_C_COMPILER=' + (msys2('clang64', 'bin', 'clang.exe') if cc == 'clang' else msys2('mingw64', 'bin', 'gcc.exe')),
               '-DSDL2_DIR=' + msys2('mingw64', 'lib', 'cmake', 'SDL2').replace('\\', '/')]
        bld = [cmake, '--build', b, '-j8']
        # the compiler's own DLLs: MINGW64's for gcc, CLANG64's for clang
        e = dict(os.environ, PATH=os.pathsep.join([msys2('clang64' if cc == 'clang' else 'mingw64', 'bin'), os.environ.get('PATH', '')]))
    for cmd in (cfg, bld):
        r = subprocess.run(cmd, env=e, capture_output=True, text=True, errors='replace')
        if r.returncode:
            sys.exit('rp: the port did not build (%s):\n%s' % (b, (r.stdout + r.stderr)[-3000:]))
    return os.path.join(b, 'u6.exe' if variant.startswith('win') else 'u6')


def port_dgroup():
    """regenerates the port's DGROUP table (port/tools/gen_dgroup.py --masks: port/game/dgroup_port.c,
    tu_*.c, dgs_*.inc and exhume.toml's generated masks) from a port build's symbols, then builds
    the port again with it"""
    exe = port_build()
    nmf = os.path.join(rc().work, 'port.nm')
    os.makedirs(os.path.dirname(nmf), exist_ok=True)
    if on_wsl_side():
        r = subprocess.run(['wsl', '-e', 'bash', '-lc', 'nm -S --defined-only %s' % exe], capture_output=True, text=True)
    else:
        r = subprocess.run(['nm', '-S', '--defined-only', exe], capture_output=True, text=True)
    if r.returncode:
        sys.exit('rp: nm failed: %s' % r.stderr[-300:])
    open(nmf, 'w').write(r.stdout)
    if subprocess.run([sys.executable, os.path.join(ROOT, 'port', 'tools', 'gen_dgroup.py'), '--nm', nmf, '--masks']).returncode:
        return 1
    port_build()
    return 0


def port_replay(rec, out, exe=None, timeout=1800, variant='linux-gcc'):
    """replays REC (a RECORD.OUT of the replay DOS build) in the port (--dos-replay), from the
    character [_chargen] makes, as tools/replay.py's run_port does: a fresh home folder with the
    stage in it, the game's own directory as --data; STATE.OUT and the home's SAVEGAME are
    copied to OUT, the port's log to OUT/port.log"""
    R = rc()
    exe = exe or port_build(variant)
    stage = chargen()
    out = os.path.abspath(out)
    home = os.path.join(out, 'home')
    shutil.rmtree(home, ignore_errors=True)
    shutil.copytree(stage, home)
    for f in ('STATE.OUT', 'port.log'):
        if os.path.exists(os.path.join(out, f)):
            os.remove(os.path.join(out, f))
    shutil.rmtree(os.path.join(out, 'SAVEGAME'), ignore_errors=True)
    data = os.path.join(ROOT, R.data) if not os.path.isabs(R.data) else R.data
    args = ['--data', data, '--home', home, '--hidden', '--no-audio', '--dos-replay', os.path.abspath(rec)]
    print('rp: replaying %s in the port (%s)' % (rec, os.environ.get('U6_PORT') or variant))
    t0 = time.time()
    if on_wsl_side(variant):
        line = 'SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout %d %s %s' % (
            timeout, exe, ' '.join(shlex.quote(wsl_path(a)) if os.path.exists(a) else shlex.quote(a) for a in args))
        r = subprocess.run(['wsl', '-e', 'bash', '-lc', line], capture_output=True, text=True, errors='replace')
    else:
        try:
            r = subprocess.run([exe] + args, capture_output=True, text=True, errors='replace', timeout=timeout,
                               env=port_env(variant))
        except subprocess.TimeoutExpired:
            sys.exit('rp: the port did not finish within %d s' % timeout)
    open(os.path.join(out, 'port.log'), 'w').write(r.stdout + r.stderr)
    if os.path.exists(os.path.join(home, 'STATE.OUT')):
        shutil.copy(os.path.join(home, 'STATE.OUT'), os.path.join(out, 'STATE.OUT'))
    names = {n.upper(): n for n in os.listdir(home)}
    if 'SAVEGAME' in names:
        shutil.copytree(os.path.join(home, names['SAVEGAME']), os.path.join(out, 'SAVEGAME'))
    print('rp: the port exited with %d after %.0f s' % (r.returncode, time.time() - t0))
    if not os.path.exists(os.path.join(out, 'STATE.OUT')):
        print('rp: the port wrote no STATE.OUT (%s)' % os.path.join(out, 'port.log'))
        return r.returncode or 1
    return r.returncode


def port_run(exe, args, variant, timeout=600):
    """runs the port EXE with ARGS (paths turned into WSL's for a WSL build), no window, no sound;
    returns (exit code, output)"""
    if on_wsl_side(variant):
        line = 'SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout %d %s %s' % (
            timeout, exe, ' '.join(shlex.quote(wsl_path(a)) if os.path.isabs(a) else shlex.quote(a) for a in args))
        r = subprocess.run(['wsl', '-e', 'bash', '-lc', line], capture_output=True, text=True, errors='replace')
        return r.returncode, r.stdout + r.stderr
    try:
        r = subprocess.run([exe] + args, capture_output=True, text=True, errors='replace', timeout=timeout,
                           env=port_env(variant))
    except subprocess.TimeoutExpired:
        return -1, 'timed out after %d s' % timeout
    return r.returncode, r.stdout + r.stderr


# the port's own session for port-self: from the character [_chargen] makes, a few steps, a look
# north, passes, the game saved (ctrl+s, Y); times in ms from the start (each key also waits for
# the game to take the one before: portmain.c's --keys)
SELF_KEYS = ('3000:left 3400:down 3800:down 4200:right 4600:l 5000:up 5400:enter 6000:space '
             '6400:space 6800:space 7200:space 8000:^s 9000:Y')


def save_files(d):
    """{NAME: sha1} of the files in the SAVEGAME folder of d (matched without case)"""
    if not os.path.isdir(d):
        return {}
    sg = [n for n in os.listdir(d) if n.upper() == 'SAVEGAME']
    if not sg:
        return {}
    p = os.path.join(d, sg[0])
    return {n.upper(): sha1(os.path.join(p, n)) for n in os.listdir(p) if os.path.isfile(os.path.join(p, n))}


def same_saves(what, a, b):
    fa, fb = save_files(a), save_files(b)
    bad = sorted(n for n in set(fa) | set(fb) if fa.get(n) != fb.get(n))
    print('rp: saves (%s): %d files, %s' % (what, len(fa), 'all identical' if not bad and fa else
                                           'DIFFER: ' + ' '.join(bad) if bad else 'none'))
    return 1 if bad or not fa else 0


def port_self(out, variant='linux-gcc', keys=SELF_KEYS):
    """The port against itself, through Exhume's recorder (PLAN-exact-port.md, Phase 2 task 4):
    1. a session recorded with --record (the dumps too) from [_chargen]'s character, KEYS typed
       and then F9; replayed with --replay --out, and the two runs' dumps compared (replay.py
       compare --same-build) and their saved games;
    2. the same keys in a black box session (no --record, ended by --run-ms), replayed from its
       recordings/ folder with --out: the saved games compared (a black box has no dumps)."""
    out = os.path.abspath(out)
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    exe = port_build(variant)
    R = rc()
    data = os.path.join(ROOT, R.data) if not os.path.isabs(R.data) else R.data
    stage = chargen()
    common = ['--data', data, '--hidden', '--no-audio']
    res = {}

    def home(name):
        h = os.path.join(out, name)
        shutil.copytree(stage, h)
        return h

    def run(what, args, log):
        t0 = time.time()
        code, text = port_run(exe, common + args, variant)
        open(os.path.join(out, log), 'w').write(text)
        print('rp: %s: exit %d after %.0f s (%s)' % (what, code, time.time() - t0, log))
        return code

    print('== 1. --record, --replay --out (%s)' % variant)
    h1 = home('home-rec')
    run('record', ['--home', h1, '--game', '--record', os.path.join(out, 'rec'), '--keys', keys + ' 16000:f9',
                   '--run-ms', '60000'], 'rec.log')
    run('replay', ['--home', home('home-rep'), '--replay', os.path.join(out, 'rec'), '--out', os.path.join(out, 'rep')],
        'rep.log')
    res['recording = replay'] = exhume('compare', os.path.join(out, 'rec'), os.path.join(out, 'rep'), '--same-build')
    res['saves: recording = replay'] = same_saves('recording, replay', h1, os.path.join(out, 'rep'))

    print('\n== 2. the black box (%s)' % variant)
    h2 = home('home-bb')
    run('black box', ['--home', h2, '--game', '--keys', keys, '--run-ms', '16000'], 'bb.log')
    recs = sorted(os.listdir(os.path.join(h2, 'recordings'))) if os.path.isdir(os.path.join(h2, 'recordings')) else []
    if not recs:
        print('rp: the black box session left no recording')
        res['black box'] = 1
    else:
        bb = os.path.join(h2, 'recordings', recs[-1])
        run('black box replay', ['--home', home('home-bbrep'), '--replay', bb, '--out', os.path.join(out, 'bbrep')],
            'bbrep.log')
        res['saves: black box = replay'] = same_saves('black box, replay', h2, os.path.join(out, 'bbrep'))
    print()
    for k, v in res.items():
        print('rp port-self: %-28s %s' % (k, 'DIFFERENT' if v else 'identical'))
    return 1 if any(res.values()) else 0


def port_saves(dos, port):
    """the saved game the session left, DOS's against the port's. DOS's run holds the whole
    SAVEGAME (the game's copy, the stage over it, and what the session wrote); the port's home
    holds the stage and what the session wrote, and reads the rest from the game's directory.
    So every file DOS's run changed from the game's own (with the stage) must be in the port's,
    with the same bytes, and every file in the port's must be DOS's. Returns the number of
    differences."""
    R = rc()
    data = os.path.join(ROOT, R.data) if not os.path.isabs(R.data) else R.data
    def files(d):
        if not os.path.isdir(d):
            return {}
        return {n.upper(): os.path.join(d, n) for n in os.listdir(d) if os.path.isfile(os.path.join(d, n))}
    def ci(root, name):
        for n in os.listdir(root):
            if n.upper() == name:
                return os.path.join(root, n)
        return os.path.join(root, name)
    base = files(ci(data, 'SAVEGAME'))
    base.update(files(os.path.join(chargen(), 'SAVEGAME')))
    fd, fp = files(os.path.join(dos, 'SAVEGAME')), files(os.path.join(port, 'SAVEGAME'))
    bad = 0
    changed = [n for n in sorted(fd) if n not in base or sha1(fd[n]) != sha1(base[n])]
    for n in changed:
        if n not in fp:
            print('rp: saves: %s changed in DOS, not written by the port' % n); bad += 1
    for n in sorted(fp):
        if n not in fd:
            print('rp: saves: %s only in the port' % n); bad += 1
        elif sha1(fp[n]) != sha1(fd[n]):
            print('rp: saves: %s differs' % n); bad += 1
    print('rp: saves: %d files DOS changed, %d in the port\'s home, %s' % (
        len(changed), len(fp), 'all identical' if not bad else '%d differences' % bad))
    return bad


def port_check(out, rec, variants=('linux-gcc',)):
    """REC replayed in DOS (DOSBox-X) and in each of the port's builds VARIANTS, each one's dumps
    compared with DOS's across the builds (replay.py compare: the masks of both scopes,
    [replay.segments]) and its saved games with DOS's. One variant's run goes to OUT/port, more
    to OUT/port-VARIANT."""
    out = os.path.abspath(out)
    exes = [(v, port_build(v)) for v in variants]   # all built before anything runs
    if replay(rec, os.path.join(out, 'dos')):
        return 1
    res = {}
    for v, exe in exes:
        d = os.path.join(out, 'port' if len(exes) == 1 else 'port-' + v)
        if port_replay(rec, d, exe, variant=v):
            print('rp: the port failed (%s)' % os.path.join(d, 'port.log'))
        print('\n== DOS against the port (%s)' % v)
        r = exhume('compare', os.path.join(out, 'dos'), d)
        print('\n== the saved games (%s)' % v)
        b = port_saves(os.path.join(out, 'dos'), d)
        res[v] = (r, b)
    if len(exes) > 1:
        print('\n== summary')
        for v, (r, b) in res.items():
            print('  %-12s checkpoints %s, saves %s' % (v, 'identical' if not r else 'DIFFER', 'identical' if not b else 'DIFFER'))
    return 1 if any(r or b for r, b in res.values()) else 0


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
    p = sub.add_parser('chargen'); p.add_argument('out', nargs='?', help='a stage directory (default build/dos/stage, cached)')
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
    def variant_args(p, many):
        p.add_argument('--variant', action='append' if many else 'store', choices=PORT_VARIANTS,
                       help='the port build (default linux-gcc)' + ('; repeatable' if many else ''))
        if many:
            p.add_argument('--all', action='store_true', help='every variant: %s' % ', '.join(PORT_VARIANTS))
    p = sub.add_parser('port'); p.add_argument('rec'); p.add_argument('out'); variant_args(p, False)
    p = sub.add_parser('port-build'); variant_args(p, True)
    p = sub.add_parser('port-dgroup')
    p = sub.add_parser('port-check'); p.add_argument('out'); variant_args(p, True)
    p.add_argument('--rec', default=os.path.join(ROOT, 'tests', 'replay', 'first.rec'),
                   help='a RECORD.OUT of the replay DOS build (default tests/replay/first.rec)')
    p = sub.add_parser('port-self'); p.add_argument('out'); variant_args(p, True)
    p.add_argument('--keys', default=SELF_KEYS, help="the port's --keys script (MS:KEY ...; F9 is added)")
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
    if a.cmd == 'port':
        return port_replay(a.rec, a.out, variant=a.variant or 'linux-gcc')
    if a.cmd == 'port-dgroup':
        return port_dgroup()
    variants = PORT_VARIANTS if getattr(a, 'all', False) else (getattr(a, 'variant', None) or ['linux-gcc'])
    if a.cmd == 'port-build':
        for v in variants:
            print('rp: %s: %s' % (v, port_build(v)))
        return 0
    if a.cmd == 'port-check':
        return port_check(a.out, a.rec, variants)
    if a.cmd == 'port-self':
        bad = 0
        for v in variants:
            bad |= port_self(a.out if len(variants) == 1 else os.path.join(a.out, v), v, a.keys)
        return bad
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
