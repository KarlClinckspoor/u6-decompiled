#!/usr/bin/env python3
"""Edit an Ultima VI saved game, for test set-ups (PLAN-exact-port.md section 19: test set-ups
go through the saved game, not runtime cheats, so DOS and the port start from the same files
and a session stays proved against DOS).

    python3 tools/u6save.py DIR [--base GAMEDIR] [--out OUTDIR] COMMAND [COMMAND ...]

DIR holds SAVEGAME/ (or is a SAVEGAME folder). Files the commands need that DIR lacks (an
OBJBLKxy area file) come from GAMEDIR's SAVEGAME/ (default: $U6_GAMEDATA or gamedata/). The
result goes to OUTDIR's SAVEGAME/ (default: DIR's), only the files that changed. Each COMMAND is
one argument (quote it):

    show                        the party, their attributes and inventories, the state block
    set NAME VALUE              a field of the saved state block (SRC/gamestat.c's names:
                                KARMA, Time_H, Date_D, IsOnQuest, MoonstonePos[3] ...); an array
                                without an index takes VALUE in each element
    npc WHO FIELD VALUE         an NPC's field: str dex int magic hp exp level movepts status
                                talkflags mode commode flag schedindex, or name (party only)
    party NPC[:NAME] ...        the party list, in order (NAME for a member new to it)
    teleport X Y Z              the party to X, Y, Z (all on that tile, as the game's PartyExit
                                puts them before they step out), with their belongings: their
                                items move to the area file of the new place
    give WHO TYPE [QUANTITY [QUALITY [FRAME]]] [equip]
                                a new item (object type TYPE, 0..3FFh) in WHO's inventory;
                                equip readies it (a spellbook must be, to cast)
    unequip WHO TYPE            WHO's readied item of TYPE back to the inventory
    place TYPE X Y Z [QUANTITY [QUALITY [FRAME]]]
                                a new object on the map (a cannon: 0DDh, frame its
                                direction, 0 north, 1 east, 2 south, 3 west)
    fill TYPE X0 Y0 X1 Y1 Z [QUANTITY [QUALITY [FRAME]]]
                                place on every tile of a rectangle (a crowded area)
    map X Y Z [R]               the terrain around a place (. passable, ~ water, # not)
    count X Y Z                 the number of objects in the area file of a place
    put TYPE [QUANTITY [QUALITY [FRAME]]]
                                a new item inside the item the last give or place made: a spell
                                (3Ah, quality its number) in a spellbook (39h), a moonstone
                                (49h, frame its phase) in the vortex cube (3Eh)

WHO is an NPC number (1 is the Avatar), `avatar`, or a party member's name. Numbers may be
hex (0x...).

The files (SRC/console.c's GAME_Load and GAME_Save, SRC/object.c):
  OBJLIST     the NPCs' objects (the first 100h of ObjStatus, ObjPos, ObjShapeType, Amount), their
              tables (NPCStatus, STREN, DEXTE, INTEL, ExpPoints, HitPoints, Names, Party,
              PartySize, Level, SchedIndex, NPCMode, NPCComMode, MAGIC, MovePts, OrigShapeType,
              TalkFlags, Leader, NPCFlag, PathData) and the state block (gamestat.c), 1C73h bytes
  OBJBLKxy    the objects of one area (x = 'A' + X / 128, y = 'A' + Y / 128 on the surface;
              OBJBLKzI for dungeon level z = 'A' + level - 1): a word count, then 8 bytes each
              (status, position, shape and type, amount). An item an NPC holds is stored in the
              file of the area where the NPC stands, its position the NPC's number; an item in a
              container, the container's index in the file.
"""
import argparse, os, re, shutil, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

OBJLIST_SIZE = 0x1C73
GSTA_AT, GSTA_DS = 0x1BF1, 0x2C4A          # the state block in OBJLIST, and in DGROUP

