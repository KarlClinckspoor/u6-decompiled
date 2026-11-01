"""Which program the tools in tools/u work on: U.EXE (the default) or END.EXE.

    U6_PROG=end python3 tools/u/udis.py ...

Each has its sources (SRC_U, SRC_END), its data segment and its symbol file; both are
linked with the same OSI library (SRC_U/OSILIB) and Origin's CM.LIB."""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))

PROFILES = {
    'u': dict(exe='U.EXE', src='SRC_U', dgroup=0x1499, syms='usyms.txt', lnk='u.lnk',
              libs='L\\CM\\CMU L\\OSILIB'),
    'end': dict(exe='END.EXE', src='SRC_END', dgroup=0x07B4, syms='endsyms.txt', lnk='end.lnk',
                libs='L\\CM\\CMU L\\OSILIB'),
}
NAME = os.environ.get('U6_PROG', 'u').lower()
P = PROFILES[NAME]
EXE = P['exe']
SRC = os.path.join(ROOT, P['src'])
SRC_U = os.path.join(ROOT, 'SRC_U')
OSILIB = os.path.join(SRC_U, 'OSILIB')
DGROUP = P['dgroup']
SYMS = os.path.join(HERE, P['syms'])


def find_exe():
    env = os.environ.get('U6_UEXE') if NAME == 'u' else os.environ.get('U6_' + NAME.upper() + 'EXE')
    for p in (env, os.path.join(ROOT, 'gamedata', EXE), os.path.join(ROOT, '..', 'gamedata', EXE),
              '/home/claude/gamedata/' + EXE):
        if p and os.path.exists(p):
            return p
    raise SystemExit('%s not found: set U6_%sEXE' % (EXE, NAME.upper()))


def source(name, exts=('.c', '.C', '.asm', '.ASM')):
    """a module's source: in the program's folder, else in SRC_U (shared modules)"""
    for d in (SRC, SRC_U):
        for e in exts:
            p = os.path.join(d, name + e)
            if os.path.exists(p):
                return p
    return None
