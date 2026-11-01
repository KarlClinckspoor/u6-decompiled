#!/usr/bin/env python3
"""List the expressions whose value can differ between Turbo C (16-bit int) and the host.

    python3 port/tools/intaudit.py [FILE.c ...] > audit.txt

Turbo C does unsigned 16-bit arithmetic when an operand is `unsigned` (uint16) and none is
long; the host promotes uint16 to a signed 32-bit int first. So a comparison, a division,
a modulo or a right shift whose operand is (or contains, before any narrowing) such an
operation can give a different answer, and so can an `int` arithmetic result that leaves
16 bits before it is compared, divided, shifted right or widened. Hexadecimal constants
8000h..FFFFh are unsigned int in Turbo C and int on the host.

This walks clang's AST and prints, per site, the operator, the DOS type of its operands
and the source text, for a person to decide (a cast in the source that gives the DOS
result, proved by tools/dosbuild.py, or "no difference possible").
"""
import json, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(ROOT, 'SRC')
INC = os.path.join(ROOT, 'port', 'include')

CMP = {'<', '>', '<=', '>=', '/', '%', '>>', '==', '!='}
ARITH = {'+', '-', '*', '<<', '|', '^', '&', '~'}


def dos_type(qt):
    """'s16','u16','s32','u32','s8','u8', or None"""
    t = qt.get('desugaredQualType', qt.get('qualType', ''))
    q = qt.get('qualType', '')
    names = q + ' ' + t
    if 'uint16' in q or t in ('unsigned short',):
        return 'u16'
    if 'int16' in q or t == 'short':
        return 's16'
    if 'uint32' in q or t == 'unsigned int':
        return 'u32'
    if 'int32' in q or t == 'int':
        return 's32' if 'int32' in q else 'int'
    if t in ('unsigned char',):
        return 'u8'
    if t in ('char', 'signed char'):
        return 's8'
    if t in ('long', 'long long', 'unsigned long'):
        return 'wide'
    return None


def strip(n):
    while n.get('kind') in ('ImplicitCastExpr', 'ParenExpr') and n.get('inner'):
        if n.get('kind') == 'ImplicitCastExpr' and n.get('castKind') not in ('IntegralCast', 'LValueToRValue', 'NoOp'):
            break
        n = n['inner'][0]
    return n


def dtype_of(n):
    """the type the expression has in Turbo C"""
    n0 = strip(n)
    k = n0.get('kind')
    if k == 'IntegerLiteral':
        v = int(n0.get('value', '0'))
        txt = n0.get('_text', '')
        if 0x8000 <= v <= 0xffff and not txt.startswith(tuple('123456789')):
            return 'u16'
        if v > 0x7fff:
            return 's32'
        return 's16'
    if k == 'CharacterLiteral':
        return 's16'
    if k == 'BinaryOperator' and n0['opcode'] in CMP | ARITH | {'&&', '||'}:
        if n0['opcode'] in ('<', '>', '<=', '>=', '==', '!=', '&&', '||'):
            return 's16'
        a, b = dtype_of(n0['inner'][0]), dtype_of(n0['inner'][1])
        return arith(a, b, n0['opcode'])
    if k == 'UnaryOperator':
        return dtype_of(n0['inner'][0]) if n0['opcode'] in ('-', '~', '+') else 's16'
    if k == 'CStyleCastExpr':
        return dos_type(n0['type']) or 's16'
    if k == 'ConditionalOperator':
        return arith(dtype_of(n0['inner'][1]), dtype_of(n0['inner'][2]), '?')
    t = dos_type(n0.get('type', {}))
    if t in ('u8', 's8'):
        return 's16'
    if t == 'int':
        return 's16'   # an implicit-int function or an int the port still has
    return t or 's16'


def arith(a, b, op):
    if op in ('<<', '>>'):
        return a if a in ('u16', 's32', 'u32') else 's16'
    s = {a, b}
    if 'u32' in s:
        return 'u32'
    if 's32' in s:
        return 's32'
    if 'u16' in s:
        return 'u16'
    return 's16'


SIMPLE = {'DeclRefExpr', 'MemberExpr', 'ArraySubscriptExpr', 'CallExpr', 'UnaryOperator'}


