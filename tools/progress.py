#!/usr/bin/env python3
"""progress.py: progress card (docs/progress.svg) and numbers (docs/progress.json).

Three programs are built back byte for byte with Turbo C 2.0, MASM 3.0 and TLINK (README, "Verified
with the real compiler"): GAME.EXE (SRC/), U.EXE (SRC_U/) and END.EXE (SRC_END/). One tile is one
function of their sources:

  - a C function: its definition starts with the address marker `/*C_SSSS_OOOO*/` (frame, offset), and
  - an assembly routine: `ProcStart NAME` (Origin's OSI library macro) or `NAME PROC`,

in the order GAME, U, END, then by frame and offset (the OSI library that END links is SRC_U/OSILIB, so
it is counted under U), and its status is:

  matched  the function has a source and a real name
  attempt  reserved (a function being worked on)
  open     the function still carries an address placeholder for a name (C_SSSS_OOOO, __SSSS_OOOO, MUS_xxxx)
  aside    reserved for code that is not Ultima VI's own: none (Turbo C's CM.LIB is linked from the
           compiler's files and is not in the repository)

The percentage is matched / (matched + attempt + open). The script reads the sources only; it does
not rebuild anything (tools/dosbuild.py and tools/u/ubuild.py do) or compare bytes.

    python3 tools/progress.py            # writes docs/progress.svg and docs/progress.json
    python3 tools/progress.py --check    # exit 1 if the committed files are stale
"""
import argparse, glob, json, os, re, sys
from html import escape

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NAME = 'u6-decompiled'
PROGRAMS = [('GAME', ['SRC']), ('U', ['SRC_U']), ('END', ['SRC_END'])]
CAPTION = ('Function map: 1 function per tile (C and assembly), in order GAME.EXE, U.EXE, END.EXE, '
           'then by frame and offset')
COLORS = dict(matched='#2a8c3c', attempt='#b8860b', open='#64748b', aside='#b94a53')
LABELS = dict(matched='Matched', attempt='Attempt', open='Open', aside='Aside')
PLACEHOLDER = re.compile(r'^_*(?:C|MUS|SEG)?_*(?:[0-9A-Fa-f]{4}_)?[0-9A-Fa-f]{4}$')
MARKER = re.compile(r'/\*C_([0-9A-Fa-f]{4})_([0-9A-Fa-f]{4})\*/[ \t]*((?:\w+[ \t*]+)*)\**[ \t]*(\w+)[ \t]*\(')
PROCSTART = re.compile(r'^ProcStart[ \t]+(\w+)[ \t]*(?:;[ \t]*([0-9A-Fa-f]{4}):([0-9A-Fa-f]{4}))?', re.M)
PROC = re.compile(r'^(\w+)[ \t]+PROC\b', re.M | re.I)


def read(path):
    with open(path, encoding='latin1', newline='') as f:
        return f.read()


def strip_comments(text):
    def keep(m):
        s = m.group(0)
        return s if re.fullmatch(r'/\*C_[0-9A-Fa-f]{4}_[0-9A-Fa-f]{4}\*/', s) else re.sub(r'[^\n]', ' ', s)
    return re.sub(r'/\*.*?\*/', keep, text, flags=re.S)


def c_functions(path):
    text = strip_comments(read(path))
    out = []
    for m in MARKER.finditer(text):
        i, depth = m.end(), 1
        while i < len(text) and depth:
            depth += {'(': 1, ')': -1}.get(text[i], 0)
            i += 1
        rest = text[i:]
        if re.match(r'\s*;', rest):                        # a declaration
            continue
        if re.match(r'\s*(?:[^;{}]*;\s*)*\{', rest):       # K&R argument declarations, then the body
            out.append((m.group(4), (int(m.group(1), 16) << 16) | int(m.group(2), 16)))
    return out


def asm_functions(path):
    text = read(path)
    out = []
    for m in PROCSTART.finditer(text):
        addr = (int(m.group(2), 16) << 16 | int(m.group(3), 16)) if m.group(2) else 0
        out.append((m.group(1), addr))
    for m in PROC.finditer(text):
        name = m.group(1)
        pm = re.fullmatch(r'C_([0-9A-Fa-f]{4})_([0-9A-Fa-f]{4})', name)
        out.append((name, (int(pm.group(1), 16) << 16 | int(pm.group(2), 16)) if pm else 0))
    return out


