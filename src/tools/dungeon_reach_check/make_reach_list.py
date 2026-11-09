#!/usr/bin/env python3
"""Make the list dungeon_reach_check walks: every dungeon and raid the bots run, its entrance and its bosses in the bots' order.

Sources, all in the repository:
  * entrances: the scenario catalogue (src/modules/PlayerbotV2/Diagnostics/ScenarioDefs.h, kDungeons and the raid entrances), which
    holds the Classic areatrigger_teleport targets;
  * bosses and their spots: the bots' own dungeon scripts (src/modules/PlayerbotV2/Bot/Dungeon/Scripts), the Classic advice when a
    script has one, `a.bosses` index for index with `a.progression_waypoints` (the spots the tank walks to), and the per-wing lists of
    the Scarlet Monastery and Dire Maul scripts;
  * a map with no script spots (the Forever dungeons, most raids): the boss entries of the Classic instance script
    (src/server/scripts/Custom/Classic/instances, ClassicDungeonBossEntry, encounter order) placed at their creature rows in
    sql/custom/world.

--spawns FILE replaces every boss spot with the boss's first spawn in the world database (a tab-separated file of
entry, map, x, y, z, as make_reach_spawns.sql writes it). Use it on the PC: the database is what the server runs.

A boss with no spot from any source is written as a 'missing' line; the check prints it as not checked.

Usage: make_reach_list.py [--spawns spawns.tsv] [--map ID ...] > reach_list.txt
"""
import argparse
import glob
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
MODULE = os.path.join(ROOT, "src", "modules", "PlayerbotV2")
SCRIPTS = os.path.join(MODULE, "Bot", "Dungeon", "Scripts")
SCENARIOS = os.path.join(MODULE, "Diagnostics", "ScenarioDefs.h")
INSTANCES = os.path.join(ROOT, "src", "server", "scripts", "Custom", "Classic", "instances")
WORLD_SQL = os.path.join(ROOT, "sql", "custom", "world")

RAID_KEYS = {409: "mc", 469: "bwl", 249: "onyxia", 309: "zg", 509: "aq20", 531: "aq40", 533: "naxx"}
NUM = r"-?\d+(?:\.\d+)?f?"


def read(path):
    with open(path, encoding="latin-1") as f:
        return f.read()


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def brace_block(text, start):
    """The text between the brace at or after `start` and its matching close brace."""
    i = text.index("{", start)
    depth = 0
    for j in range(i, len(text)):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                return text[i + 1:j]
    raise ValueError("unbalanced braces")


def parse_nested(text):
    """'{ { 1, 2 }, { 3.0f, 4.0f } }' contents -> nested lists of numbers (comments must be stripped)."""
    stack = [[]]
    for tok in re.findall(r"\{|\}|" + NUM, text):
        if tok == "{":
            stack.append([])
        elif tok == "}":
            done = stack.pop()
            stack[-1].append(done)
        else:
            stack[-1].append(float(tok.rstrip("f")))
    return stack[0]


def comment_names(text):
    """entry -> name from lines that hold one entry and a comment: '3914,   // Rethilgore', '{ CWC_ENC_KRESH, 3653 }, // Kresh'."""
    names = {}
    for line in text.splitlines():
        code, sep, comment = line.partition("//")
        ids = re.findall(r"(?<![\w.])(\d{3,7})(?![\w.])", code)
        if not sep or len(ids) != 1:
            continue
        name = re.split(r"\s*[(:]", comment.strip())[0].strip()
        if name and not name[0].isdigit():
            names.setdefault(int(ids[0]), name)
    return names


# ---- entrances ----

def entrances():
    text = read(SCENARIOS)
    rows = {}
    block = brace_block(text, text.index("kDungeons[]"))
    for m in re.finditer(r'\{\s*"(\w+)",\s*"([^"]*)",\s*(\d+),\s*(' + NUM + r'),\s*(' + NUM + r'),\s*(' + NUM + r')', block):
        key, summary, mp = m.group(1), m.group(2), int(m.group(3))
        x, y, z = (float(m.group(i).rstrip("f")) for i in (4, 5, 6))
        if key.endswith("36"):          # the same run at another level
            continue
        rows.setdefault(mp, []).append((key.split("_")[0], summary.split(":")[0], (x, y, z)))
    for m in re.finditer(r"RaidEntrance k\w+Entrance\s*=\s*\{\s*(\d+),\s*(" + NUM + r"),\s*(" + NUM + r"),\s*(" + NUM + r")", text):
        mp = int(m.group(1))
        x, y, z = (float(m.group(i).rstrip("f")) for i in (2, 3, 4))
        rows.setdefault(mp, []).append((RAID_KEYS.get(mp, "raid%d" % mp), "raid", (x, y, z)))
    return rows


