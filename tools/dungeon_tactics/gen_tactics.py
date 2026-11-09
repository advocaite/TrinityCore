#!/usr/bin/env python3
# Dungeon tactics tables for the bots, generated from VMaNGOS data.
#   python3 -I gen_tactics.py --batch 1            writes the C++ data files and the per-dungeon docs
#   python3 -I gen_tactics.py --batch 1 --check    prints the spells that have no entry in spell_facts.py and exits 1 if any matter
# Sources (see vmdata.py): creature_template + spawns of the VMaNGOS 2 Oct 2017 dump, the creature AI Forever runs (converted VMaNGOS
# EventAI / spell lists in 2026_09_28_15_world_vanilla_smartai.sql), vanilla damage / heal values of classic_spell_value, and
# spell_facts.py / boss_manual.py for what a spell id means and for the bosses whose casts sit in a C++ script.
import argparse, collections, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmdata, spell_facts, boss_manual, dungeons

REPO = vmdata.REPO
CPP_DIR = os.path.join(REPO, 'src/modules/PlayerbotV2/Bot/Dungeon/Tactics')
DOC_DIR = os.path.join(REPO, 'doc/dungeon_tactics')

SCHOOL = {'phys': 1, 'holy': 2, 'fire': 4, 'nature': 8, 'frost': 16, 'shadow': 32, 'arcane': 64}
TYPE_NAME = {0: 'none', 1: 'beast', 2: 'dragonkin', 3: 'demon', 4: 'elemental', 5: 'giant', 6: 'undead', 7: 'humanoid', 8: 'critter',
             9: 'mechanical', 10: 'not specified', 11: 'totem'}
MECH = {1: 'charm', 2: 'disorient', 3: 'disarm', 4: 'distract', 5: 'fear', 6: 'fumble', 7: 'root', 8: 'pacify', 9: 'silence', 10: 'sleep',
        11: 'snare', 12: 'stun', 13: 'freeze', 14: 'knockout', 15: 'bleed', 16: 'bandage', 17: 'polymorph', 18: 'banish', 19: 'shield',
        20: 'shackle', 21: 'mount', 22: 'infected', 23: 'turn', 24: 'horror', 25: 'invulnerability', 26: 'interrupt', 27: 'daze',
        28: 'discovery', 29: 'immune shield', 30: 'sap'}
SCHOOL_NAMES = ['physical', 'holy', 'fire', 'nature', 'frost', 'shadow', 'arcane']
TGT = {1: 'Self', 2: 'Victim', 5: 'Random', 6: 'RandomNotTop', 7: 'Invoker', 9: 'Friendly', 3: 'Random', 4: 'Random', 8: 'Position'}
WHEN = {'combat': 'Combat', 'ooc': 'Ooc', 'aggro': 'Aggro', 'spawn': 'Spawn', 'death': 'Death', 'evade': 'Evade', 'ally': 'AllyUnbuffed'}
CASTED = re.compile(r'bolt|fireball|smite|heal|flamestrike|blizzard|immolate|mana burn|holy light|chain|summon|fear|sleep|polymorph|'
                    r'dominate|resurrect|siphon|call of the grave|void|mind blast|shock$|spike|drain|conflagrate|pyroblast|arcane missiles', re.I)

def guess_school(name):
    n = name.lower()
    if re.search(r'fire|flame|immolate|pyro|burn|molten|scorch|melt|ember', n) and 'mana burn' not in n: return 'fire'
    if re.search(r'frost|ice|blizzard|cone of cold|chill', n): return 'frost'
    if re.search(r'shadow|curse|mana burn|siphon|drain|call of the grave|void|mind|coil|death|dark', n): return 'shadow'
    if re.search(r'lightning|chain|earth|poison|toxic|nature|thunder|thorn|venom|nova' , n): return 'nature' if 'fire nova' not in n and 'frost nova' not in n else None
    if re.search(r'holy|smite|heal|renew|crusader|hammer|exorcism|consecrat', n): return 'holy'
    if re.search(r'arcane|mana', n): return 'arcane'
    return None

