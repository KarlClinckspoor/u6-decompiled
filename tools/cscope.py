"""Scope analysis with pycparser on preprocessed text.

analyze(pp_text, name) -> dict(
    funcs=[{'name':..., 'locals': set(...)}...]   # in source order
    globals=set(...)  # top-level declared names, struct members, tags, typedefs, enum consts
)
"""
import re
from pycparser import c_parser, c_ast

PARSE_DEFS = ['far=', 'near=', 'huge=', 'cdecl=', 'pascal=', 'interrupt=', '_cdecl=', '__far=']


def _fix_for_parser(txt):
    txt = re.sub(r'\b(static|extern)\s+(?=[A-Za-z_]\w*\s*\()', r'\1 int ', txt)
    txt = re.sub(r'=\s*\.\.\.\s*;', '= 0;', txt)
    return txt


class _Collect(c_ast.NodeVisitor):
    def __init__(self):
        self.names = set()

    def visit_Decl(self, node):
        if node.name:
            self.names.add(node.name)
        self.generic_visit(node)

    def visit_Label(self, node):
        self.names.add(node.name)
        self.generic_visit(node)

    def visit_Struct(self, node):
        # struct declared inside a function: tag/members are "local" too, but we keep them global
        self.generic_visit(node)


class _Members(c_ast.NodeVisitor):
    def __init__(self):
        self.names = set()

    def _su(self, node):
        if node.name:
            self.names.add(node.name)
        if node.decls:
            for d in node.decls:
                if d.name:
                    self.names.add(d.name)
        self.generic_visit(node)

    visit_Struct = _su
    visit_Union = _su

    def visit_Enum(self, node):
        if node.name:
            self.names.add(node.name)
        if node.values:
            for e in node.values.enumerators:
                self.names.add(e.name)
        self.generic_visit(node)

    def visit_Typedef(self, node):
        self.names.add(node.name)
        self.generic_visit(node)


def analyze(pp_text, name='tu'):
    ast = c_parser.CParser().parse(_fix_for_parser(pp_text), name)
    funcs = []
    glob = set()
    for ext in ast.ext:
        if isinstance(ext, c_ast.FuncDef):
            col = _Collect()
            # parameters
            fdecl = ext.decl.type
            while not isinstance(fdecl, c_ast.FuncDecl):
                fdecl = fdecl.type
            if fdecl.args:
                for p in fdecl.args.params:
                    if isinstance(p, c_ast.ID):
                        col.names.add(p.name)
                    elif getattr(p, 'name', None):
                        col.names.add(p.name)
            if ext.param_decls:
                for d in ext.param_decls:
                    col.names.add(d.name)
            col.visit(ext.body)
            funcs.append({'name': ext.decl.name, 'locals': col.names, 'knr': bool(ext.param_decls)})
            glob.add(ext.decl.name)
        elif isinstance(ext, c_ast.Decl):
            if ext.name:
                glob.add(ext.name)
        elif isinstance(ext, c_ast.Typedef):
            glob.add(ext.name)
    m = _Members()
    m.visit(ast)
    glob |= m.names
    return {'funcs': funcs, 'globals': glob}