# (name, offset in OBJLIST, bytes per NPC)
NPC_FIELDS = {
    'status': (0x800, 1), 'str': (0x900, 1), 'dex': (0xA00, 1), 'int': (0xB00, 1),
    'exp': (0xC00, 2), 'hp': (0xE00, 1), 'level': (0xFF1, 1), 'schedindex': (0x10F1, 1),
    'mode': (0x11F1, 1), 'commode': (0x12F1, 1), 'magic': (0x13F1, 1), 'movepts': (0x14F1, 1),
    'talkflags': (0x17F1, 1), 'flag': (0x19F1, 1),
}
OBJ_STATUS, OBJ_POS, OBJ_SHAPE, OBJ_AMOUNT = 0, 0x100, 0x400, 0x600
NAMES, PARTY, PARTY_SIZE = 0xF00, 0xFE0, 0xFF0

LOCXYZ, CONTAINED, INVEN, EQUIP = 0, 8, 0x10, 0x18


def num(s):
    return int(s, 0)


def gsta_fields():
    """SRC/gamestat.c's state block: {name: (offset, element size, count, signed)}"""
    sizes = {'unsigned char': (1, False), 'char': (1, True), 'int16': (2, True), 'uint16': (2, False)}
    out = {}
    src = open(os.path.join(ROOT, 'SRC', 'gamestat.c'), encoding='latin-1').read()
    for m in re.finditer(r'/\*([0-9A-F]{4})\*/(unsigned char|char|uint16|int16)\s+(\w+)((?:\[\d+\])*)', src):
        ds, typ, name, dims = int(m.group(1), 16), m.group(2), m.group(3), m.group(4)
        if name == 'GameStateEnd':
            continue
        n = 1
        for d in re.findall(r'\[(\d+)\]', dims):
            n *= int(d)
        size, signed = sizes[typ]
        out[name] = (ds - GSTA_DS, size, n, signed)
    return out


def coord(raw):
    """(x, y, z) of a position's three bytes (u6.h's GetCoordX/Y/Z)"""
    return (raw[0] | (raw[1] & 3) << 8, (raw[1] >> 2 | raw[2] << 6) & 0x3FF, raw[2] >> 4)


def raw_coord(x, y, z):
    """u6.h's SetCoordXYZ"""
    return bytes([x & 0xFF, (x >> 8 & 3) | (y << 2 & 0xFC), (y >> 6 & 0x0F) | (z << 4 & 0xF0)])


def area_file(x, y, z):
    return 'OBJBLK' + (chr(65 + (x >> 7)) + chr(65 + (y >> 7)) if z == 0 else chr(65 + z - 1) + 'I')


class Terrain:
    """the map's tiles and their terrain flags, read from the game's MAP, CHUNKS and TILEFLAG
    (SRC/map.c's getChunkIdxAt; main.c reads TILEFLAG's first 800h bytes as TerrainType)"""
    IMPASSABLE, WET = 0x02, 0x01

    def __init__(self, gamedir):
        def f(n):
            for x in os.listdir(gamedir):
                if x.upper() == n:
                    return open(os.path.join(gamedir, x), 'rb').read()
            sys.exit('u6save: no %s in %s' % (n, gamedir))
        self.map, self.chunks, self.flags = f('MAP'), f('CHUNKS'), f('TILEFLAG')[:0x800]

    def tile(self, x, y, z):
        x, y = x & 0x3FF, y & 0x3FF
        if z == 0:
            base = ((x >> 7) + ((y >> 7) << 3)) * 0x180
            n = ((y >> 3) & 15) * 16 + ((x >> 3) & 15)
        else:
            base = 0x6000 + (z - 1) * 0x600
            n = ((y >> 3) & 31) * 32 + ((x >> 3) & 31)
        p = base + n * 3 // 2
        w = self.map[p] | self.map[p + 1] << 8
        chunk = w >> 4 if n & 1 else w & 0xFFF
        return self.chunks[chunk * 64 + (y & 7) * 8 + (x & 7)]

    def passable(self, x, y, z):
        return not self.flags[self.tile(x, y, z)] & self.IMPASSABLE