NAME_TAGS = [   # keyword in a spell name -> tags; only used for spells that have no entry in spell_facts.py (flagged Unverified)
    (r'heal|renew|regrowth|rejuvenat|restor', 'heal'), (r'sleep|slumber', 'sleep'), (r'fear|terrify|scare|panic|horrify|scream', 'fear'),
    (r'stun|bash|hammer of justice|concuss|stomp|knock ?down', 'stun'), (r'root|entangl|frost nova|net$|ensnare|web|vines|chains of ice', 'root'),
    (r'slow|hamstring|crippl|wing clip|tendon|daze', 'snare'), (r'silence', 'silence'), (r'dominate|mind control|charm|enslave|possess', 'charm'),
    (r'summon|call (of|for)|raise|conjure', 'summon'), (r'ward|shield|barrier|protection', 'shield buff'), (r'enrage|frenzy|rage|berserk|enlarge', 'enrage'),
    (r'curse', 'curse debuff'), (r'disarm', 'disarm'), (r'mana burn|drain mana|siphon', 'drain'), (r'stealth|vanish', 'stealth'),
    (r'charge|leap|harpoon', 'charge'), (r'nova|explosion|volley|blizzard|rain of|flamestrike|trample|shockwave|thunderclap|howl|cone', 'aoe'),
    (r'sunder|rend|weak|expose|armor|befuddl|vulnerab|distract|poison|venom|toxin|disease|plague|infect', 'debuff'),
    (r'battle shout|command|aura|stance|fortitude|thorns|mark of|arcane intellect|inner fire|blessing', 'buff'),
]
INFER_DISPEL = [(r'curse', 'curse'), (r'poison|venom|toxin', 'poison'), (r'disease|plague|infect', 'disease')]

def infer_tags(name):
    tags = set()
    for pat, t in NAME_TAGS:
        if re.search(pat, name, re.I): tags.update(t.split())
    return tags

def infer_dispel(name):
    for pat, d in INFER_DISPEL:
        if re.search(pat, name, re.I): return d
    return None

class Spells:
    """What the dump and spell_facts.py know about each spell id."""
    def __init__(self):
        self.ai = vmdata.load_ai_comment_names()
        self.vals = collections.defaultdict(list)
        for r in vmdata.table_rows(vmdata.SPELLVAL, 'classic_spell_value'):
            self.vals[r['spell_id']].append(r)
    def ai_name(self, sid):
        c = self.ai.get(sid)
        if not c: return ''
        return re.sub(r'\s*\(.*?\)\s*$', '', c.most_common(1)[0][0]).strip()
    def confirmed(self, sid):
        """the EventAI comments of the VMaNGOS dump call the spell what spell_facts.py does"""
        fct = spell_facts.F.get(sid)
        return bool(fct) and any(fct['name'].lower() == re.sub(r'\s*\(.*?\)\s*$', '', n).strip().lower() for n in self.ai.get(sid, {}))
    def name(self, sid):
        fct = spell_facts.F.get(sid)
        if fct: return fct['name']
        for r in self.vals.get(sid, []): return r['spell_name']
        return self.ai_name(sid)
    def direct(self, sid):
        """mean direct damage / heal of the first damage or heal effect, and which kind"""
        for r in self.vals.get(sid, []):
            if r['effect'] in (2, 10) or (r['effect'] == 9):
                return r['effect'], r['base_points'] + (max(r['die_sides'], 1) + 1) / 2.0
        return None, 0
    def periodic(self, sid):
        for r in self.vals.get(sid, []):
            if r['effect'] in (6, 27) and r['apply_aura'] in (3, 53):
                return 'dot', r['base_points'] + (max(r['die_sides'], 1) + 1) / 2.0
            if r['effect'] == 6 and r['apply_aura'] == 8:
                return 'hot', r['base_points'] + (max(r['die_sides'], 1) + 1) / 2.0
            if r['effect'] == 6 and r['apply_aura'] == 69:
                return 'absorb', r['base_points']
        return None, 0
    def ground(self, sid):
        return any(r['effect'] == 27 for r in self.vals.get(sid, []))

def squishy_hp(level):
    return 50 + 30 * level