def nonneg_const(n):
    n0 = strip(n)
    if n0.get('kind') in ('IntegerLiteral', 'CharacterLiteral'):
        return int(n0.get('value', '0')) >= 0
    return False


def neg_const(n):
    n0 = strip(n)
    if n0.get('kind') == 'UnaryOperator' and n0.get('opcode') == '-' and strip(n0['inner'][0]).get('kind') == 'IntegerLiteral':
        return True
    return False


def is_arith(n):
    n0 = strip(n)
    return n0.get('kind') == 'BinaryOperator' and n0['opcode'] in ('+', '-', '*', '<<')


def walk(n, out, src_of):
    k = n.get('kind')
    if k == 'BinaryOperator' and n.get('opcode') in CMP:
        a, b = n['inner'][0], n['inner'][1]
        ta, tb = dtype_of(a), dtype_of(b)
        op = n['opcode']
        dos = arith(ta, tb, op)
        reason = None
        if op == '>>':
            if ta == 'u16' and is_arith(a):
                reason = 'unsigned 16-bit arithmetic shifted right'
            elif ta == 's16' and is_arith(a):
                reason = '16-bit arithmetic shifted right without narrowing'
        elif dos == 'u16':
            # one side unsigned: differs if the other can be negative or either is arithmetic
            for x, y, tx, ty in ((a, b, ta, tb), (b, a, tb, ta)):
                if tx == 'u16' and (ty == 's16' and not nonneg_const(y)):
                    if neg_const(y):
                        reason = 'unsigned 16-bit %s with a negative constant (DOS compares as unsigned)' % op
                    elif ty == 's16':
                        reason = 'unsigned 16-bit %s against a signed value' % op
                if is_arith(x) and tx == 'u16':
                    reason = 'unsigned 16-bit arithmetic compared (%s)' % op
        else:
            for side in (a, b):
                if is_arith(side) and dtype_of(side) == 's16':
                    reason = '16-bit %s result used by %s without narrowing' % (strip(side)['opcode'], op)
        if reason:
            out.append((n, reason))
    for c in n.get('inner', []) or []:
        if isinstance(c, dict):
            walk(c, out, src_of)


def main():
    files = sys.argv[1:] or sorted(os.path.join(SRC, f) for f in os.listdir(SRC) if f.endswith('.c'))
    total = 0
    for f in files:
        tu = os.path.basename(f)[:-2]
        cmd = ['clang', '-std=gnu89', '-fsigned-char', '-w', '-Wno-error=int-conversion', '-Wno-error=return-type', '-Wno-error=incompatible-function-pointer-types', '-Wno-error=implicit-function-declaration', '-Wno-error=implicit-int', '-DPORT_TU_%s' % tu, '-I', SRC, '-I', INC,
               '-include', os.path.join(INC, 'compat.h'), '-fsyntax-only', '-Xclang', '-ast-dump=json', f]
        r = subprocess.run(cmd, capture_output=True, text=True, cwd=SRC)
        if r.returncode:
            print('# %s: clang failed' % f, file=sys.stderr)
            continue
        ast = json.loads(r.stdout)
        lines = open(f, encoding='latin-1').read().split('\n')
        sites = []
        for d in ast.get('inner', []):
            loc = d.get('loc', {})
            if d.get('kind') == 'FunctionDecl' and d.get('inner') and loc.get('file', f).endswith(os.path.basename(f)):
                fs = []
                walk(d, fs, None)
                for n, why in fs:
                    rng = n.get('range', {}).get('begin', {})
                    ln = rng.get('line') or rng.get('expansionLoc', {}).get('line')
                    sites.append((ln, d.get('name'), why))
        seen = set()
        for ln, fn, why in sites:
            key = (ln, why)
            if key in seen or not ln:
                continue
            seen.add(key)
            total += 1
            print('%s:%s: %s: %s\n    %s' % (os.path.basename(f), ln, fn, why, lines[ln - 1].strip() if ln and ln <= len(lines) else ''))
    print('# %d sites' % total, file=sys.stderr)


if __name__ == '__main__':
    main()