def classify():
    flat = []
    for label, folders in PROGRAMS:
        rows, seen = [], set()
        for folder in folders:
            for path in sorted(glob.glob(os.path.join(ROOT, folder, '**', '*'), recursive=True)):
                ext = path.rsplit('.', 1)[-1].upper() if '.' in os.path.basename(path) else ''
                if ext == 'C':
                    fs = c_functions(path)
                elif ext == 'ASM':
                    fs = asm_functions(path)
                else:
                    continue
                for name, addr in fs:
                    if (name.upper(), addr) in seen:
                        continue
                    seen.add((name.upper(), addr))
                    rows.append(dict(program=label, file=os.path.relpath(path, ROOT).replace(os.sep, '/'),
                                     addr=addr, name=name,
                                     status='open' if PLACEHOLDER.match(name) else 'matched'))
        flat += sorted(rows, key=lambda r: (r['addr'], r['file'], r['name']))
    return flat


def summary(rows):
    c = {k: 0 for k in COLORS}
    for r in rows:
        c[r['status']] += 1
    total = c['matched'] + c['attempt'] + c['open']
    return dict(c, total=total, percent=round(100.0 * c['matched'] / total, 1) if total else 100.0)


def svg(flat):
    s = summary(flat)
    n = len(flat)
    W, PAD = 840, 20
    inner = W - 2 * PAD
    cols = 1
    for cols_try in range(20, 200):      # square tiles of at least 11 px
        if inner / cols_try < 11:
            break
        cols = cols_try
    cell = inner / cols
    top = 160
    H = int(top + -(-n // cols) * cell + 22)
    out = []
    a = out.append
    a('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d" '
      'font-family="Roboto, Helvetica, Arial, sans-serif" role="img" aria-label="%s">'
      % (W, H, W, H, escape('Decompilation progress: %d of %d functions matched' % (s['matched'], s['total']))))
    a('<rect width="%d" height="%d" rx="14" fill="#0f172a"/>' % (W, H))
    a('<text x="%d" y="38" font-size="21" fill="#ffffff">Decompilation progress</text>' % PAD)
    a('<text x="%d" y="38" font-size="15" fill="#ffffff" text-anchor="end">%s</text>' % (W - PAD, escape(NAME)))
    a('<text x="%d" y="62" font-size="15" fill="#e5e7eb">%s / %s functions matched (%.1f%%)</text>'
      % (PAD, format(s['matched'], ','), format(s['total'], ','), s['percent']))
    a('<rect x="%d" y="74" width="%d" height="18" rx="5" fill="#334155"/>' % (PAD, inner))
    if s['total']:
        a('<rect x="%d" y="74" width="%.2f" height="18" rx="5" fill="%s"/>'
          % (PAD, inner * s['matched'] / s['total'], COLORS['matched']))
    for x, k in zip([PAD, 232, 430, 628], ['matched', 'attempt', 'open', 'aside']):
        a('<rect x="%d" y="104" width="10" height="10" fill="%s"/>' % (x, COLORS[k]))
        a('<text x="%d" y="113" font-size="12" fill="#e5e7eb">%s: %s</text>' % (x + 14, LABELS[k], format(s[k], ',')))
    a('<text x="%d" y="140" font-size="11" fill="#e5e7eb">%s</text>' % (PAD, escape(CAPTION)))
    for i, r in enumerate(flat):
        x, y = PAD + (i % cols) * cell, top + (i // cols) * cell
        a('<rect x="%.2f" y="%.2f" width="%.2f" height="%.2f" fill="%s" stroke="#0f172a" stroke-width="1">'
          '<title>%s %s %04X:%04X %s (%s)</title></rect>'
          % (x, y, cell, cell, COLORS[r['status']], r['program'], r['file'], r['addr'] >> 16, r['addr'] & 0xFFFF,
             escape(r['name']), r['status']))
    a('</svg>')
    return '\n'.join(out) + '\n'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--check', action='store_true')
    args = ap.parse_args()
    flat = classify()
    result = {label: summary([r for r in flat if r['program'] == label]) for label, _ in PROGRAMS}
    result['all'] = summary(flat)
    js = json.dumps(result, indent=1) + '\n'
    paths = {os.path.join(ROOT, 'docs', 'progress.json'): js, os.path.join(ROOT, 'docs', 'progress.svg'): svg(flat)}
    if args.check:
        stale = [p for p, t in paths.items() if not os.path.exists(p) or read(p) != t]
        if stale:
            print('stale:', *stale)
            sys.exit(1)
        return
    for p, t in paths.items():
        with open(p, 'w', encoding='utf-8', newline='\n') as f:
            f.write(t)
    for k, r in result.items():
        print('%-5s matched %4d  open %3d  -> %.1f%% of %d' % (k, r['matched'], r['open'], r['percent'], r['total']))
    for x in flat:
        if x['status'] == 'open':
            print('open:', x['program'], x['file'], x['name'])


if __name__ == '__main__':
    main()