def timing(r):
    et = r['event_type']; p = [r['event_param%d' % i] for i in range(1, 5)]
    t = dict(first=(0, 0), repeat=(0, 0), hp=(100, 0), tgt_hp=None, rng=(0, 0), when='combat')
    if et == 0:
        t['first'] = (p[0], p[1]); t['repeat'] = (p[2], p[3])
    elif et == 9:
        t['rng'] = (p[0], p[1]) if max(p[0], p[1]) <= 255 else (0, 0); t['repeat'] = (p[2], p[3])   # a range gate is yards; a bigger number is not one
    elif et in (74, 14):
        t['tgt_hp'] = (p[1], p[0]); t['repeat'] = (p[2], p[3])
    elif et == 2:
        t['hp'] = (p[1], p[0]); t['repeat'] = (p[2], p[3])
    elif et == 1:
        t['when'] = 'ooc'; t['first'] = (p[0], p[1]); t['repeat'] = (p[2], p[3])
    elif et == 4: t['when'] = 'aggro'
    elif et == 63: t['when'] = 'spawn'
    elif et == 6: t['when'] = 'death'
    elif et == 7: t['when'] = 'evade'
    elif et == 23: t['when'] = 'ooc'; t['repeat'] = (p[2], p[3])
    elif et == 16: t['when'] = 'ally'; t['tgt_hp'] = None; t['repeat'] = (p[2], p[3])
    return t

def extract(entry, scripts, depth=0):
    """cast / summon / flee rows of a creature's smart script (and the action lists it calls)"""
    out = dict(casts=[], summons=[], flee=0, scripted_calls=0)
    rows = scripts.get((0, entry), []) if depth == 0 else scripts.get((9, entry), [])
    parent = {r['link']: r for r in rows if r['link']}          # a LINK row (event 61) runs with the row that points at it
    for r in rows:
        a = r['action_type']
        tr = parent.get(r['id'], r) if r['event_type'] == 61 else r
        if a in (11, 85, 86):
            t = timing(tr) if depth == 0 else dict(first=(0, 0), repeat=(0, 0), hp=(100, 0), tgt_hp=None, rng=(0, 0), when='combat')
            t.update(spell=r['action_param1'], flags=r['action_param2'], target=r['target_type'], scripted=depth > 0)
            out['casts'].append(t)
        elif a == 12:
            out['summons'].append(dict(entry=r['action_param1'], when=timing(tr)['when'] if depth == 0 else 'combat'))
        elif a == 25 and tr['event_type'] == 2:
            out['flee'] = max(out['flee'], tr['event_param2'])
        elif a == 80 and depth == 0:
            out['scripted_calls'] += 1
            sub = extract(r['action_param1'], scripts, 1)
            out['casts'] += sub['casts']; out['summons'] += sub['summons']
    return out

def cc_mask(t):
    ty, mi, rank = t['type'], t['MechanicImmuneMask'], t['rank']
    if rank >= 3: return 0
    def free(mech): return not (mi >> (mech - 1)) & 1
    m = 0
    if ty in (7, 1, 8) and free(17): m |= 1          # Polymorph (humanoid, beast, critter)
    if ty == 7 and free(30): m |= 2                  # Sap (humanoid, out of combat)
    if free(13) and ty not in (11,): m |= 4          # Freezing Trap
    if ty == 6 and free(20): m |= 8                  # Shackle Undead
    if ty in (3, 4) and free(18): m |= 16            # Banish (demon, elemental)
    if ty in (1, 2) and free(10): m |= 32            # Hibernate (beast, dragonkin)
    if ty == 1 and free(5): m |= 64                  # Scare Beast
    return m

def decode_mask(mask, table):
    return [table[i + 1] for i in range(32) if (mask >> i) & 1 and (i + 1) in table]