class Obj:
    """one record of an area file; parent is the container's Obj for a CONTAINED one"""
    def __init__(self, b, parent=None):
        self.status, self.pos, self.shape, self.amount = b[0], bytes(b[1:4]), b[4] | b[5] << 8, b[6] | b[7] << 8
        self.parent = parent

    use = property(lambda s: s.status & 0x18)
    assoc = property(lambda s: s.pos[0] | s.pos[1] << 8)


class Area:
    def __init__(self, data):
        n = data[0] | data[1] << 8
        self.objs = []
        for i in range(n):
            b = data[2 + 8 * i:10 + 8 * i]
            o = Obj(b)
            if o.use == CONTAINED:
                o.parent = self.objs[o.assoc]
            self.objs.append(o)

    def root(self, o):
        while o.parent:
            o = o.parent
        return o

    def groups(self):
        """the records in runs of a top-level one and what it holds (contained ones follow their
        container, OBJ_UnloadArea)"""
        out = []
        for o in self.objs:
            if o.use == CONTAINED and out and self.root(o) in out[-1]:
                out[-1].append(o)
            elif o.use == CONTAINED:
                for g in out:                        # its container's run, wherever it is
                    if self.root(o) in g:
                        g.append(o)
                        break
            else:
                out.append([o])
        return out

    def data(self):
        index = {id(o): i for i, o in enumerate(self.objs)}
        b = bytearray([len(self.objs) & 0xFF, len(self.objs) >> 8])
        for o in self.objs:
            pos = o.pos
            if o.use == CONTAINED:
                # the container's index; the third byte is the game's stack junk (object.c's
                # __ObjSerialize never sets it), kept as it was
                i = index[id(o.parent)]
                pos = bytes([i & 0xFF, i >> 8, o.pos[2]])
            b += bytes([o.status]) + pos + bytes([o.shape & 0xFF, o.shape >> 8, o.amount & 0xFF, o.amount >> 8])
        return bytes(b)