# ---- bots' dungeon scripts ----

def script_for_map(mp):
    """(path, text, offset of the class's map_id) of the bots' dungeon script for a map."""
    for path in glob.glob(os.path.join(SCRIPTS, "*.cpp")):
        text = read(path)
        m = re.search(r"map_id\(\)\s*const\s*override\s*\{\s*return\s+%d\s*;" % mp, text)
        if m:
            return path, text, m.end()
    return None, None, 0


def classic_block(text, at):
    """The advice the Classic realm gets from the script class at `at`: its Classic advice function when it calls one, else its
    get_advice body."""
    g = text.find("get_advice(", at)
    if g < 0:
        return ""
    body = brace_block(text, g)
    m = re.search(r"if\s*\(\s*IsVanillaRealm\(\)\s*\)\s*return\s+(\w+)\(\)", body)
    if m:
        fn = re.search(r"DungeonAdvice\s+%s\s*\(\s*\)" % m.group(1), text)
        if fn:
            return brace_block(text, fn.end())
    return body


def list_after(block, name):
    m = re.search(r"\ba\.%s\s*=\s*\{" % name, block)
    return brace_block(block, m.start()) if m else None


def script_wings(block, names):
    """[(wing_entrance or None, [(entry, name, (x, y, z) or None)])]"""
    m = re.search(r"std::array<Wing,\s*\d+>\s*const\s+wings\s*=\s*\{", block)
    if m:
        wings = []
        raw = brace_block(block, m.end() - 1)
        for w in parse_nested(strip_comments(raw))[0]:
            ids, spots, ent = w[0], w[1], w[2]
            bosses = [(int(e), names.get(int(e), "boss %d" % int(e)), tuple(spots[i]) if i < len(spots) else None)
                      for i, e in enumerate(ids)]
            wings.append((tuple(ent), bosses))
        return wings

    ids_raw = list_after(block, "bosses")
    if not ids_raw:
        return []
    ids = [int(v) for v in parse_nested(strip_comments(ids_raw))]
    wp_raw = list_after(block, "progression_waypoints")
    spots = [tuple(p) for p in parse_nested(strip_comments(wp_raw))] if wp_raw else []
    # Index for index, as the bots read them: a boss past the last spot (a summoned final boss) has no spot of its own.
    return [(None, [(e, names.get(e, "boss %d" % e), spots[i] if i < len(spots) else None) for i, e in enumerate(ids)])]


# ---- Classic instance scripts and repository spawns ----

def instance_bosses():
    """map -> [(entry, name)] in encounter order, from the ClassicDungeonBossEntry arrays."""
    out = {}
    for path in glob.glob(os.path.join(INSTANCES, "**", "*.cpp"), recursive=True) + \
            glob.glob(os.path.join(INSTANCES, "**", "*.h"), recursive=True):
        text = read(path)
        arr = re.search(r"ClassicDungeonBossEntry\s+\w+\[\]\s*=\s*\{", text)
        if not arr:
            continue
        mp = None
        m = re.search(r"InstanceMapScript\([^,]+,\s*(\w+)\)", text)
        if m:
            tok = m.group(1)
            if tok.isdigit():
                mp = int(tok)
            else:
                for src in [text] + [read(h) for h in glob.glob(os.path.join(os.path.dirname(path), "*.h"))]:
                    d = re.search(r"\b%s\s*=\s*(\d+)" % tok, src)
                    if d:
                        mp = int(d.group(1))
                        break
        if mp is None:
            continue
        names = comment_names(brace_block(text, arr.end() - 1))
        ids = re.findall(r"\{\s*\w+\s*,\s*(\d+)\s*\}", brace_block(text, arr.end() - 1))
        out[mp] = [(int(e), names.get(int(e), "boss %s" % e)) for e in ids]
    return out