def build_mob(entry, tpl, count, scripts, S, boss_idx):
    t = tpl[entry]
    ex = extract(entry, scripts)
    manual = boss_manual.M.get(entry)
    if manual:
        for row in manual['rows']:
            tg = {'self': 1, 'victim': 2, 'random': 5, 'friendly': 9}[row['target']]
            ex['casts'].append(dict(first=row['first'], repeat=row['repeat'], hp=row['hp'], tgt_hp=None, rng=(0, 0), when='combat',
                                    spell=row['spell'], flags=0, target=tg, scripted=True, note=row.get('note', '')))
    spells = []
    level = t['maxlevel']
    for c in ex['casts']:
        sid = c['spell']
        fct = spell_facts.F.get(sid)
        name = S.name(sid)
        kind, dmg = S.direct(sid)
        pkind, pval = S.periodic(sid)
        tags = set(fct['tags']) if fct else set()
        conf = (('c' if S.confirmed(sid) else fct['conf']) if fct else 'd' if S.vals.get(sid) else 'a' if name else '-')
        if not fct and name and not S.vals.get(sid):
            tags |= infer_tags(name)                      # only a name from the EventAI comments: tags guessed from it
        school = SCHOOL.get(fct['school']) if fct else None
        if school is None:
            g = guess_school(name) if name else None
            school = SCHOOL.get(g, 0) if g else 0
        cast = fct['cast'] if fct else None
        dispel = fct['dispel'] if fct else (infer_dispel(name) if conf == 'a' else None)
        tgt = TGT.get(c['target'], 'Other')
        if conf == 'a' and tgt in ('Self', 'Friendly'):
            if tags & {'debuff', 'curse'}: tags -= {'debuff', 'curse'}; tags.add('buff')
        ranged = bool(c['flags'] & 64) or c['rng'][0] >= 5
        if kind == 10: tags.add('heal')
        elif kind == 2 and not fct: tags.add('dmg')
        if pkind == 'dot': tags.add('dot')
        if pkind == 'hot': tags.add('hot')
        if pkind == 'absorb': tags.add('shield')
        if S.ground(sid): tags.update(('aoe', 'ground'))
        if c['target'] == 9 and kind == 10: tags.add('heal')
        if tgt == 'Self' and kind == 2 and 'aoe' not in tags: tags.add('aoe')       # damage cast on itself = a nova around the caster
        castable = (cast is not None and cast > 0) or (cast is None and (ranged and kind == 2 or (name and CASTED.search(name))))
        if castable and 'dmg' in tags and 'aoe' not in tags: tags.add('nuke')
        sev = 0
        hp_frac = dmg / squishy_hp(level) if kind == 2 else 0
        if kind == 2:
            sev = 3 if hp_frac >= 0.20 else 2 if hp_frac >= 0.10 else 1 if hp_frac >= 0.04 else 0
            if 'nuke' in tags and c['repeat'][0] and c['repeat'][0] <= 4000 and hp_frac >= 0.06: sev = max(sev, 2)
        if tags & {'fear', 'charm', 'sleep'}: sev = max(sev, 3 if castable else 2)
        if tags & {'stun', 'silence'}: sev = max(sev, 2)
        if tags & {'root', 'snare', 'drain', 'summon'}: sev = max(sev, 2 if (castable or 'summon' in tags) else 1)
        if 'heal' in tags and tgt == 'Friendly': sev = max(sev, 3)
        if tags & {'shield', 'enrage', 'immune'}: sev = max(sev, 2)
        if tags & {'curse', 'dot', 'debuff'} and tgt in ('Victim', 'Random', 'RandomNotTop'): sev = max(sev, 1)
        counter = 0
        if castable and tags & {'heal', 'fear', 'charm', 'sleep', 'summon', 'nuke', 'drain', 'root', 'aoe', 'stun', 'silence'} and sev >= 1:
            counter |= 1                                      # Interrupt
        if dispel and tgt != 'Self' and tgt != 'Friendly' and tags & {'fear', 'sleep', 'root', 'snare', 'dot', 'silence', 'charm', 'curse', 'debuff', 'drain'} and sev >= 1:
            counter |= 2                                      # Dispel (the victim cleanses it)
        if dispel in ('magic',) and (tgt in ('Self', 'Friendly') or c['when'] in ('ooc', 'spawn', 'aggro')) and tags & {'shield', 'enrage', 'immune'}:
            counter |= 4                                      # Purge (take it off the mob)
        if tags & {'aoe', 'ground'} and 'aoe' in tags and sev >= 1: counter |= 8   # StepOut
        if 'immune' in tags: counter |= 16                    # StopDamage (hold DPS while it lasts)
        spells.append(dict(spell=sid, name=name, school=school, cast=cast, tags=tags, dispel=dispel, tgt=tgt, when=c['when'],
                           first=c['first'], repeat=c['repeat'], hp=c['hp'], tgt_hp=c['tgt_hp'], rng=c['rng'], ranged=ranged,
                           dmg=int(round(dmg)) if kind == 2 else 0, heal=int(round(dmg)) if kind == 10 else 0,
                           tick=int(round(pval)) if pkind == 'dot' else 0, sev=sev, counter=counter, hold=(fct['hold'] if fct else 0), conf=conf,
                           scripted=c['scripted'], note=c.get('note', ''), castable=castable))
    # out-of-combat self buffs (Frost Armor...) are not a combat tactic; one row per spell, target and moment
    seen, kept = set(), []
    for sp in spells:
        if sp['when'] in ('ooc',) and sp['sev'] < 2: continue
        k = (sp['spell'], sp['tgt'], sp['when'], sp['hp'])
        if k in seen: continue
        seen.add(k); kept.append(sp)
    spells = kept
    flags = 0
    heals_allies = any('heal' in s['tags'] and s['tgt'] == 'Friendly' for s in spells)
    summoner = bool(ex['summons']) or any('summon' in s['tags'] and s['when'] == 'combat' for s in spells)
    has_caster_nuke = any('nuke' in s['tags'] and s['sev'] >= 1 for s in spells)
    hits_hard = any(s['castable'] and s['tags'] & {'nuke', 'aoe'} and s['sev'] >= 2 and s['tgt'] != 'Self' or ('aoe' in s['tags'] and 'ground' in s['tags'] and s['sev'] >= 2) for s in spells)
    buffs_allies = any(s['tgt'] in ('Friendly', 'Invoker') and 'heal' not in s['tags'] for s in spells) or any(s['spell'] == 5115 for s in spells)
    drains = any('drain' in s['tags'] for s in spells)
    ccs_players = any(s['tags'] & {'fear', 'charm', 'sleep'} for s in spells)
    prio = 1
    if heals_allies: prio = 5
    elif ccs_players: prio = 5
    elif summoner and entry not in boss_idx: prio = 4
    elif hits_hard: prio = 4
    elif buffs_allies or drains: prio = 3
    elif has_caster_nuke or t['unit_class'] == 8: prio = 2
    is_boss = entry in boss_idx
    if is_boss: prio = 1
    if t['rank'] == 0 and not spells: prio = 0
    mask = cc_mask(t)
    cc_priority = (not is_boss) and bool(mask & 0b111111) and (heals_allies or ccs_players or hits_hard or summoner or drains)
    return dict(entry=entry, name=t['name'], lvl=(t['minlevel'], t['maxlevel']), rank=t['rank'], type=t['type'], cls=t['unit_class'],
                spawns=count, mech_imm=t['MechanicImmuneMask'], school_imm=t['SchoolImmuneMask'], flee=ex['flee'],
                heals_allies=heals_allies, summoner=summoner, caster=has_caster_nuke or t['unit_class'] == 8 and bool(spells), buffer=buffs_allies,
                boss=is_boss, cpp=bool(t['ScriptName']) and not spells and not manual, prio=prio, cc=mask, cc_priority=cc_priority,
                summons=ex['summons'], spells=spells, scripted_calls=ex['scripted_calls'])

