"""Preprocess every C translation unit of a source tree with gcc's cpp.

System headers are replaced by stubs (identical for both trees, so they cancel
out in a comparison).  Include lookup is made case-insensitive like DOS.
"""
import os, shutil, subprocess, sys, tempfile

STUB_HEADERS = ['dos.h', 'stdlib.h', 'alloc.h', 'process.h', 'ctype.h', 'string.h', 'math.h', 'stdio.h', 'mem.h', 'io.h', 'conio.h']


def _mirror(src_dir, dst_dir):
    for root, dirs, files in os.walk(src_dir):
        rel = os.path.relpath(root, src_dir)
        out = os.path.join(dst_dir, rel)
        os.makedirs(out, exist_ok=True)
        for f in files:
            p = os.path.join(root, f)
            data = open(p, 'rb').read().replace(b'\r\n', b'\n')
            open(os.path.join(out, f), 'wb').write(data)
            for alias in {f.lower(), f.upper()}:
                ap = os.path.join(out, alias)
                if not os.path.exists(ap):
                    os.symlink(f, ap)


def translation_units(src_dir):
    tus = []
    for root, dirs, files in os.walk(src_dir):
        for f in sorted(files):
            if f.lower().endswith('.c'):
                tus.append(os.path.relpath(os.path.join(root, f), src_dir))
    return sorted(tus)


def preprocess_tree(src_dir, extra_defs=(), only=None):
    """returns {tu_relpath: preprocessed_text}"""
    res = {}
    with tempfile.TemporaryDirectory() as tmp:
        tree = os.path.join(tmp, 'src')
        stub = os.path.join(tmp, 'stub')
        os.makedirs(stub)
        for h in STUB_HEADERS:
            open(os.path.join(stub, h), 'w').write('')
        _mirror(src_dir, tree)
        for tu in translation_units(src_dir):
            if only and tu not in only and os.path.basename(tu) not in only:
                continue
            p = os.path.join(tree, tu)
            cmd = ['gcc', '-E', '-P', '-undef', '-nostdinc', '-I', stub, '-I', os.path.dirname(p),
                   '-I', tree] + ['-D' + d for d in extra_defs] + ['-x', 'c', p]
            r = subprocess.run(cmd, capture_output=True, text=True)
            if r.returncode != 0:
                raise RuntimeError('cpp failed on %s:\n%s' % (tu, r.stderr))
            res[tu] = r.stdout
    return res


if __name__ == '__main__':
    out = preprocess_tree(sys.argv[1])
    for k, v in out.items():
        print(k, len(v))