def repo_spawns():
    """(entry, map) -> (x, y, z) of the first creature row in sql/custom/world."""
    out = {}
    for path in sorted(glob.glob(os.path.join(WORLD_SQL, "*.sql"))):
        text = read(path)
        for ins in re.finditer(r"INSERT INTO `creature` \(([^)]*)\) VALUES", text):
            cols = [c.strip(" `") for c in ins.group(1).split(",")]
            try:
                ci, cm, cx, cy, cz = (cols.index(c) for c in ("id", "map", "position_x", "position_y", "position_z"))
            except ValueError:
                continue
            end = text.find(";", ins.end())
            for row in re.finditer(r"\(([^()]*)\)", text[ins.end():end]):
                f = [v.strip() for v in row.group(1).split(",")]
                if len(f) != len(cols):
                    continue
                try:
                    key = (int(f[ci]), int(f[cm]))
                    out.setdefault(key, (float(f[cx]), float(f[cy]), float(f[cz])))
                except ValueError:
                    continue
    return out


def db_spawns(path):
    out = {}
    with open(path, encoding="utf-8-sig") as f:
        for line in f:
            p = line.split()
            if len(p) >= 5 and p[0].isdigit():
                out.setdefault((int(p[0]), int(p[1])), (float(p[2]), float(p[3]), float(p[4])))
    return out


def nearest_wing(pos, wings):
    best, bd = 0, 1e30
    for i, (ent, _) in enumerate(wings):
        d = (ent[0] - pos[0]) ** 2 + (ent[1] - pos[1]) ** 2
        if d < bd:
            best, bd = i, d
    return best


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--spawns", help="tab-separated entry, map, x, y, z from the world database")
    ap.add_argument("--map", type=int, action="append", help="only this map (repeat for more)")
    args = ap.parse_args()

    ents = entrances()
    inst = instance_bosses()
    repo = repo_spawns()
    db = db_spawns(args.spawns) if args.spawns else {}
    # Boss names: the Classic instance scripts first (one boss per line there), then the bots' scripts.
    names = {}
    for lst in inst.values():
        for e, n in lst:
            if not n.startswith("boss "):
                names.setdefault(e, n)
    for path in sorted(glob.glob(os.path.join(SCRIPTS, "*.cpp"))):
        for k, v in comment_names(read(path)).items():
            names.setdefault(k, v)

    out = ["# dungeon_reach_check list, written by make_reach_list.py%s" % (" with world database spawns" if db else ""),
           "# dungeon <map> <key> <name> / start <x> <y> <z> / boss <entry> <x> <y> <z> <name> / missing <entry> <name>"]
    for mp in sorted(ents):
        if args.map and mp not in args.map:
            continue
        rows = ents[mp]
        path, text, at = script_for_map(mp)
        wings = []
        source = ""
        if text:
            wings = script_wings(classic_block(text, at), names)
            source = os.path.basename(path)
        if not wings and mp in inst:
            wings = [(None, [(e, n, None) for e, n in inst[mp]])]
            source = "Classic instance script"

        # One dungeon block per scenario entrance row; the bosses go to their wing (script wings) or to the nearest entrance.
        blocks = [(key, title, ent, []) for key, title, ent in rows]
        for went, bosses in wings:
            for e, n, spot in bosses:
                if db:
                    spot = db.get((e, mp), spot)
                elif spot is None:
                    spot = repo.get((e, mp))
                anchor = went or spot
                idx = nearest_wing(anchor, [(b[2], None) for b in blocks]) if anchor and len(blocks) > 1 else 0
                blocks[idx][3].append((e, n, spot))

        for key, title, ent, bosses in blocks:
            out.append("")
            out.append("dungeon %d %s %s   # bosses from %s" % (mp, key, title, source or "nowhere"))
            out.append("start %.2f %.2f %.2f" % ent)
            if not bosses:
                out.append("# no bosses known for this map")
            for e, n, spot in bosses:
                if spot:
                    out.append("boss %d %.2f %.2f %.2f %s" % (e, spot[0], spot[1], spot[2], n))
                else:
                    out.append("missing %d %s" % (e, n))
    sys.stdout.write("\n".join(out) + "\n")


if __name__ == "__main__":
    main()