def keep(t, count, built):
    """creatures worth a row: hostile fighters. Critters, prisoners, quest givers and bunnies are not."""
    if t['type'] in (8, 11) or t['npcflag']: return False
    if t['faction_A'] in (35, 12, 11, 79, 80, 190) and not built['spells']: return False
    if t['name'].lower().startswith(('spawn point', 'waypoint')): return False
    return built['spells'] or t['rank'] >= 1 or built['boss']

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--batch', type=int, required=True)
    ap.add_argument('--check', action='store_true')
    args = ap.parse_args()
    tpl = vmdata.load_templates(); spawns = vmdata.load_spawns(); scripts = vmdata.load_smart(); S = Spells()
    result = {}
    missing = collections.defaultdict(set)
    for d in dungeons.BATCHES[args.batch]:
        boss_idx = {e: i for i, e in enumerate(d['bosses'])}
        mobs = []
        for entry, count in sorted(spawns[d['map']].items()):
            t = tpl.get(entry)
            if not t: continue
            b = build_mob(entry, tpl, count, scripts, S, boss_idx)
            if not keep(t, count, b): continue
            for s in b['spells']:
                if s['spell'] not in spell_facts.F and s['sev'] >= 1 and not s['name']:
                    missing[d['key']].add((s['spell'], b['name']))
            mobs.append(b)
        for e in d['bosses']:
            if e not in {m['entry'] for m in mobs} and e in tpl:
                b = build_mob(e, tpl, spawns[d['map']].get(e, 0), scripts, S, boss_idx); mobs.append(b)   # event-spawned boss
        result[d['key']] = (d, mobs)
    if args.check:
        for k, v in missing.items():
            print(k, sorted(v))
        return 1 if missing else 0
    import emit
    emit.write(result, CPP_DIR, DOC_DIR, S)
    return 0

if __name__ == '__main__':
    sys.exit(main())