class Save:
    def __init__(self, d, base=None):
        self.dir = d if os.path.basename(os.path.normpath(d)).upper() == 'SAVEGAME' else self._ci(d, 'SAVEGAME')
        self.base = base
        self.files = {}                      # NAME: bytes, read or changed
        self.changed = set()
        self.areas = {}
        self.objlist = bytearray(self.read('OBJLIST'))
        if len(self.objlist) < OBJLIST_SIZE:
            sys.exit('u6save: %s is %d bytes, not a saved game (%d)' % ('OBJLIST', len(self.objlist), OBJLIST_SIZE))
        self.gsta = gsta_fields()

    @staticmethod
    def _ci(d, name):
        if os.path.isdir(d):
            for n in os.listdir(d):
                if n.upper() == name.upper():
                    return os.path.join(d, n)
        return os.path.join(d, name)

    def read(self, name):
        if name not in self.files:
            for d in (self.dir, self.base and self._ci(self.base, 'SAVEGAME')):
                if d and os.path.exists(self._ci(d, name)):
                    self.files[name] = open(self._ci(d, name), 'rb').read()
                    break
            else:
                sys.exit('u6save: no SAVEGAME/%s in %s%s' % (name, self.dir, ' or ' + self.base if self.base else ''))
        return self.files[name]

    def area(self, name):
        if name not in self.areas:
            self.areas[name] = Area(self.read(name))
        return self.areas[name]

    def write(self, out_dir=None):
        d = self._ci(out_dir, 'SAVEGAME') if out_dir else self.dir
        os.makedirs(d, exist_ok=True)
        self.files['OBJLIST'] = bytes(self.objlist)
        for name, a in self.areas.items():
            if name in self.changed:
                self.files[name] = a.data()
        names = sorted(self.changed | {'OBJLIST'})
        for n in names:
            p = self._ci(d, n)
            if os.path.exists(p) and os.path.basename(p) != n:
                os.remove(p)                 # one name only, upper case, as the game's folder has
            open(os.path.join(d, n), 'wb').write(self.files[n])
        return names

    # ---- the tables ----
    def get(self, at, size, signed=False):
        return int.from_bytes(self.objlist[at:at + size], 'little', signed=signed)

    def put(self, at, size, v):
        self.objlist[at:at + size] = (v & ((1 << 8 * size) - 1)).to_bytes(size, 'little')

    def party(self):
        return list(self.objlist[PARTY:PARTY + self.objlist[PARTY_SIZE]])

    def name(self, slot):
        return self.objlist[NAMES + 14 * slot:NAMES + 14 * slot + 14].split(b'\0')[0].decode('latin-1')

    def who(self, s):
        if s.lower() == 'avatar':
            return 1
        try:
            n = num(s)
        except ValueError:
            for slot, npc in enumerate(self.party()):
                if self.name(slot).lower() == s.lower():
                    return npc
            sys.exit('u6save: no party member named %s (the party: %s)' % (
                s, ', '.join(self.name(i) for i in range(len(self.party())))))
        if not 0 <= n < 0x100:
            sys.exit('u6save: NPC %s is not 0..FFh' % s)
        return n

    def npc_pos(self, n):
        return coord(self.objlist[OBJ_POS + 3 * n:OBJ_POS + 3 * n + 3])

    # ---- the commands ----
    def cmd_set(self, name, value):
        m = re.match(r'(\w+)(?:\[(\d+)\])?$', name)
        if not m or m.group(1) not in self.gsta:
            sys.exit('u6save: no field %s in the state block (%s)' % (name, ' '.join(self.gsta)))
        off, size, n, _ = self.gsta[m.group(1)]
        idx = [int(m.group(2))] if m.group(2) is not None else range(n)
        for i in idx:
            if i >= n:
                sys.exit('u6save: %s has %d elements' % (m.group(1), n))
            self.put(GSTA_AT + off + i * size, size, num(value))

    def cmd_party(self, *members):
        """party NPC[:NAME] ...: the party list (at most 8), in order; a member already in the
        party keeps its name unless NAME is given, a new one needs NAME (the name the game shows).
        Before a teleport, so that it brings the new members; their NPC modes are left to `npc`
        (a party member's mode is 1, and 2 for the Avatar)"""
        if not 1 <= len(members) <= 8:
            sys.exit('u6save: party: 1 to 8 members')
        old = {npc: self.name(slot) for slot, npc in enumerate(self.party())}
        names = []
        for m in members:
            n, _, nm = m.partition(':')
            n = self.who(n)
            if not nm and n not in old:
                sys.exit('u6save: party: NPC %d is new to the party: give its name (%d:NAME)' % (n, n))
            names.append((n, nm or old[n]))
        for slot in range(8):
            n, nm = names[slot] if slot < len(names) else (0, '')
            self.objlist[PARTY + slot] = n
            b = nm.encode('latin-1')[:13]
            self.objlist[NAMES + 14 * slot:NAMES + 14 * slot + 14] = b + bytes(14 - len(b))
        self.objlist[PARTY_SIZE] = len(names)

    def cmd_npc(self, who, field, value):
        n = self.who(who)
        if field == 'name':
            if n not in self.party():
                sys.exit('u6save: only a party member has a name in the saved game')
            slot = self.party().index(n)
            b = value.encode('latin-1')[:13]
            self.objlist[NAMES + 14 * slot:NAMES + 14 * slot + 14] = b + bytes(14 - len(b))
            return
        if field not in NPC_FIELDS:
            sys.exit('u6save: no NPC field %s (%s name)' % (field, ' '.join(NPC_FIELDS)))
        at, size = NPC_FIELDS[field]
        self.put(at + size * n, size, num(value))

    def held(self, area, npc):
        """the runs of area holding npc's items (INVEN or EQUIP, the position its number)"""
        return [g for g in area.groups() if g[0].status & 0x10 and g[0].assoc == npc]

    def key(self, g):
        """OBJ_CompareOrder's order of a run: its root's tile (an NPC's for a held one), by y,
        then x, then z downwards"""
        o = g[0]
        x, y, z = self.npc_pos(o.assoc) if o.status & 0x10 and o.assoc < 0x100 else coord(o.pos)
        return (y, x, -z)

    def insert(self, area, runs, in_front=False):
        """runs placed in area before the first run that comes after them in the game's order;
        in_front: before the runs on the same tile too, as the game's AddMapObj puts a new object
        in front of the objects already there (the first one FindLoc finds, which a drop onto
        the tile takes as its container)"""
        if not runs:
            return
        rest = area.groups()
        k = self.key(runs[0])
        i = next((j for j, g in enumerate(rest) if (self.key(g) >= k if in_front else self.key(g) > k)), len(rest))
        rest[i:i] = runs
        area.objs = [o for g in rest for o in g]

    def terrain(self):
        if not getattr(self, '_terrain', None):
            self._terrain = Terrain(self.base or default_base())
        return self._terrain

    def cmd_map(self, x, y, z, r='8'):
        """map X Y Z [R]: the terrain around X, Y: . passable, ~ wet (water), # impassable, @ the
        place, P a party member, E an egg"""
        x, y, z, r = num(x), num(y), num(z), num(r)
        t = self.terrain()
        party = {self.npc_pos(n) for n in self.party()}
        eggs = set()
        a = self.area(area_file(x, y, z))
        for o in a.objs:
            if o.use == LOCXYZ and o.shape & 0x3FF == 0x14F:
                eggs.add(coord(o.pos))
        for yy in range(y - r, y + r + 1):
            row = ''
            for xx in range(x - r, x + r + 1):
                p = (xx, yy, z)
                f = t.flags[t.tile(xx, yy, z)]
                row += '@' if (xx, yy) == (x, y) else 'P' if p in party else 'E' if p in eggs else \
                    '~' if f & Terrain.WET else '#' if f & Terrain.IMPASSABLE else '.'
            print('%4d %s' % (yy, row))

    def cmd_teleport(self, x, y, z, force=None):
        x, y, z = num(x), num(y), num(z)
        if not (0 <= x < 0x400 and 0 <= y < 0x400 and 0 <= z <= 5):
            sys.exit('u6save: %d,%d,%d is off the map' % (x, y, z))
        if force != 'force' and not self.terrain().passable(x, y, z):
            sys.exit('u6save: %d,%d,%d is impassable terrain (tile %02Xh); add "force" to teleport there '
                     'anyway, or look with "map %d %d %d"' % (x, y, z, self.terrain().tile(x, y, z), x, y, z))
        if self.get(GSTA_AT + self.gsta['Active'][0], 1) == len(self.party()):
            sys.exit('u6save: the party is in a vehicle (Active = PartySize); not moved')
        moving = {}
        for npc in self.party():
            src = area_file(*self.npc_pos(npc))
            a = self.area(src)
            runs = self.held(a, npc)
            gone = {id(o) for g in runs for o in g}
            a.objs = [o for o in a.objs if id(o) not in gone]
            if runs:
                self.changed.add(src)
            moving[npc] = runs
            self.objlist[OBJ_POS + 3 * npc:OBJ_POS + 3 * npc + 3] = raw_coord(x, y, z)
        dst = self.area(area_file(x, y, z))
        for npc in self.party():
            self.insert(dst, moving[npc])
            if moving[npc]:
                self.changed.add(area_file(x, y, z))
        for f, v in (('MapX', x), ('MapY', y), ('MapZ', z)):
            self.put(GSTA_AT + self.gsta[f][0], 2, v)

    @staticmethod
    def item(status, typ, quantity, quality, frame, pos=b'\0\0\0'):
        t, q, ql, f = num(typ), num(quantity), num(quality), num(frame)
        if not (0 <= t < 0x400 and 0 <= q < 0x100 and 0 <= ql < 0x100 and 0 <= f < 0x40):
            sys.exit('u6save: type 0..3FFh, quantity and quality 0..FFh, frame 0..3Fh')
        return Obj(bytes([status]) + pos + (t | f << 10).to_bytes(2, 'little') + bytes([q, ql]))

    def cmd_give(self, who, typ, *rest):
        """give WHO TYPE [QUANTITY [QUALITY [FRAME]]] [equip]: equip readies it (EQUIP; the
        game works out the slot from the type, STAT_GetEquipSlot), which a spellbook needs"""
        rest = list(rest)
        equip = bool(rest) and rest[-1] == 'equip'
        if equip:
            rest.pop()
        q, ql, f = (rest + ['1', '0', '0'][len(rest):])[:3] if len(rest) <= 3 else sys.exit('u6save: give: too many arguments')
        n = self.who(who)
        name = area_file(*self.npc_pos(n))
        a = self.area(name)
        o = self.item((EQUIP if equip else INVEN) | 1, typ, q, ql, f, bytes([n, 0, 0]))
        self.last = (name, o)
        runs = self.held(a, n)
        if runs:                             # after the NPC's own items
            last = runs[-1][-1]
            i = a.objs.index(last) + 1
            a.objs[i:i] = [o]
        else:
            self.insert(a, [[o]])
        if len(a.objs) >= 0xC00:
            sys.exit('u6save: %s would hold too many objects' % name)
        self.changed.add(name)

    def cmd_unequip(self, who, typ):
        """unequip WHO TYPE: WHO's readied item of TYPE goes to the inventory (EQUIP to INVEN), to
        free a hand for another"""
        n, t = self.who(who), num(typ)
        a = self.area(area_file(*self.npc_pos(n)))
        for run in self.held(a, n):
            if run[0].use == EQUIP and run[0].shape & 0x3FF == t:
                run[0].status = (run[0].status & ~0x18) | INVEN
                self.changed.add(area_file(*self.npc_pos(n)))
                return
        sys.exit('u6save: unequip: NPC %d has no readied item of type %03Xh' % (n, t))

    def cmd_place(self, typ, x, y, z, *rest):
        """place TYPE X Y Z [QUANTITY [QUALITY [FRAME]]] [behind]: a new object on the map (a
        cannon, 0DDh, frame its direction: 0 north, 1 east, 2 south, 3 west), in the area file of
        that place, in front of the objects already on that tile as the game's AddMapObj puts
        one (the first FindLoc finds: a drop onto the tile takes it as the container); behind:
        after them instead (as u6save did before, which [drake]'s recording was made with)"""
        rest = list(rest)
        behind = bool(rest) and rest[-1] == 'behind'
        if behind:
            rest.pop()
        quantity, quality, frame = (rest + ['0', '0', '0'][len(rest):])[:3]
        x, y, z = num(x), num(y), num(z)
        name = area_file(x, y, z)
        a = self.area(name)
        o = self.item(LOCXYZ, typ, quantity, quality, frame, raw_coord(x, y, z))
        self.insert(a, [[o]], in_front=not behind)
        self.last = (name, o)
        self.changed.add(name)

    def cmd_fill(self, typ, x0, y0, x1, y1, z, *rest):
        """fill TYPE X0 Y0 X1 Y1 Z [QUANTITY [QUALITY [FRAME]]]: one new object on every tile of
        the rectangle (inclusive), as place does each, for a crowded area (docs/CODE_REVIEW.md,
        CR-007: the object table full while an area loads)"""
        x0, y0, x1, y1 = num(x0), num(y0), num(x1), num(y1)
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.cmd_place(typ, str(x), str(y), z, *rest)
                if len(self.area(area_file(x, y, num(z))).objs) >= 0xC00:
                    sys.exit('u6save: fill: %s would hold too many objects' % area_file(x, y, num(z)))

    def cmd_count(self, x, y, z):
        """count X Y Z: the number of objects in the area file of that place"""
        name = area_file(num(x), num(y), num(z))
        print('%s: %d objects' % (name, len(self.area(name).objs)))

    def cmd_put(self, typ, quantity='1', quality='0', frame='0'):
        """put TYPE [QUANTITY [QUALITY [FRAME]]]: a new item inside the item the last give made
        (a spell in a spellbook: type 3Ah, quality the spell's number; a moonstone in the vortex
        cube: type 49h, frame its phase), after what that item already holds"""
        if not getattr(self, 'last', None):
            sys.exit('u6save: put: no give before it to put into')
        name, box = self.last
        a = self.area(name)
        o = self.item(CONTAINED | 1, typ, quantity, quality, frame)
        o.parent = box
        i = a.objs.index(box) + 1
        while i < len(a.objs) and a.objs[i].use == CONTAINED and self._inside(a.objs[i], box):
            i += 1
        a.objs[i:i] = [o]
        self.changed.add(name)

    @staticmethod
    def _inside(o, box):
        while o.parent:
            if o.parent is box:
                return True
            o = o.parent
        return False

    def cmd_show(self):
        g = self.gsta
        val = lambda f: self.get(GSTA_AT + g[f][0], g[f][1], g[f][3])
        print('time %02d:%02d, %d/%d/%d; karma %d; quest %d; at %d,%d,%d' % (
            val('Time_H'), val('Time_M'), val('Date_M'), val('Date_D'), val('Date_Y'), val('KARMA'),
            val('IsOnQuest'), val('MapX'), val('MapY'), val('MapZ')))
        for slot, npc in enumerate(self.party()):
            f = {k: self.get(at + size * npc, size) for k, (at, size) in NPC_FIELDS.items()}
            print('%-13s NPC %3d at %d,%d,%d: str %d dex %d int %d magic %d hp %d exp %d level %d' % (
                self.name(slot), npc, *self.npc_pos(npc), f['str'], f['dex'], f['int'], f['magic'],
                f['hp'], f['exp'], f['level']))
            a = self.area(area_file(*self.npc_pos(npc)))
            for run in self.held(a, npc):
                for o in run:
                    print('    %s type %03Xh frame %d quantity %d quality %d%s' % (
                        'contained' if o.use == CONTAINED else 'equipped ' if o.use == EQUIP else 'held     ',
                        o.shape & 0x3FF, o.shape >> 10, o.amount & 0xFF, o.amount >> 8,
                        '' if o.use != CONTAINED else ' (in %03Xh)' % (o.parent.shape & 0x3FF)))

    def apply(self, command):
        import shlex
        w = shlex.split(command)
        if not w:
            return
        fn = getattr(self, 'cmd_' + w[0], None)
        if not fn:
            sys.exit('u6save: no command %s (show set npc party teleport give put place fill unequip map count)' % w[0])
        try:
            fn(*w[1:])
        except TypeError:
            sys.exit('u6save: %s: wrong arguments (python3 tools/u6save.py --help)' % command)


def default_base():
    return os.environ.get('U6_GAMEDATA') or os.path.join(ROOT, 'gamedata')


def edit(save_dir, commands, base=None, out=None):
    """applies COMMANDS to the saved game in save_dir; returns the names of the files written"""
    s = Save(save_dir, base or default_base())
    for c in commands:
        s.apply(c)
    return s.write(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('dir')
    ap.add_argument('commands', nargs='+')
    ap.add_argument('--base', default=default_base(), help="the game's folder, for the area files DIR lacks")
    ap.add_argument('--out', help="where SAVEGAME/ goes (default DIR's)")
    a = ap.parse_args()
    s = Save(a.dir, a.base)
    for c in a.commands:
        s.apply(c)
    if any(c.split()[0] not in ('show', 'map') for c in a.commands if c.strip()):
        print('u6save: wrote %s' % ' '.join(s.write(a.out)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
