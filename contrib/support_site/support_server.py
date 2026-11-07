"""Forever support site: the page the in-game Support window (Help) opens.

The worldserver answers the client's SSO token request (CMSG_GENERATE_SSO_TOKEN) with a token stored in
auth.battlenet_sso_tokens. The in-game browser then opens <sso url>?token=<token>&ref=<page>; /s, /sso and /login/sso
look the token up, start a session and show the support pages: unstuck, bug reports, help tickets, FAQ, staff view.
The look (static/theme.css, static/app.js) is shared with the planned GM portal.

Usage:
  py -3 support_server.py [config.json]                      run the site (default config: support_site.json next to this file)
  py -3 support_server.py --make-token <bnetAccountId> <realmId> [config.json]
                                                              issue a 1 hour test token and print the URL (desktop browser test)
Account page (/account): Battle.net account, game accounts, characters on every realm, and the authenticator (TOTP, any
authenticator app). bnetserver asks for its code at login; secured accounts get the +4 backpack slots in game.
Shop (/shop): shop points, premium status, selling gold on the Gold Market (premium accounts; ".shop goldlist" through RA).
Shop admin (/staff/shop, staff level >= shop_admin_level): products and tabs of the in-game Store, Gold Market listings,
accounts (points, premium, Gold Market bans), the service fee. Product changes are pushed with ".shop reload" on every realm.

Needs: pip install pymysql segno
"""
import base64
import hashlib
import hmac
import html
import json
import os
import secrets
import socket
import ssl
import sys
import threading
import time
import traceback
from http.cookies import SimpleCookie
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlencode, urlsplit

import pymysql

try:
    import segno                # QR code of the authenticator key (pip install segno); without it only the key is shown
except ImportError:
    segno = None

HERE = os.path.dirname(os.path.abspath(__file__))
STATIC_DIR = os.path.join(HERE, 'static')
# cache buster for /static links: newest file time in static/
STATIC_VERSION = str(int(max(os.path.getmtime(os.path.join(STATIC_DIR, f)) for f in os.listdir(STATIC_DIR))))
STATIC_TYPES = {'.css': 'text/css; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.svg': 'image/svg+xml', '.png': 'image/png'}

RACES = {1: 'Human', 2: 'Orc', 3: 'Dwarf', 4: 'Night Elf', 5: 'Undead', 6: 'Tauren', 7: 'Gnome', 8: 'Troll'}
HORDE_RACES = {2, 5, 6, 8}
CLASSES = {1: 'Warrior', 2: 'Paladin', 3: 'Hunter', 4: 'Rogue', 5: 'Priest', 7: 'Shaman', 8: 'Mage', 9: 'Warlock', 11: 'Druid'}
CATEGORIES = {
    'bug': 'Bug report',
    'stuck': 'Stuck',
    'account': 'Account or character',
    'player': 'Report a player',
    'other': 'Something else',
}
STATUS = {'open': ('Waiting for staff', 'amber'), 'answered': ('Staff replied', 'green'), 'closed': ('Closed', '')}
MAX_OPEN_TICKETS = 10
SUBJECT_MAX = 120
MESSAGE_MAX = 4000

# line icons (24x24, stroke)
ICONS = {
    'home': '<path d="M3 10.5 12 3l9 7.5"/><path d="M5 9.5V20a1 1 0 0 0 1 1h4v-6h4v6h4a1 1 0 0 0 1-1V9.5"/>',
    'pin': '<path d="M20 10c0 6-8 12-8 12s-8-6-8-12a8 8 0 0 1 16 0Z"/><circle cx="12" cy="10" r="3"/>',
    'bug': '<rect x="8" y="6" width="8" height="14" rx="4"/><path d="M12 20v-8M8 13H4M20 13h-4M8 17.5 5 19.5M16 17.5l3 2M8 9.5 5 7.5M16 9.5l3-2M9.5 6.5 8 3.5M14.5 6.5 16 3.5"/>',
    'help': '<circle cx="12" cy="12" r="9.5"/><path d="M9.2 9.2a2.9 2.9 0 0 1 5.6 1c0 2-2.8 2.6-2.8 4.2"/><path d="M12 17.6h.01"/>',
    'tickets': '<path d="M20.5 14.5a2 2 0 0 1-2 2H8l-4.5 4V5.5a2 2 0 0 1 2-2h13a2 2 0 0 1 2 2Z"/><path d="M8 9h8M8 12.5h5"/>',
    'book': '<path d="M2.5 4h6a3.5 3.5 0 0 1 3.5 3.5V21a2.5 2.5 0 0 0-2.5-2.5h-7Z"/><path d="M21.5 4h-6A3.5 3.5 0 0 0 12 7.5V21a2.5 2.5 0 0 1 2.5-2.5h7Z"/>',
    'shield': '<path d="M12 21.5s7.5-3.6 7.5-9.5V5.2L12 2.5 4.5 5.2V12c0 5.9 7.5 9.5 7.5 9.5Z"/>',
    'sun': '<circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/>',
    'check': '<circle cx="12" cy="12" r="9.5"/><path d="m8 12.3 2.7 2.7L16 9.7"/>',
    'alert': '<circle cx="12" cy="12" r="9.5"/><path d="M12 7.5v5.5M12 16.5h.01"/>',
    'chevron': '<path d="m6 9 6 6 6-6"/>',
    'send': '<path d="M21.5 2.5 10.5 13.5"/><path d="m21.5 2.5-7 19-4-8.5-8.5-4Z"/>',
    'user': '<circle cx="12" cy="8" r="4"/><path d="M4.5 21a7.5 7.5 0 0 1 15 0"/>',
    'plus': '<path d="M12 5v14M5 12h14"/>',
    'x': '<path d="M18 6 6 18M6 6l12 12"/>',
    'undo': '<path d="M3 7v6h6"/><path d="M3.5 13A9 9 0 1 0 6 6.3L3 9"/>',
    'search': '<circle cx="11" cy="11" r="7"/><path d="m20.5 20.5-4.5-4.5"/>',
    'lock': '<rect x="4.5" y="10.5" width="15" height="10.5" rx="2"/><path d="M8 10.5V7.5a4 4 0 0 1 8 0v3"/>',
    'coin': '<circle cx="12" cy="12" r="9.5"/><path d="M14.8 8.6c-.6-.8-1.6-1.2-2.8-1.2-1.7 0-2.9.9-2.9 2.2 0 3 6 1.6 6 4.6 0 1.3-1.3 2.3-3.1 2.3-1.3 0-2.4-.5-3-1.4M12 5.5v2M12 16.5v2"/>',
    'store': '<path d="M3.5 9 5 4h14l1.5 5"/><path d="M3.5 9a2.8 2.8 0 0 0 5.7 0 2.8 2.8 0 0 0 5.6 0 2.8 2.8 0 0 0 5.7 0"/><path d="M5 11.5V20h14v-8.5"/><path d="M10 20v-5h4v5"/>',
}

# ---------------------------------------------------------------- in-game Store (worldserver TokenHandler.cpp, tables world.shop_*,
# auth.account_shop_points / shop_config, characters.shop_gold_listing; ".shop" console commands through RA)

SHOP_ACTIONS = {1: 'Item', 2: 'Gold', 3: 'Spell / mount', 4: 'Service', 5: 'Profession', 6: 'Level', 7: 'Title', 8: 'Reputation', 9: 'Realm transfer', 10: 'Character boost token'}
SHOP_ACTION_HINTS = {     # labels of the two values in the product editor ('' = not used)
    1: ('Item ID', 'Amount'), 2: ('Gold', ''), 3: ('Spell ID (mount, pet, ...)', ''), 4: ('Service', ''),
    5: ('Profession', 'Skill level'), 6: ('Level', ''), 7: ('Title ID', ''), 8: ('Faction ID', 'Standing (42000 = exalted)'), 9: ('', ''), 10: ('Boost type (6 = level 60)', ''),
}
SHOP_SERVICES = {1: 'Name change', 2: 'Appearance change', 3: 'Race change', 4: 'Faction change'}
PROFESSIONS = {171: 'Alchemy', 164: 'Blacksmithing', 333: 'Enchanting', 202: 'Engineering', 182: 'Herbalism', 165: 'Leatherworking',
               186: 'Mining', 393: 'Skinning', 197: 'Tailoring', 185: 'Cooking', 129: 'First Aid', 356: 'Fishing'}
# "new product" presets of the GM shop page: (label, group hint, name, description, icon, [(action, param1, param2)])
SHOP_PRESETS = {
    'item': ('Item', 'Item', '', 134400, [(1, 0, 1)]),
    'gold': ('Gold', '100 Gold', 'Gold for this character.', 133784, [(2, 1000000, 0)]),
    'rename': ('Name change', 'Name Change', 'Change this character\'s name on the character screen.', 136243, [(4, 1, 0)]),
    'customize': ('Appearance change', 'Appearance Change', 'Change this character\'s appearance on the character screen.', 136243, [(4, 2, 0)]),
    'race': ('Race change', 'Race Change', 'Change this character\'s race on the character screen.', 136243, [(4, 3, 0)]),
    'faction': ('Faction change', 'Faction Change', 'Change this character\'s faction on the character screen.', 136243, [(4, 4, 0)]),
    'level': ('Level boost', 'Level 60', 'Raise this character to level 60. Visit your class trainer for new spells.', 136243, [(6, 60, 0)]),
    'profession': ('Max profession', 'Max Alchemy', 'Alchemy at 300 with every recipe.', 136240, [(5, 171, 300)]),
    'mount': ('Mount / spell', 'Mount', 'A mount for this character.', 132261, [(3, 0, 0)]),
    'title': ('Title', 'Title', 'A title for this character.', 133784, [(7, 0, 0)]),
    'reputation': ('Reputation', 'Exalted', 'Exalted with a faction.', 133784, [(8, 0, 42000)]),
    'boost': ('Character boost', 'Level 60 Boost', 'A level 60 boost for any character, used on the character screen.', 136243, [(10, 6, 0)]),
}
GOLD_MARKET_MAX_GOLD = 200000
GOLD_MARKET_MIN_PRICE = 2


def shop_account(account_id):
    row = query_one(auth_db(), 'SELECT Points, Premium, GoldMarketBanned, GoldMarketBanReason FROM account_shop_points WHERE AccountID = %s', (account_id,))
    return row or {'Points': 0, 'Premium': 0, 'GoldMarketBanned': 0, 'GoldMarketBanReason': ''}


def shop_setting(name, default):
    row = query_one(auth_db(), 'SELECT Value FROM shop_config WHERE Name = %s', (name,))
    return row['Value'] if row else default


def gold_fee_percent():
    try:
        return max(0, min(100, int(shop_setting('GoldMarketFee', '10'))))
    except ValueError:
        return 10


def gold_fee(price, percent):
    """Same rule as the worldserver (TokenHandler.cpp): percent of the price, at least 1 point while the percent is not 0."""
    fee = price * percent // 100
    return max(fee, 1) if percent else 0


def realm_online(realm_id):
    """realmlist.flag 0x2 = offline (bnetserver keeps it up to date); a refused connection costs 2 s on Windows, so skip those."""
    row = query_one(auth_db(), 'SELECT flag FROM realmlist WHERE id = %s', (realm_id,))
    return bool(row) and not (int(row['flag']) & 2)


def shop_reload_all():
    """The worldservers cache the products: ".shop reload" on every running realm, all at once. Returns the realms that failed
    (offline realms load the new products when they start)."""
    failed = []

    def reload(realm_id):
        try:
            if not ra_command(realm_id, 'shop reload').startswith('OK'):
                failed.append(realm_name(realm_id))
        except OSError:
            failed.append(realm_name(realm_id))

    threads = [threading.Thread(target=reload, args=(r,)) for r in CFG['realms'] if ra_available(r)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    return failed


def shop_action_text(action, p1, p2):
    if action == 1:
        return 'Item %d x%d' % (p1, p2)
    if action == 2:
        return '%s gold' % '{:,}'.format(p1 // 10000)
    if action == 3:
        return 'Spell %d' % p1
    if action == 4:
        return SHOP_SERVICES.get(p1, 'Service %d' % p1)
    if action == 5:
        return 'Max %s (%d)' % (PROFESSIONS.get(p1, 'skill %d' % p1), p2 or 300)
    if action == 6:
        return 'Level %d' % p1
    if action == 7:
        return 'Title %d' % p1
    if action == 8:
        return 'Reputation %d: %d' % (p1, p2)
    if action == 9:
        return 'Realm transfer'
    if action == 10:
        return 'Boost token (type %d)' % p1
    return 'Action %d' % action


def to_int(value, default=0):
    try:
        return int(str(value).strip())
    except (TypeError, ValueError):
        return default

# ---------------------------------------------------------------- authenticator (TOTP, RFC 6238: 6 digits, 30 s, SHA-1, like the
# Battle.net and Google authenticator apps); bnetserver checks the same secret with Trinity::Crypto::TOTP

TOTP_STEP = 30


def totp_new_secret():
    return base64.b32encode(secrets.token_bytes(20)).decode('ascii').rstrip('=')


def totp_code(secret, timestamp):
    key = base64.b32decode(secret + '=' * (-len(secret) % 8))
    digest = hmac.new(key, int(timestamp // TOTP_STEP).to_bytes(8, 'big'), hashlib.sha1).digest()
    offset = digest[-1] & 0x0F
    return '%06d' % ((int.from_bytes(digest[offset:offset + 4], 'big') & 0x7FFFFFFF) % 1000000)


def totp_check(secret, code):
    code = ''.join(ch for ch in code if ch.isdigit())
    now = time.time()
    return len(code) == 6 and any(hmac.compare_digest(totp_code(secret, now + d * TOTP_STEP), code) for d in (-1, 0, 1))


def totp_uri(secret, email):
    issuer = CFG.get('server_name', 'Forever')
    return 'otpauth://totp/%s:%s?%s' % (issuer.replace(':', ''), email.replace(':', ''),
                                        urlencode({'secret': secret, 'issuer': issuer, 'algorithm': 'SHA1', 'digits': 6, 'period': TOTP_STEP}))


def qr_svg(text):
    if not segno:
        return ''
    return segno.make(text, error='m', micro=False).svg_inline(scale=5, border=3, dark='#000000', light='#ffffff')

# ---------------------------------------------------------------- armory data

# equipment slot -> label; paper-doll columns like the game's character frame
SLOT_NAMES = {0: 'Head', 1: 'Neck', 2: 'Shoulder', 3: 'Shirt', 4: 'Chest', 5: 'Waist', 6: 'Legs', 7: 'Feet', 8: 'Wrist', 9: 'Hands',
              10: 'Finger', 11: 'Finger', 12: 'Trinket', 13: 'Trinket', 14: 'Back', 15: 'Main Hand', 16: 'Off Hand', 17: 'Ranged', 18: 'Tabard'}
DOLL_LEFT = [0, 1, 2, 14, 4, 3, 18, 8]
DOLL_RIGHT = [9, 5, 6, 7, 10, 11, 12, 13]
DOLL_BOTTOM = [15, 16, 17]
INVENTORY_TYPES = {1: 'Head', 2: 'Neck', 3: 'Shoulder', 4: 'Shirt', 5: 'Chest', 6: 'Waist', 7: 'Legs', 8: 'Feet', 9: 'Wrist', 10: 'Hands',
                   11: 'Finger', 12: 'Trinket', 13: 'One-Hand', 14: 'Off Hand', 15: 'Ranged', 16: 'Back', 17: 'Two-Hand', 18: 'Bag', 19: 'Tabard',
                   20: 'Chest', 21: 'Main Hand', 22: 'Off Hand', 23: 'Held In Off-hand', 25: 'Thrown', 26: 'Ranged', 28: 'Relic'}
WEAPON_TYPES = {0: 'Axe', 1: 'Axe', 2: 'Bow', 3: 'Gun', 4: 'Mace', 5: 'Mace', 6: 'Polearm', 7: 'Sword', 8: 'Sword', 10: 'Staff', 13: 'Fist Weapon',
                14: 'Miscellaneous', 15: 'Dagger', 16: 'Thrown', 18: 'Crossbow', 19: 'Wand', 20: 'Fishing Pole'}
ARMOR_TYPES = {1: 'Cloth', 2: 'Leather', 3: 'Mail', 4: 'Plate', 6: 'Shield', 7: 'Libram', 8: 'Idol', 9: 'Totem'}
BONDING = {1: 'Binds when picked up', 2: 'Binds when equipped', 3: 'Binds when used', 4: 'Quest Item'}
# ItemModType -> (text, primary); primary stats show as "+N Stamina", the rest as "Equip: ..." lines
STAT_TYPES = {0: ('Mana', True), 1: ('Health', True), 3: ('Agility', True), 4: ('Strength', True), 5: ('Intellect', True), 6: ('Spirit', True),
              7: ('Stamina', True), 12: ('defense rating', False), 13: ('dodge rating', False), 14: ('parry rating', False), 15: ('block rating', False),
              16: ('hit rating', False), 18: ('spell hit rating', False), 19: ('critical strike rating', False), 21: ('spell critical rating', False),
              31: ('hit rating', False), 32: ('critical strike rating', False), 35: ('resilience rating', False), 36: ('haste rating', False),
              37: ('expertise rating', False), 38: ('attack power', False), 39: ('ranged attack power', False), 41: ('healing done', False),
              42: ('spell damage done', False), 43: ('mana per 5 sec.', False), 45: ('spell power', False), 46: ('health per 5 sec.', False),
              47: ('spell penetration', False), 48: ('block value', False), 50: ('Armor', True), 51: ('Fire Resistance', True),
              52: ('Frost Resistance', True), 53: ('Holy Resistance', True), 54: ('Shadow Resistance', True), 55: ('Nature Resistance', True),
              56: ('Arcane Resistance', True)}
TRIGGERS = {0: 'Use', 1: 'Equip', 2: 'Chance on hit'}
SKILL_PROFESSION, SKILL_SECONDARY = 11, 9          # SkillLine.CategoryID
CLASS_ICONS = {1: 'classicon_warrior', 2: 'classicon_paladin', 3: 'classicon_hunter', 4: 'classicon_rogue', 5: 'classicon_priest',
               7: 'classicon_shaman', 8: 'classicon_mage', 9: 'classicon_warlock', 11: 'classicon_druid'}

RATE = {}                   # (ip, bucket) -> recent request times
RATE_LOCK = threading.Lock()


def rate_ok(ip, bucket, per_minute):
    now = time.time()
    with RATE_LOCK:
        hits = [t for t in RATE.get((ip, bucket), []) if now - t < 60]
        allowed = len(hits) < per_minute
        if allowed:
            hits.append(now)
        RATE[(ip, bucket)] = hits
        if len(RATE) > 50000:                       # forget idle visitors
            for key in [k for k, v in RATE.items() if not v or now - v[-1] > 60]:
                del RATE[key]
    return allowed


def world_db():
    return CFG['db'].get('world', 'world')


def icon_url(name, size='large'):
    return '%s/%s/%s.jpg' % (CFG.get('icon_cdn', 'https://wow.zamimg.com/images/wow/icons'), size, name or 'inv_misc_questionmark')


def names(kind):
    """id -> (name, icon name, extra) for races, classes, skills, achievements (world.armory_name), cached for 10 minutes."""
    cache = NAMES_CACHE.get(kind)
    if cache and cache[0] > time.time():
        return cache[1]
    rows = query(world_db(), 'SELECT n.id, n.name, n.extra, i.name AS icon FROM armory_name n LEFT JOIN armory_icon i ON i.fileDataId = n.iconFileDataId '
                             'WHERE n.kind = %s', (kind,))
    data = {r['id']: (r['name'], r['icon'], r['extra']) for r in rows}
    if data:                                        # empty until ".armory export" has run: ask again next time
        NAMES_CACHE[kind] = (time.time() + 600, data)
    return data


NAMES_CACHE = {}


def item_tooltip_data(entries):
    if not entries:
        return {}
    marks = ','.join(['%s'] * len(entries))
    rows = query(world_db(), 'SELECT a.*, i.name AS icon FROM armory_item a LEFT JOIN armory_icon i ON i.fileDataId = a.iconFileDataId '
                             'WHERE a.id IN (%s)' % marks, tuple(entries))
    out = {}
    for r in rows:
        stats = [(int(t), int(v)) for t, v in (s.split(':') for s in r['stats'].split(',') if s)]
        effects = []
        for e in (x for x in r['effects'].split('|') if x):
            trig, spell, name = (e.split(':', 2) + ['', ''])[:3]
            if int(trig) in TRIGGERS and name:
                effects.append('%s: %s' % (TRIGGERS[int(trig)], name))
        if r['class'] == 2:
            kind = WEAPON_TYPES.get(r['subclass'], '')
        elif r['class'] == 4:
            kind = ARMOR_TYPES.get(r['subclass'], '')
        else:
            kind = ''
        lines = []
        if r['bonding'] in BONDING:
            lines.append(['', BONDING[r['bonding']]])
        lines.append(['split', INVENTORY_TYPES.get(r['inventoryType'], ''), kind])
        if r['dmgMax']:
            speed = r['delay'] / 1000.0
            lines.append(['split', '%d - %d Damage' % (r['dmgMin'], r['dmgMax']), 'Speed %.2f' % speed])
            if speed:
                lines.append(['', '(%.1f damage per second)' % ((r['dmgMin'] + r['dmgMax']) / 2 / speed)])
        if r['armor']:
            lines.append(['', '%d Armor' % r['armor']])
        for t, v in stats:
            text, primary = STAT_TYPES.get(t, ('stat %d' % t, True))
            if primary:
                lines.append(['', '%+d %s' % (v, text)])
        if r['requiredLevel'] > 1:
            lines.append(['', 'Requires Level %d' % r['requiredLevel']])
        for t, v in stats:
            text, primary = STAT_TYPES.get(t, ('stat %d' % t, True))
            if not primary:
                lines.append(['green', 'Equip: Increases %s by %d.' % (text, v)])
        lines += [['green', e] for e in effects]
        if r['description']:
            lines.append(['flavor', '"%s"' % r['description']])
        out[r['id']] = {'name': r['name'], 'q': r['quality'], 'ilvl': r['itemLevel'], 'icon': icon_url(r['icon']), 'lines': lines}
    return out
LOGO = ('<svg class="logo" viewBox="0 0 32 32" aria-hidden="true"><path d="M16 2.5 27.7 9.2v13.6L16 29.5 4.3 22.8V9.2Z" fill="#e6b04a"/>'
        '<path d="M16 8.5 22.5 12.2v7.6L16 23.5l-6.5-3.7v-7.6Z" fill="#0b1118"/><circle cx="16" cy="16" r="3.2" fill="#e6b04a"/></svg>')

CFG = {}
SESSIONS = {}
SESSIONS_LOCK = threading.Lock()


def log(*args):
    print(time.strftime('%H:%M:%S'), *args, flush=True)


def icon(name, cls='i'):
    return '<svg class="%s" viewBox="0 0 24 24" aria-hidden="true">%s</svg>' % (cls, ICONS[name])


# ---------------------------------------------------------------- database

def connect(database):
    c = CFG['db']
    return pymysql.connect(host=c['host'], port=int(c['port']), user=c['user'], password=c['password'], database=database,
                           charset='utf8mb4', autocommit=True, cursorclass=pymysql.cursors.DictCursor)


# open connections kept for reuse (a new connection costs ~35 ms on Windows; a page runs dozens of queries)
DB_POOL = {}
DB_POOL_LOCK = threading.Lock()


def run_sql(database, sql, args, fetch):
    with DB_POOL_LOCK:
        conn = DB_POOL.setdefault(database, []).pop() if DB_POOL.get(database) else None
    try:
        if conn is None:
            conn = connect(database)
        else:
            conn.ping(reconnect=True)
        with conn.cursor() as cur:
            cur.execute(sql, args)
            result = cur.fetchall() if fetch else cur.lastrowid
    except Exception:
        if conn is not None:
            conn.close()
        raise
    with DB_POOL_LOCK:
        pool = DB_POOL.setdefault(database, [])
        if len(pool) < 8:
            pool.append(conn)
        else:
            conn.close()
    return result


def query(database, sql, args=()):
    return run_sql(database, sql, args, True)


def query_one(database, sql, args=()):
    rows = query(database, sql, args)
    return rows[0] if rows else None


def execute(database, sql, args=()):
    return run_sql(database, sql, args, False)


def auth_db():
    return CFG['db']['auth']


def realm_cfg(realm_id):
    return CFG['realms'].get(str(realm_id))


def characters_db(realm_id):
    rc = realm_cfg(realm_id)
    if not rc:
        raise RuntimeError('realm %s is not in the config' % realm_id)
    return rc['characters']


def realm_name(realm_id):
    row = query_one(auth_db(), 'SELECT name FROM realmlist WHERE id = %s', (realm_id,))
    return row['name'] if row else 'Realm %s' % realm_id


def account_characters(realm_id, account_id):
    return query(characters_db(realm_id),
                 'SELECT guid, name, race, class, level, online, map, zone, position_x, position_y, position_z '
                 'FROM characters WHERE account = %s AND deleteInfos_Account IS NULL ORDER BY name', (account_id,))


def staff_level(account_id, realm_id):
    row = query_one(auth_db(), 'SELECT MAX(SecurityLevel) AS lvl FROM account_access WHERE AccountID = %s AND (RealmID = -1 OR RealmID = %s)',
                    (account_id, realm_id))
    return int(row['lvl'] or 0) if row else 0


# ---------------------------------------------------------------- remote access (worldserver console)

def ra_available(realm_id):
    ra = CFG.get('ra') or {}
    rc = realm_cfg(realm_id) or {}
    return bool(ra.get('user') and rc.get('ra_port')) and realm_online(realm_id)


def ra_command(realm_id, command):
    """Runs one console command on the realm's worldserver through RA (worldserver.conf Ra.Enable) and returns its output."""
    ra = CFG['ra']
    port = int(realm_cfg(realm_id)['ra_port'])
    with socket.create_connection((ra.get('host', '127.0.0.1'), port), timeout=15) as sock:
        def read_until(*marks):
            buf = b''
            while not any(m in buf for m in marks):
                chunk = sock.recv(4096)
                if not chunk:
                    break
                buf += chunk
            return buf.decode('utf-8', 'replace')

        read_until(b'Username: ')
        sock.sendall(ra['user'].encode() + b'\r\n')
        read_until(b'Password: ')
        sock.sendall(ra['password'].encode() + b'\r\n')
        if 'Authentication failed' in read_until(b'TC>', b'Authentication failed'):
            raise RuntimeError('RA login failed (check ra.user / ra.password; the account needs Ra.MinLevel on realm -1)')
        sock.sendall(command.encode('utf-8') + b'\r\n')
        out = read_until(b'TC>')
        sock.sendall(b'quit\r\n')
    return out.replace('TC>', '').strip()


def ra_quote(text):
    return text.replace('"', "'").replace('\r', ' ').replace('\n', ' ')


# ---------------------------------------------------------------- sessions

def start_session(token_row):
    sid = secrets.token_urlsafe(32)
    session = {
        'bnet': token_row['battlenetAccountId'],
        'account': token_row['accountId'],
        'realm': token_row['realmId'],
        'char': token_row['characterGuid'],
        'expires': token_row['expires'],
        'csrf': secrets.token_urlsafe(24),
        'staff': staff_level(token_row['accountId'], token_row['realmId']) >= int(CFG.get('staff_min_level', 2)),
        'level': staff_level(token_row['accountId'], token_row['realmId']),
    }
    with SESSIONS_LOCK:
        now = time.time()
        for old in [k for k, v in SESSIONS.items() if v['expires'] < now]:
            del SESSIONS[old]
        SESSIONS[sid] = session
    return sid


PENDING = {}                # code checks for several players behind one address: id -> {row, code, ip, target, expires, tries}
PENDING_LOCK = threading.Lock()


def online_from_ip(ip):
    """Characters online right now on accounts whose last login came from this address, as session rows."""
    accounts = query(auth_db(), 'SELECT id, battlenet_account FROM account WHERE last_ip = %s AND online <> 0 AND battlenet_account IS NOT NULL', (ip,))
    if not accounts:
        return []
    by_id = {a['id']: a['battlenet_account'] for a in accounts}
    rows = []
    for realm_id in CFG['realms']:
        marks = ','.join(['%s'] * len(by_id))
        for c in query(characters_db(realm_id), 'SELECT guid, account, name FROM characters WHERE online = 1 AND account IN (%s)' % marks, tuple(by_id)):
            rows.append({'battlenetAccountId': by_id[c['account']], 'accountId': c['account'], 'realmId': int(realm_id),
                         'characterGuid': c['guid'], 'characterName': c['name'], 'expires': int(time.time()) + 4 * 3600})
    return rows


def same_player(candidates):
    return len({c['battlenetAccountId'] for c in candidates}) == 1


def get_session(sid):
    with SESSIONS_LOCK:
        session = SESSIONS.get(sid)
        if session and session['expires'] < time.time():
            del SESSIONS[sid]
            session = None
    return session


# ---------------------------------------------------------------- html helpers

def esc(value):
    return html.escape(str(value), quote=True)


def fmt_time(ts):
    return time.strftime('%Y-%m-%d %H:%M', time.localtime(int(ts)))


def fmt_ago(ts):
    d = int(time.time() - int(ts))
    if d < 60:
        return 'just now'
    if d < 3600:
        return '%d min ago' % (d // 60)
    if d < 86400:
        return '%d h ago' % (d // 3600)
    return '%d d ago' % (d // 86400)


def status_tag(status):
    label, color = STATUS.get(status, (status, ''))
    return '<span class="tag %s">%s</span>' % (color, esc(label))


def char_name(c):
    return '<span class="c%d"><b>%s</b></span>' % (c['class'], esc(c['name']))


def faction(race):
    races = names('race')       # extra = ChrRaces.Alliance (1 = Horde) once ".armory export" wrote it; else the vanilla Horde races
    horde = races[race][2] == 1 if race in races and any(r[2] == 1 for r in races.values()) else race in HORDE_RACES
    return '<img class="fac-img" src="%s" alt="%s" title="%s">' % (
        icon_url('pvpcurrency-honor-horde' if horde else 'pvpcurrency-honor-alliance', 'medium'), 'H' if horde else 'A', 'Horde' if horde else 'Alliance')


def alert(kind, text):
    return '<div class="alert %s">%s<div class="t">%s</div></div>' % (kind, icon('check' if kind == 'ok' else 'alert'), text)


def kv_rows(rows):
    return ''.join('<div class="kv"><span class="muted">%s</span><span>%s</span></div>' % (k, v) for k, v in rows)


def panel(title, body, sub='', right='', flush=False, cls=''):
    head = '<div class="panel-h"><h2>%s</h2>%s%s</div>' % (
        title, '<span class="sub">%s</span>' % sub if sub else '', '<div class="right">%s</div>' % right if right else '') if title else ''
    return '<section class="panel %s">%s%s</section>' % (cls, head, body if flush else '<div class="panel-b">%s</div>' % body)


# ---------------------------------------------------------------- request handling

class Handler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'
    server_version = 'ForeverSupport/1'

    def do_GET(self):
        self.route('GET')

    def do_POST(self):
        self.route('POST')

    def log_message(self, fmt, *args):
        pass

    # ---- plumbing

    def route(self, method):
        url = urlsplit(self.path)
        self.query = {k: v[0] for k, v in parse_qs(url.query).items()}
        self.form = {}
        if method == 'POST':
            length = min(int(self.headers.get('Content-Length') or 0), 64 * 1024)
            self.form = {k: v[0] for k, v in parse_qs(self.rfile.read(length).decode('utf-8', 'replace')).items()}
        self.cookies = SimpleCookie(self.headers.get('Cookie') or '')
        sid = self.cookies['fsid'].value if 'fsid' in self.cookies else ''
        self.session = get_session(sid) if sid else None
        self.route_path = url.path.rstrip('/') or '/'
        path = self.route_path
        if not path.startswith('/static/'):
            log(method, path, '(session)' if self.session else '')
        try:
            if path.startswith('/static/'):
                return self.static(path[len('/static/'):])
            if path in ('/s', '/sso', '/login/sso'):
                return self.sso()
            if path in DIRECT_ENTRY:
                return self.direct_entry(DIRECT_ENTRY[path])
            if path == '/verify' and method == 'POST':
                return self.verify()
            if path == '/armory' or path.startswith('/armory/'):
                return self.armory(path)
            if path == '/favicon.ico':
                return self.send(404, b'', 'text/plain')
            if path.startswith('/service/'):
                return self.breaking_news()
            if not self.session:
                return self.page('Support', alert('info', 'Please open this page from the game: press <b>Help</b> (the question mark) '
                                                          'in the main menu.'), status=403)
            if method == 'POST' and not hmac.compare_digest(self.form.get('csrf', ''), self.session['csrf']):
                return self.page('Expired', alert('bad', 'This form has expired. Please go back and try again.'), status=400)
            handler = ROUTES.get((method, path))
            if not handler:
                return self.page('Not found', '<p class="muted">That page does not exist.</p>', status=404)
            handler(self)
        except Exception:
            log('ERROR', traceback.format_exc())
            self.page('Error', alert('bad', 'Something went wrong on our side. Please try again in a moment.'), status=500)

    # The "Breaking News" box on the login and character screens. The client downloads it from
    # https://breaking-news.support.blizzard.com/service/<product>/<branch>/<region>/<locale>; the Forever Launcher (2.5.2+)
    # points that URL at this site's plain http port (news_http_port, default 80: the download refuses our certificate). Answer
    # like Blizzard's: "SERVERALERT:" and the message as HTML, from breaking_news.html or breaking_news.txt next to this file
    # (read on every request, so editing needs no restart); missing or empty = no box.
    def breaking_news(self):
        log('breaking news requested:', self.path)
        # breaking_news.html: sent as it is (the box is a game SimpleHTML frame, not a browser: see breaking_news.html.example)
        try:
            with open(os.path.join(HERE, 'breaking_news.html'), encoding='utf-8-sig') as f:
                html = f.read().strip()
            if html:
                if not html.lower().startswith('<html'):
                    html = '<html><body>%s</body></html>' % html
                return self.send(200, ('SERVERALERT:\n' + html).encode('utf-8'), 'text/html; charset=UTF-8')
        except OSError:
            pass
        text = ''
        try:
            with open(os.path.join(HERE, 'breaking_news.txt'), encoding='utf-8-sig') as f:
                text = f.read().strip()
        except OSError:
            pass
        if not text:
            return self.send(200, b'', 'text/html; charset=utf-8')
        paragraphs = ''.join('<p>%s</p>' % esc(p).replace('\n', '<br>') for p in text.replace('\r\n', '\n').split('\n\n') if p.strip())
        body = 'SERVERALERT:\n<html><body>%s</body></html>' % paragraphs
        return self.send(200, body.encode('utf-8'), 'text/html; charset=UTF-8')

    def send(self, status, body, content_type='text/html; charset=utf-8', headers=(), cache=False):
        self.send_response(status)
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'public, max-age=300' if cache else 'no-store')
        self.send_header('X-Frame-Options', 'DENY')
        self.send_header('X-Content-Type-Options', 'nosniff')
        for k, v in headers:
            self.send_header(k, v)
        self.end_headers()
        self.wfile.write(body)

    def static(self, name):
        ext = os.path.splitext(name)[1]
        path = os.path.join(STATIC_DIR, os.path.basename(name))
        if ext not in STATIC_TYPES or not os.path.isfile(path):
            return self.send(404, b'', 'text/plain')
        with open(path, 'rb') as f:
            self.send(200, f.read(), STATIC_TYPES[ext], cache=True)

    def redirect(self, location, headers=()):
        self.send(303, b'', 'text/plain', [('Location', location), *headers])

    def cookie(self, name, allowed, default):
        value = self.cookies[name].value if name in self.cookies else default
        return value if value in allowed else default

    def page(self, title, body, lead='', status=200, section='Support', head='', heading=True):
        s = self.session
        theme = self.cookie('fs_theme', ('dark', 'light'), 'dark')
        size = self.cookie('fs_size', ('s', 'm', 'l'), 'm')
        server = esc(CFG.get('server_name', 'Forever'))

        foot = ''
        items = []
        if s:
            answered = query_one(auth_db(), "SELECT COUNT(*) AS n FROM support_tickets WHERE battlenetAccountId = %s AND status = 'answered'",
                                 (s['bnet'],))['n']
            items = [('/', 'home', 'Home', 0), ('/account', 'user', 'Account', 0), ('/shop', 'coin', 'Shop', 0), ('/unstuck', 'pin', 'Unstuck', 0),
                     ('/ticket/new?category=bug', 'bug', 'Report a bug', 0), ('/ticket/new', 'help', 'Get help', 0), ('/tickets', 'tickets', 'My tickets', answered),
                     ('/faq', 'book', 'FAQ', 0)]
            if s['staff']:
                waiting = query_one(auth_db(), "SELECT COUNT(*) AS n FROM support_tickets WHERE status = 'open'")['n']
                items.append(None)
                items.append(('/staff', 'shield', 'Staff', waiting))
                if self.is_shop_admin():
                    items.append(('/staff/shop', 'store', 'Shop admin', 0))
            items.append(None)
            char = self.current_character()
            foot = ('<div class="side-foot"><div class="row"><span class="dot"></span>%s</div>%s</div>') % (
                esc(realm_name(s['realm'])),
                '<div class="row"><span class="dot off"></span>%s<span class="v">%s</span></div>' % ('Character', esc(char['name'])) if char else '')
        items.append(('/armory', 'search', 'Armory', 0))
        current = self.route_path + ('?category=bug' if self.query.get('category') == 'bug' else '')
        links = []
        for item in items:
            if item is None:
                links.append('<div class="sep"></div>')
                continue
            href, ic, label, count = item
            active = current == href or (href == '/tickets' and current == '/ticket') or (href == '/armory' and current.startswith('/armory')) or \
                (href == '/staff/shop' and current.startswith('/staff/shop'))
            links.append('<a href="%s" class="%s" title="%s">%s<span class="lbl">%s</span>%s</a>' % (
                esc(href), 'on' if active else '', esc(label), icon(ic), esc(label), '<span class="count">%d</span>' % count if count else ''))
        nav = '<nav class="nav">%s</nav>' % ''.join(links)
        crumb = '%s %s' % (server, section) + (' · %s' % esc(title) if title != section else '')

        sizes = ''.join('<button type="button" data-set-size="%s" class="%s" title="%s">%s</button>' % (
            k, 'on' if k == size else '', t, l) for k, l, t in (('s', 'A-', 'Smaller text'), ('m', 'A', 'Normal text'), ('l', 'A+', 'Larger text')))
        # the game's Help window (BlizzardBrowser, 963 x 602) gets a compact layout
        ingame = ' data-ingame="1"' if 'WorldOfWarcraft/' in (self.headers.get('User-Agent') or '') else ''
        doc = ('<!doctype html><html lang="en" data-theme="%s" data-size="%s"' + ingame + '><head><meta charset="utf-8">'
               '<meta name="viewport" content="width=device-width,initial-scale=1"><title>%s · %s %s</title>'
               '<link rel="stylesheet" href="/static/theme.css?v=' + STATIC_VERSION + '"><script src="/static/app.js?v=' + STATIC_VERSION + '" defer></script>%s</head><body>'
               '<div class="app"><aside class="side"><a class="brand" href="%s">%s<span class="txt"><b>%s</b><small>%s</small></span></a>%s%s</aside>'
               '<div class="main"><div class="top"><span class="crumb">%s</span><div class="tools"><div class="seg">%s</div>'
               '<button type="button" class="icon-btn" data-toggle-theme title="Light / dark">%s</button></div></div>'
               '<main class="content">%s%s%s</main></div></div></body></html>') % (
            theme, size, esc(title), server, esc(section), head, '/' if s or section == 'Support' else '/armory', LOGO, server, esc(section),
            nav, foot, crumb, sizes, icon('sun'), '<h1>%s</h1>' % esc(title) if heading else '', '<p class="lead">%s</p>' % lead if lead else '', body)
        self.send(status, doc.encode('utf-8'))

    def csrf_field(self):
        return '<input type="hidden" name="csrf" value="%s">' % esc(self.session['csrf'])

    def current_character(self):
        s = self.session
        if not s or not s['char']:
            return None
        if 'char_row' not in s:
            s['char_row'] = query_one(characters_db(s['realm']), 'SELECT guid, name, class FROM characters WHERE guid = %s AND account = %s',
                                      (s['char'], s['account']))
        return s['char_row']

    def characters(self):
        return account_characters(self.session['realm'], self.session['account'])

    # ---- pages

    def sso(self):
        token = self.query.get('token', '')
        ref = self.query.get('ref', '')
        row = query_one(auth_db(), 'SELECT * FROM battlenet_sso_tokens WHERE token = %s AND expires > UNIX_TIMESTAMP()', (token,)) if token else None
        log('SSO', 'ok' if row else 'rejected', 'account', row['accountId'] if row else '-', 'ref', ref)
        if not row:
            self.session = None
            return self.page('Support', alert('bad', 'Your support login has expired. Close this window and open Help again.'), status=403)
        execute(auth_db(), 'DELETE FROM battlenet_sso_tokens WHERE token = %s', (token,))
        sid = start_session(row)
        target = '/tickets' if 'ticket' in ref.lower() else '/'
        self.redirect(target, [('Set-Cookie', 'fsid=%s; Path=/; HttpOnly; Secure; SameSite=Lax' % sid)])

    def direct_entry(self, target):
        """The Help window opens our URLs directly (the launcher points the client's Help URL at this site), without a token in the URL.
        Who it is: a token this IP got a moment ago, else the character that is online from this IP. Several players behind one
        address pick their character and type a code the server shows on that character's screen."""
        if self.session:
            return self.redirect(target)
        ip = self.client_address[0]
        row = query_one(auth_db(), 'SELECT * FROM battlenet_sso_tokens WHERE ip = %s AND issued >= UNIX_TIMESTAMP() - %s '
                                   'AND expires > UNIX_TIMESTAMP() ORDER BY issued DESC LIMIT 1', (ip, int(CFG.get('direct_entry_seconds', 30))))
        if row:
            log('direct entry', ip, 'token, account', row['accountId'])
            execute(auth_db(), 'DELETE FROM battlenet_sso_tokens WHERE token = %s', (row['token'],))
            return self.login(row, target)

        candidates = online_from_ip(ip)
        log('direct entry', ip, '%d online character(s) from this address' % len(candidates))
        if len(candidates) == 1:
            return self.login(candidates[0], target)
        if not candidates:
            return self.page('Support', alert('bad', 'We could not tell who you are. Log in to the game, then open Help again.'), status=403)
        return self.verify_form(candidates, target)

    def login(self, row, target):
        sid = start_session(row)
        self.redirect(target, [('Set-Cookie', 'fsid=%s; Path=/; HttpOnly; Secure; SameSite=Lax' % sid)])

    def verify_form(self, candidates, target, note=''):
        options = ''.join('<label class="pick" style="margin:6px 0"><input type="radio" name="c" value="%d"%s> <b>%s</b>&nbsp;<span class="muted">%s</span></label>' % (
            i, ' checked' if i == 0 else '', esc(c['characterName']), esc(realm_name(c['realmId']))) for i, c in enumerate(candidates))
        one = same_player(candidates)
        body = note + panel('Which character are you?', (
            '<p class="muted">%s</p><form method="post" action="/verify">%s<input type="hidden" name="t" value="%s"><div class="actions">'
            '<button class="btn primary" type="submit">%s</button></div></form>') % (
            'You have several characters online. Which one needs help?' if one else
            'Several players are connected from your network. Pick your character and we will show a code on its screen.',
            options, esc(target), 'Continue' if one else 'Send me a code'))
        self.page('Support', body)

    def verify(self):
        ip = self.client_address[0]
        candidates = online_from_ip(ip)
        target = self.form.get('t', '/') if self.form.get('t', '/') in DIRECT_ENTRY.values() else '/'
        pid = self.form.get('p', '')
        if pid:
            with PENDING_LOCK:
                pend = PENDING.get(pid)
                if pend:
                    pend['tries'] += 1
            if not pend or pend['expires'] < time.time() or pend['ip'] != ip or pend['tries'] > 5:
                return self.verify_form(candidates, target, alert('bad', 'That code has expired. Please ask for a new one.')) if candidates else \
                    self.page('Support', alert('bad', 'Log in to the game, then open Help again.'), status=403)
            if not hmac.compare_digest(self.form.get('code', '').strip(), pend['code']):
                return self.code_form(pid, pend, alert('bad', 'That code is not right. Check your game screen.'))
            with PENDING_LOCK:
                PENDING.pop(pid, None)
            return self.login(pend['row'], pend['target'])
        try:
            choice = candidates[int(self.form.get('c', '-1'))]
        except (ValueError, IndexError):
            return self.verify_form(candidates, target) if candidates else self.redirect('/kb')
        if same_player(candidates):     # multiboxing on one battle.net account: one person, no code needed
            return self.login(choice, target)
        code = '%06d' % secrets.randbelow(1000000)
        out = ra_command(choice['realmId'], 'send message %s Support website code: %s' % (choice['characterName'], code)) if ra_available(choice['realmId']) else None
        if out is None:
            return self.page('Support', alert('bad', 'We cannot send codes right now. Please try again later.'), status=503)
        pid = secrets.token_urlsafe(18)
        pend = {'row': choice, 'code': code, 'ip': ip, 'target': target, 'expires': time.time() + 300, 'tries': 0}
        with PENDING_LOCK:
            for old in [k for k, v in PENDING.items() if v['expires'] < time.time()]:
                del PENDING[old]
            PENDING[pid] = pend
        log('verify code sent to', choice['characterName'])
        self.code_form(pid, pend)

    def code_form(self, pid, pend, note=''):
        self.page('Support', note + panel('Enter your code', (
            '<p class="muted">A 6-digit code is now shown on <b>%s</b>\'s screen.</p><form method="post" action="/verify">'
            '<input type="hidden" name="p" value="%s"><div class="field"><input type="text" name="code" inputmode="numeric" maxlength="6" autofocus required></div>'
            '<div class="actions"><button class="btn primary" type="submit">Continue</button></div></form>') % (esc(pend['row']['characterName']), esc(pid))))

    def home(self):
        s = self.session
        char = self.current_character()
        tiles = [
            ('/unstuck', 'pin', 'Unstuck', 'Stuck in the world? Send a character to its inn or the nearest graveyard.'),
            ('/ticket/new?category=bug', 'bug', 'Report a bug', 'Quests, creatures, spells or items that do not work as they should.'),
            ('/ticket/new?category=stuck', 'alert', 'Still stuck', 'Unstuck did not help? A game master will move you.'),
            ('/ticket/new', 'help', 'Get help', 'Account, character or player problems, and everything else.'),
            ('/tickets', 'tickets', 'My tickets', 'Follow your reports and read replies from the staff.'),
            ('/faq', 'book', 'FAQ', 'Answers to the most common questions.'),
        ]
        body = ''
        if CFG.get('news_html'):
            body += panel('News', CFG['news_html'], cls='accent')
        body += '<div class="tiles">%s</div>' % ''.join(
            '<a class="tile" href="%s"><span class="ic">%s</span><b>%s</b><span>%s</span></a>' % (esc(h), icon(i), esc(t), esc(d))
            for h, i, t, d in tiles)
        recent = query(auth_db(), 'SELECT id, category, subject, status, updated FROM support_tickets WHERE battlenetAccountId = %s '
                                  'ORDER BY updated DESC LIMIT 5', (s['bnet'],))
        if recent:
            body += panel('Your latest tickets', self.ticket_rows(recent, False), right='<a href="/tickets">See all</a>', flush=True)
        lead = ('Hello %s. ' % char_name(char) if char else 'Hello. ') + 'What can we help you with?'
        self.page('Support', '<div class="stack">%s</div>' % body, lead)

    def faq(self):
        entries = CFG.get('faq') or []
        items = ''.join('<details class="faq"><summary>%s%s</summary><div class="a">%s</div></details>' % (esc(q), icon('chevron', 'i sm'), a)
                        for q, a in entries)
        self.page('Frequently asked questions', panel('', items or '<div class="empty">Nothing here yet.</div>', flush=True),
                  'Can\'t find your answer? <a href="/ticket/new">Ask us</a>.')

    # account

    def bnet_account(self):
        return query_one(auth_db(), 'SELECT id, email, joindate, last_login, online, locked, authenticator_secret FROM battlenet_accounts WHERE id = %s',
                         (self.session['bnet'],))

    def account_page(self, note=''):
        s = self.session
        bnet = self.bnet_account()
        if not bnet:
            return self.page('Account', alert('bad', 'Your Battle.net account was not found.'), status=404)
        secured = bool(bnet['authenticator_secret'])

        def kv(rows):
            return ''.join('<div class="kv"><span class="muted">%s</span><span>%s</span></div>' % (k, v) for k, v in rows)

        info = kv([
            ('E-mail', esc(bnet['email'])),
            ('Account number', '#%d' % bnet['id']),
            ('BattleTag', '<span class="muted">Not available on this server yet</span>'),
            ('Member since', esc(str(bnet['joindate'])[:10])),
            ('Last login', esc(str(bnet['last_login'])[:16]) if bnet['last_login'] else '<span class="muted">Never</span>'),
            ('Status', '<span class="tag green">Online</span>' if bnet['online'] else '<span class="tag">Offline</span>'),
            ('Authenticator', '<span class="tag green">On</span>' if secured else '<span class="tag amber">Off</span>'),
        ])

        # security
        pending = s.get('totp_pending')
        if secured:
            security = ('<p>Your account is protected by an authenticator. The game asks for its code each time you log in.</p>'
                        '<p class="muted">Lost your phone or reset the app? <a href="/ticket/new?category=account">Ask a game master</a> to remove it.</p>'
                        '<form method="post" action="/account/authenticator/remove">%s<div class="field"><span>To remove it, enter a current code</span>'
                        '<input type="text" name="code" inputmode="numeric" autocomplete="one-time-code" maxlength="7" required></div>'
                        '<div class="actions"><button class="btn danger" type="submit">Remove authenticator</button></div></form>') % self.csrf_field()
        elif pending:
            qr = qr_svg(totp_uri(pending, bnet['email']))
            grouped = ' '.join(pending[i:i + 4] for i in range(0, len(pending), 4))
            security = (
                '<ol class="steps"><li>Install an authenticator app on your phone: the <b>Battle.net</b> app, Google Authenticator, '
                'Microsoft Authenticator or Authy.</li><li>In the app, add an account and scan this code%s.</li>'
                '<li>Type the 6-digit code the app shows and press <b>Turn on</b>.</li></ol>'
                '%s<div class="field"><span>Or type this key into the app (time based)</span><div class="mono totp-key">%s</div></div>'
                '<form method="post" action="/account/authenticator/confirm">%s<div class="field"><span>Code from the app</span>'
                '<input type="text" name="code" inputmode="numeric" autocomplete="one-time-code" maxlength="7" autofocus required></div>'
                '<div class="actions"><button class="btn primary" type="submit">Turn on</button>'
                '<button class="btn ghost" type="submit" formaction="/account/authenticator/cancel" formnovalidate>Cancel</button></div></form>') % (
                '' if qr else ' (or type the key below)', '<div class="qr">%s</div>' % qr if qr else '', esc(grouped), self.csrf_field())
        else:
            security = ('<p>Add an authenticator to protect your account. After your password, the game asks for a code from an app on your phone.</p>'
                        '<ul class="perks"><li>4 extra backpack slots on all your characters</li><li>Full access to the Group Finder</li>'
                        '<li>No "attach an Authenticator" reminders in game</li></ul>'
                        '<form method="post" action="/account/authenticator/start">%s<div class="actions">'
                        '<button class="btn primary" type="submit">Set up an authenticator</button></div></form>') % self.csrf_field()

        # game accounts and characters
        accounts = query(auth_db(), 'SELECT id, username, expansion, last_login, online FROM account WHERE battlenet_account = %s ORDER BY battlenet_index',
                         (bnet['id'],))
        chars = []
        if accounts:
            ids = tuple(a['id'] for a in accounts)
            marks = ','.join(['%s'] * len(ids))
            for realm_id in CFG['realms']:
                try:
                    rows = query(characters_db(realm_id), 'SELECT name, race, class, level, online, logout_time, account FROM characters '
                                                          'WHERE account IN (%s) AND deleteInfos_Account IS NULL' % marks, ids)
                except pymysql.MySQLError:
                    continue
                for c in rows:
                    c['realm'] = realm_name(realm_id)
                    chars.append(c)
        chars.sort(key=lambda c: (-int(c['online'] or 0), -int(c['logout_time'] or 0)))

        acc_rows = ''.join('<tr><td><b>%s</b></td><td class="num">%d</td><td>%s</td><td>%s</td></tr>' % (
            esc('WoW' + a['username'].split('#', 1)[1] if '#' in a['username'] else a['username']),
            sum(1 for c in chars if c['account'] == a['id']),
            esc(str(a['last_login'])[:16]) if a['last_login'] else '<span class="muted">Never</span>',
            '<span class="tag green">Online</span>' if a['online'] else '<span class="tag">Offline</span>') for a in accounts)
        acc_table = ('<div class="tbl-wrap"><table class="tbl"><thead><tr><th>Game account</th><th class="num">Characters</th><th>Last login</th>'
                     '<th>Status</th></tr></thead><tbody>%s</tbody></table></div>') % acc_rows if accounts else '<div class="empty">No game accounts.</div>'

        races = names('race')       # client race names (".armory export"), the vanilla list until then
        char_rows = ''.join('<tr><td>%s</td><td class="num">%d</td><td class="hide-sm">%s %s</td><td>%s</td><td class="hide-sm">%s</td></tr>' % (
            char_name(c), c['level'], esc(races[c['race']][0] if c['race'] in races else RACES.get(c['race'], '')), esc(CLASSES.get(c['class'], '')),
            esc(c['realm']),
            '<span class="tag green">Online</span>' if c['online'] else (esc(fmt_ago(c['logout_time'])) if c['logout_time'] else '')) for c in chars)
        char_table = ('<div class="tbl-wrap"><table class="tbl"><thead><tr><th>Character</th><th class="num">Level</th><th class="hide-sm">Race and class</th>'
                      '<th>Realm</th><th class="hide-sm">Last played</th></tr></thead><tbody>%s</tbody></table></div>') % char_rows \
            if chars else '<div class="empty">No characters yet.</div>'

        body = note + '<div class="cols"><div class="stack">%s%s</div><div class="stack">%s%s</div></div>' % (
            panel('Characters', char_table, sub='%d on all realms' % len(chars), flush=True),
            panel('Game accounts', acc_table, flush=True),
            panel('Battle.net account', info),
            panel('Authenticator', security, right='<span class="tag green">Protected</span>' if secured else ''))
        self.page('Account', body, 'Your Battle.net account, game accounts and characters.')

    def authenticator_start(self):
        bnet = self.bnet_account()
        if bnet and not bnet['authenticator_secret']:
            self.session['totp_pending'] = totp_new_secret()
        self.redirect('/account')

    def authenticator_cancel(self):
        self.session.pop('totp_pending', None)
        self.redirect('/account')

    def authenticator_confirm(self):
        pending = self.session.get('totp_pending')
        if not pending:
            return self.redirect('/account')
        if not rate_ok(self.client_address[0], 'totp', 10):
            return self.account_page(alert('bad', 'Too many tries. Wait a minute and try again.'))
        if not totp_check(pending, self.form.get('code', '')):
            return self.account_page(alert('bad', 'That code is not right. Check that the time on your phone is correct, then try the newest code.'))
        execute(auth_db(), 'UPDATE battlenet_accounts SET authenticator_secret = %s WHERE id = %s AND authenticator_secret IS NULL',
                (pending, self.session['bnet']))
        self.session.pop('totp_pending', None)
        log('authenticator added, bnet account', self.session['bnet'])
        self.account_page(alert('ok', 'Your authenticator is on. From your next login the game asks for its code. '
                                      'Log your character out and back in to get the 4 extra backpack slots.'))

    def authenticator_remove(self):
        bnet = self.bnet_account()
        if not bnet or not bnet['authenticator_secret']:
            return self.redirect('/account')
        if not rate_ok(self.client_address[0], 'totp', 10):
            return self.account_page(alert('bad', 'Too many tries. Wait a minute and try again.'))
        if not totp_check(bnet['authenticator_secret'], self.form.get('code', '')):
            return self.account_page(alert('bad', 'That code is not right. Try the newest code from your app.'))
        execute(auth_db(), 'UPDATE battlenet_accounts SET authenticator_secret = NULL WHERE id = %s', (bnet['id'],))
        log('authenticator removed, bnet account', bnet['id'])
        self.account_page(alert('ok', 'The authenticator was removed. The 4 extra backpack slots go away at your next login '
                                      '(items in them are sent to you by mail).'))

    # unstuck

    def unstuck_form(self, note=''):
        s = self.session
        chars = self.characters()
        cooldown = int(CFG.get('unstuck_cooldown_minutes', 15))
        if chars:
            rows = ''.join(
                '<tr><td><label class="pick"><input type="radio" name="guid" value="%d"%s>%s</label></td><td>%s</td><td class="num">%d</td>'
                '<td class="hide-sm">%s <span class="c%d">%s</span></td><td><span class="nowrap"><span class="dot%s"></span>%s</span></td></tr>' % (
                    c['guid'], ' checked' if c['guid'] == s['char'] or len(chars) == 1 else '', char_name(c), faction(c['race']), c['level'],
                    esc(RACES.get(c['race'], '')), c['class'], esc(CLASSES.get(c['class'], '')), '' if c['online'] else ' off',
                    'Online' if c['online'] else 'Offline') for c in chars)
            table = ('<div class="tbl-wrap"><table class="tbl"><thead><tr><th>Character</th><th>H/A</th><th class="num">Level</th>'
                     '<th class="hide-sm">Class</th><th>Status</th></tr></thead><tbody>%s</tbody></table></div>') % rows
        else:
            table = '<div class="empty">No characters on this realm.</div>'
        dest = ('<div class="panel-b"><div class="field"><span>Send the character to</span><div class="seg">'
                '<input type="radio" name="dest" id="d-inn" value="inn" checked><label for="d-inn">Its inn (hearthstone)</label>'
                '<input type="radio" name="dest" id="d-gy" value="graveyard"><label for="d-gy">Nearest graveyard</label></div></div>'
                '<div class="actions"><button class="btn primary" type="submit">%sUnstuck</button>'
                '<span class="muted">Still stuck? <a href="/ticket/new?category=stuck">Ask a game master</a></span></div></div>') % icon('pin', 'i sm')
        form = '<form method="post" action="/unstuck">%s%s</form>' % (self.csrf_field(), panel('Choose a character', table + dest, flush=True))
        self.page('Unstuck', note + form,
                  'Moves a character that is stuck in the world. Works once every %d minutes per character, not in combat or on a flight path.'
                  % cooldown)

    def unstuck(self):
        s = self.session
        try:
            guid = int(self.form.get('guid', '0'))
        except ValueError:
            guid = 0
        dest = self.form.get('dest', 'inn')
        if dest not in ('inn', 'graveyard'):
            dest = 'inn'
        char = next((c for c in self.characters() if c['guid'] == guid), None)
        if not char:
            return self.unstuck_form(alert('bad', 'Please pick one of your characters.'))

        cooldown = int(CFG.get('unstuck_cooldown_minutes', 15)) * 60
        last = query_one(auth_db(), "SELECT MAX(time) AS t FROM support_unstuck_log WHERE realmId = %s AND characterGuid = %s AND result = 'ok'",
                         (s['realm'], guid))['t']
        if last and time.time() - last < cooldown:
            wait = int((cooldown - (time.time() - last)) // 60) + 1
            return self.unstuck_form(alert('bad', '%s was moved recently. Try again in %d minute%s.' % (char_name(char), wait, '' if wait == 1 else 's')))

        if ra_available(s['realm']):
            out = ra_command(s['realm'], 'unstuck %s %s' % (char['name'], dest))
            ok = not out
        elif not char['online']:
            # no console access configured: do what the console command does for offline characters (inn only)
            home = query_one(characters_db(s['realm']), 'SELECT mapId, zoneId, posX, posY, posZ, orientation FROM character_homebind WHERE guid = %s', (guid,))
            if home:
                execute(characters_db(s['realm']),
                        "UPDATE characters SET position_x = %s, position_y = %s, position_z = %s, orientation = %s, map = %s, zone = %s, "
                        "trans_x = 0, trans_y = 0, trans_z = 0, transguid = 0, taxi_path = '', cinematic = 1 WHERE guid = %s AND online = 0",
                        (home['posX'], home['posY'], home['posZ'], home['orientation'], home['mapId'], home['zoneId'], guid))
                dest = 'inn'
            ok, out = bool(home), '' if home else 'no hearthstone location'
        else:
            return self.unstuck_form(alert('bad', 'Online unstuck is not available right now. Log out to character select and try again.'))

        execute(auth_db(), 'INSERT INTO support_unstuck_log (accountId, realmId, characterGuid, destination, result, time) VALUES (%s, %s, %s, %s, %s, %s)',
                (s['account'], s['realm'], guid, dest, 'ok' if ok else out[:255], int(time.time())))
        log('unstuck', char['name'], dest, 'ok' if ok else out)
        if ok:
            self.unstuck_form(alert('ok', '%s has been moved to %s.' % (char_name(char), 'its inn' if dest == 'inn' else 'the nearest graveyard')))
        else:
            self.unstuck_form(alert('bad', 'That did not work: %s. Leave combat or finish your flight, then try again.' % esc(out or 'unknown error')))

    # tickets

    def ticket_new_form(self, note='', values=None):
        values = values or {}
        category = values.get('category') or self.query.get('category', 'other')
        if category not in CATEGORIES:
            category = 'other'
        hints = {
            'bug': 'What happened, what did you expect, and how can we make it happen again? Quest, creature or item names help a lot.',
            'stuck': 'Where are you stuck and what did you try? Your character\'s position is attached for us.',
            'player': 'Who, when, where and what happened.',
        }
        cats = ''.join('<input type="radio" name="category" id="cat-%s" value="%s"%s><label for="cat-%s">%s</label>' % (
            k, k, ' checked' if k == category else '', k, esc(v)) for k, v in CATEGORIES.items())
        selected = int(values.get('guid') or self.session['char'] or 0)
        chars = ''.join('<option value="%d"%s>%s · level %d %s</option>' % (
            c['guid'], ' selected' if c['guid'] == selected else '', esc(c['name']), c['level'], esc(CLASSES.get(c['class'], '')))
            for c in self.characters())
        form = ('<form method="post" action="/ticket/new">%s'
                '<div class="field"><span>Type</span><div class="tbl-wrap"><div class="seg">%s</div></div></div>'
                '<div class="field"><span>Character</span><select name="guid"><option value="0">No character</option>%s</select></div>'
                '<div class="field"><span>Subject</span><input type="text" name="subject" maxlength="%d" value="%s" required></div>'
                '<div class="field"><span>Details</span><textarea name="message" maxlength="%d" placeholder="%s" required>%s</textarea></div>'
                '<div class="actions"><button class="btn primary" type="submit">%sSend</button>'
                '<span class="muted">We reply here and send you an in-game mail.</span></div></form>') % (
            self.csrf_field(), cats, chars, SUBJECT_MAX, esc(values.get('subject', '')), MESSAGE_MAX,
            esc(hints.get(category, 'Tell us as much as you can.')), esc(values.get('message', '')), icon('send', 'i sm'))
        title = 'Report a bug' if category == 'bug' else 'Get help'
        lead = ('Thanks for helping us find problems. The more detail, the faster we can fix it.' if category == 'bug'
                else 'Tell us what is going on and a game master will get back to you.')
        self.page(title, note + panel('', form), lead)

    def ticket_create(self):
        s = self.session
        f = self.form
        category = f.get('category', 'other')
        subject = f.get('subject', '').strip()[:SUBJECT_MAX]
        message = f.get('message', '').strip()[:MESSAGE_MAX]
        if category not in CATEGORIES or not subject or not message:
            return self.ticket_new_form(alert('bad', 'Please fill in a subject and the details.'), f)
        open_count = query_one(auth_db(), "SELECT COUNT(*) AS n FROM support_tickets WHERE battlenetAccountId = %s AND status <> 'closed'",
                               (s['bnet'],))['n']
        if open_count >= MAX_OPEN_TICKETS:
            return self.ticket_new_form(alert('bad', 'You already have %d open tickets. Please wait for a reply or close one.' % open_count), f)
        try:
            guid = int(f.get('guid', '0'))
        except ValueError:
            guid = 0
        char = next((c for c in self.characters() if c['guid'] == guid), None)
        now = int(time.time())
        ticket_id = execute(auth_db(),
                            'INSERT INTO support_tickets (battlenetAccountId, accountId, realmId, characterGuid, characterName, category, subject, message, '
                            'mapId, zoneId, posX, posY, posZ, status, created, updated) VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)',
                            (s['bnet'], s['account'], s['realm'], char['guid'] if char else 0, char['name'] if char else '', category, subject,
                             message, char['map'] if char else 0, char['zone'] if char else 0, char['position_x'] if char else 0,
                             char['position_y'] if char else 0, char['position_z'] if char else 0, 'open', now, now))
        log('ticket', ticket_id, category, 'by account', s['account'])
        self.redirect('/ticket?id=%d&new=1' % ticket_id)

    def load_ticket(self, ticket_id):
        try:
            ticket_id = int(ticket_id)
        except (TypeError, ValueError):
            return None
        ticket = query_one(auth_db(), 'SELECT * FROM support_tickets WHERE id = %s', (ticket_id,))
        if not ticket or (ticket['battlenetAccountId'] != self.session['bnet'] and not self.session['staff']):
            return None
        return ticket

    def ticket_list(self):
        tickets = query(auth_db(), 'SELECT id, category, subject, status, updated FROM support_tickets WHERE battlenetAccountId = %s '
                                   'ORDER BY status = \'closed\', updated DESC LIMIT 100', (self.session['bnet'],))
        new = '<a class="btn" href="/ticket/new">%sNew ticket</a>' % icon('plus', 'i sm')
        self.page('My tickets', panel('Tickets', self.ticket_rows(tickets, False), sub='· %d' % len(tickets), right=new, flush=True),
                  'Everything you sent us, newest first. Tickets with a staff reply are marked green.')

    def ticket_rows(self, tickets, staff):
        if not tickets:
            return '<div class="empty">No tickets.</div>'
        head = '<tr><th class="num">#</th><th>Subject</th>%s<th class="hide-sm">Type</th><th>Status</th><th class="hide-sm">Updated</th></tr>' % (
            '<th>Character</th>' if staff else '')
        rows = ''.join('<tr><td class="num faint">%d</td><td><a class="row-link" href="/ticket?id=%d">%s</a></td>%s<td class="hide-sm muted">%s</td>'
                       '<td>%s</td><td class="hide-sm muted nowrap" title="%s">%s</td></tr>' % (
                           t['id'], t['id'], esc(t['subject']), '<td>%s</td>' % esc(t.get('characterName') or '-') if staff else '',
                           esc(CATEGORIES.get(t['category'], t['category'])), status_tag(t['status']), fmt_time(t['updated']), fmt_ago(t['updated']))
                       for t in tickets)
        return '<div class="tbl-wrap"><table class="tbl"><thead>%s</thead><tbody>%s</tbody></table></div>' % (head, rows)

    def ticket_view(self):
        ticket = self.load_ticket(self.query.get('id'))
        if not ticket:
            return self.page('Not found', '<p class="muted">That ticket does not exist.</p>', status=404)
        s = self.session
        replies = query(auth_db(), 'SELECT * FROM support_ticket_replies WHERE ticketId = %s ORDER BY id', (ticket['id'],))
        body = alert('ok', 'Thanks! Your ticket has been sent. Staff replies show up here.') if self.query.get('new') else ''

        author = esc(ticket['characterName'] or 'You')
        events = ['<li><span class="ic muted">%s</span><span class="title"><b>%s</b> opened the ticket</span><span class="when">%s</span>'
                  '<div class="body">%s</div></li>' % (icon('user', 'i sm'), author, fmt_time(ticket['created']), esc(ticket['message']))]
        for r in replies:
            who = 'Staff' if r['isStaff'] else author
            events.append('<li><span class="ic %s">%s</span><span class="title"><b>%s</b> replied</span><span class="when">%s</span>'
                          '<div class="body">%s</div></li>' % ('staff' if r['isStaff'] else 'muted', icon('shield' if r['isStaff'] else 'user', 'i sm'),
                                                               who, fmt_time(r['created']), esc(r['message'])))
        conv = panel('Conversation', '<ul class="events">%s</ul>' % ''.join(events), right=status_tag(ticket['status']), flush=True)
        if ticket['status'] != 'closed':
            conv += panel('', ('<form method="post" action="/ticket/reply">%s<input type="hidden" name="id" value="%d">'
                               '<div class="field"><span>Reply</span><textarea name="message" maxlength="%d" required></textarea></div>'
                               '<div class="actions"><button class="btn primary" type="submit">%sSend reply</button></div></form>') % (
                self.csrf_field(), ticket['id'], MESSAGE_MAX, icon('send', 'i sm')))

        details = [('Type', esc(CATEGORIES.get(ticket['category'], ticket['category']))), ('Status', status_tag(ticket['status'])),
                   ('Opened', fmt_time(ticket['created'])), ('Updated', fmt_ago(ticket['updated']))]
        if ticket['characterName']:
            details.append(('Character', esc(ticket['characterName'])))
        if s['staff']:
            details += [('Account', '%d (bnet %d)' % (ticket['accountId'], ticket['battlenetAccountId'])), ('Realm', esc(realm_name(ticket['realmId']))),
                        ('Position', '<span class="mono">map %d zone %d<br>%.1f %.1f %.1f</span>' % (
                            ticket['mapId'], ticket['zoneId'], ticket['posX'], ticket['posY'], ticket['posZ']))]
        info = '<div class="tbl-wrap"><table class="tbl"><tbody>%s</tbody></table></div>' % ''.join(
            '<tr><td class="muted">%s</td><td>%s</td></tr>' % (k, v) for k, v in details)
        action = 'reopen' if ticket['status'] == 'closed' else 'close'
        if action == 'close' or s['staff']:
            info += ('<div class="panel-b"><form method="post" action="/ticket/status">%s<input type="hidden" name="id" value="%d">'
                     '<input type="hidden" name="action" value="%s"><button class="btn %s" type="submit">%s%s ticket</button></form></div>') % (
                self.csrf_field(), ticket['id'], action, 'danger' if action == 'close' else '', icon('x' if action == 'close' else 'undo', 'i sm'),
                action.title())
        body += '<div class="cols"><div class="stack">%s</div>%s</div>' % (conv, panel('Details', info, flush=True))
        self.page('#%d %s' % (ticket['id'], ticket['subject']), body)

    def ticket_reply(self):
        ticket = self.load_ticket(self.form.get('id'))
        message = self.form.get('message', '').strip()[:MESSAGE_MAX]
        if not ticket or ticket['status'] == 'closed' or not message:
            return self.redirect('/ticket?id=%d' % ticket['id'] if ticket else '/tickets')
        s = self.session
        is_staff_reply = s['staff'] and ticket['battlenetAccountId'] != s['bnet']
        now = int(time.time())
        execute(auth_db(), 'INSERT INTO support_ticket_replies (ticketId, accountId, isStaff, message, created) VALUES (%s, %s, %s, %s, %s)',
                (ticket['id'], s['account'], 1 if is_staff_reply else 0, message, now))
        execute(auth_db(), 'UPDATE support_tickets SET status = %s, updated = %s WHERE id = %s',
                ('answered' if is_staff_reply else 'open', now, ticket['id']))
        if is_staff_reply:
            self.notify_player(ticket)
        self.redirect('/ticket?id=%d' % ticket['id'])

    def notify_player(self, ticket):
        """Tells the player in game (by mail) that staff replied, when RA is configured."""
        if not CFG.get('notify_by_mail', True) or not ticket['characterName'] or not ra_available(ticket['realmId']):
            return
        try:
            out = ra_command(ticket['realmId'], 'send mail %s "%s" "%s"' % (
                ticket['characterName'], ra_quote('Support ticket #%d answered' % ticket['id']),
                ra_quote('A game master replied to your ticket "%s". Open Help (question mark) > My tickets to read it.' % ticket['subject'])))
            log('notify', ticket['id'], out)
        except Exception as e:
            log('notify failed', ticket['id'], e)

    def ticket_status(self):
        ticket = self.load_ticket(self.form.get('id'))
        action = self.form.get('action')
        if ticket and (action == 'close' or (action == 'reopen' and self.session['staff'])):
            execute(auth_db(), 'UPDATE support_tickets SET status = %s, updated = %s WHERE id = %s',
                    ('closed' if action == 'close' else 'open', int(time.time()), ticket['id']))
        self.redirect('/ticket?id=%d' % ticket['id'] if ticket else '/tickets')

    # armory (public)

    def armory(self, path):
        ip = self.client_address[0]
        parts = [p for p in path.split('/') if p][1:]
        bucket, limit = ('search', int(CFG.get('armory_search_per_minute', 20))) if not parts else ('page', int(CFG.get('armory_pages_per_minute', 60)))
        if not rate_ok(ip, bucket, limit):
            return self.page('Slow down', alert('bad', 'Too many requests. Please wait a minute and try again.'), status=429, section='Armory')
        if not parts:
            return self.armory_search()
        if len(parts) == 2 and parts[0].isdigit():
            return self.armory_character(int(parts[0]), parts[1])
        return self.page('Not found', '<p class="muted">That page does not exist.</p>', status=404, section='Armory')

    def armory_search(self):
        q = ' '.join(self.query.get('q', '').split())[:60]
        form = ('<form method="get" action="/armory" class="armory-search"><input type="search" name="q" value="%s" placeholder="Name, surname or both" '
                'maxlength="60" autofocus><button class="btn primary" type="submit">%sSearch</button></form>') % (esc(q), icon('search', 'i sm'))
        if len(q) < 2:
            return self.page('Armory', form, 'Look up any character on %s: type a name, a surname, or both.' % esc(CFG.get('server_name', 'Forever')),
                             section='Armory')
        first, _, last = q.partition(' ')
        like = lambda s: s.replace('\\', '\\\\').replace('%', '\\%').replace('_', '\\_') + '%'
        results = []
        for realm_id in CFG['realms']:
            if last:
                where, args = 'name LIKE %s AND surname LIKE %s', (like(first), like(last))
            else:
                where, args = '(name LIKE %s OR surname LIKE %s)', (like(first), like(first))
            for c in query(characters_db(realm_id), 'SELECT name, surname, race, class, level, online FROM characters '
                                                    'WHERE deleteInfos_Account IS NULL AND ' + where + ' ORDER BY level DESC, name LIMIT 50', args):
                c['realm'] = int(realm_id)
                results.append(c)
        results.sort(key=lambda c: (-c['level'], c['name']))
        races, classes = names('race'), names('class')
        if results:
            rows = ''.join(
                '<tr><td><a class="row-link" href="/armory/%d/%s"><span class="c%d"><b>%s</b></span>%s</a></td><td>%s</td><td class="num">%d</td>'
                '<td class="hide-sm">%s <span class="c%d">%s</span></td><td class="hide-sm muted">%s</td></tr>' % (
                    c['realm'], esc(c['name']), c['class'], esc(c['name']), ' <span class="muted">%s</span>' % esc(c['surname']) if c['surname'] else '',
                    faction(c['race']), c['level'], esc(races.get(c['race'], ('',))[0]), c['class'], esc(classes.get(c['class'], ('',))[0]),
                    esc(realm_name(c['realm']))) for c in results[:100])
            body = form + panel('Characters', '<div class="tbl-wrap"><table class="tbl"><thead><tr><th>Character</th><th>H/A</th><th class="num">Level</th>'
                                '<th class="hide-sm">Class</th><th class="hide-sm">Realm</th></tr></thead><tbody>%s</tbody></table></div>' % rows,
                                sub='· %d' % len(results), flush=True)
        else:
            body = form + panel('', '<div class="empty">No character found for "%s".</div>' % esc(q), flush=True)
        self.page('Armory', body, section='Armory')

    def armory_character(self, realm_id, name):
        if str(realm_id) not in CFG['realms']:
            return self.page('Not found', '<p class="muted">That realm does not exist.</p>', status=404, section='Armory')
        cdb = characters_db(realm_id)
        c = query_one(cdb, 'SELECT guid, name, surname, race, class, gender, level, totalKills, todayKills, online FROM characters '
                           'WHERE name = %s AND deleteInfos_Account IS NULL', (name,))
        if not c:
            return self.page('Not found', '<p class="muted">No character called %s on %s.</p>' % (esc(name), esc(realm_name(realm_id))),
                             status=404, section='Armory')
        guild = query_one(cdb, 'SELECT g.name FROM guild_member m JOIN guild g ON g.guildid = m.guildid WHERE m.guid = %s', (c['guid'],))
        gear = {r['slot']: r['itemEntry'] for r in query(cdb, 'SELECT ci.slot, ii.itemEntry FROM character_inventory ci JOIN item_instance ii '
                                                              'ON ii.guid = ci.item WHERE ci.guid = %s AND ci.bag = 0 AND ci.slot < 19', (c['guid'],))}
        tips = item_tooltip_data(sorted(set(gear.values())))
        races, classes, skills = names('race'), names('class'), names('skill')

        def slot(s):
            entry = gear.get(s)
            tip = tips.get(entry)
            if not tip:
                return '<div class="slot empty" title="%s"></div>' % esc(SLOT_NAMES[s])
            return '<div class="slot q%d" data-item="%d"><img src="%s" alt="%s" loading="lazy"></div>' % (tip['q'], entry, esc(tip['icon']), esc(tip['name']))

        portrait = icon_url(CLASS_ICONS.get(c['class']))
        doll = ('<div class="doll"><div class="col">%s</div><div class="figure"><img src="%s" alt=""><div class="figure-cap">%s<br><span class="muted">%s</span>'
                '</div></div><div class="col">%s</div><div class="bottom">%s</div></div>') % (
            ''.join(slot(s) for s in DOLL_LEFT), esc(portrait), esc(races.get(c['race'], ('',))[0]), esc(classes.get(c['class'], ('',))[0]),
            ''.join(slot(s) for s in DOLL_RIGHT), ''.join(slot(s) for s in DOLL_BOTTOM))

        st = query_one(cdb, 'SELECT * FROM character_stats WHERE guid = %s', (c['guid'],))
        if st:
            groups = [('Attributes', [('Strength', st['strength']), ('Agility', st['agility']), ('Stamina', st['stamina']), ('Intellect', st['intellect']),
                                      ('Spirit', st['spirit'])]),
                      ('Melee', [('Attack power', st['attackPower']), ('Critical', '%.2f%%' % st['critPct']), ('Dodge', '%.2f%%' % st['dodgePct']),
                                 ('Parry', '%.2f%%' % st['parryPct'])]),
                      ('Ranged & spell', [('Ranged power', st['rangedAttackPower']), ('Ranged critical', '%.2f%%' % st['rangedCritPct']),
                                          ('Spell critical', '%.2f%%' % st['spellCritPct']), ('Spell power', st['spellPower'])]),
                      ('Defense', [('Armor', st['armor']), ('Health', st['maxhealth']), ('Block', '%.2f%%' % st['blockPct']),
                                   ('Resilience', st['resilience'])]),
                      ('Resistances', [('Arcane', st['resArcane']), ('Fire', st['resFire']), ('Nature', st['resNature']), ('Frost', st['resFrost']),
                                       ('Shadow', st['resShadow'])])]
            stats = '<div class="stat-groups">%s</div>' % ''.join(
                '<div><h3>%s</h3>%s</div>' % (g, ''.join('<div class="kv"><span class="muted">%s</span><span>%s</span></div>' % (k, esc(v)) for k, v in rows))
                for g, rows in groups)
        else:
            stats = '<div class="empty">Stats appear after the character logs out once.</div>'

        sk = query(cdb, 'SELECT skill, value, max FROM character_skills WHERE guid = %s', (c['guid'],))
        def skill_rows(category):
            # Classic has a parent and a child skill line with the same name (Cooking 185 and 2939): one row per name, highest value;
            # racial lines can carry the secondary-skill category
            best = {}
            for r in sk:
                n = skills.get(r['skill'])
                if not n or n[2] != category or 'Racial' in n[0]:
                    continue
                if n[0] not in best or (r['value'], r['max']) > (best[n[0]][1]['value'], best[n[0]][1]['max']):
                    best[n[0]] = (n, r)
            rows = list(best.values())
            return ''.join('<div class="stub"><img src="%s" alt=""><span>%s</span><span class="v">%d / %d</span></div>' % (
                esc(icon_url(n[1], 'medium')), esc(n[0]), r['value'], r['max']) for n, r in sorted(rows, key=lambda x: x[0][0])) or \
                '<div class="muted small">None</div>'

        achs = names('achievement')
        recent = query(cdb, 'SELECT achievement, date FROM character_achievement WHERE guid = %s ORDER BY date DESC LIMIT 6', (c['guid'],))
        recent_html = ''.join('<div class="stub"><img src="%s" alt=""><span>%s</span><span class="v">%s</span></div>' % (
            esc(icon_url(achs.get(a['achievement'], ('', ''))[1], 'medium')), esc(achs.get(a['achievement'], ('Achievement %d' % a['achievement'],))[0]),
            fmt_ago(a['date'])) for a in recent) or '<div class="muted small">Nothing yet</div>'

        side = ('<div class="stack">%s%s%s%s</div>') % (
            panel('Professions', skill_rows(SKILL_PROFESSION)), panel('Secondary skills', skill_rows(SKILL_SECONDARY)),
            panel('Player vs Player', '<div class="stub"><span>Honorable kills</span><span class="v">%d</span></div><div class="stub"><span>Kills today</span>'
                  '<span class="v">%d</span></div>' % (c['totalKills'], c['todayKills'])),
            panel('Recent achievements', recent_html))
        header = ('<div class="char-head"><div><h1><span class="c%d">%s</span>%s</h1><p class="lead">Level %d %s <span class="c%d">%s</span> · %s%s</p>'
                  '</div><div>%s%s</div></div>') % (
            c['class'], esc(c['name']), ' <span class="surname">%s</span>' % esc(c['surname']) if c['surname'] else '', c['level'],
            esc(races.get(c['race'], ('',))[0]), c['class'], esc(classes.get(c['class'], ('',))[0]), esc(realm_name(realm_id)),
            ' · &lt;%s&gt;' % esc(guild['name']) if guild else '', faction(c['race']), ' <span class="tag green">Online</span>' if c['online'] else '')
        body = header + '<div class="cols">%s%s</div>' % ('<div class="stack">%s%s</div>' % (panel('Equipment', doll), panel('Character stats', stats)), side)
        data = '<script>window.ARMORY_ITEMS=%s;</script><script src="/static/armory.js?v=%s" defer></script>' % (json.dumps(
            {str(k): v for k, v in tips.items()}).replace('</', '<\\/'), STATIC_VERSION)
        self.page(c['name'], body + data, section='Armory', heading=False)

    # staff

    def staff(self):
        if not self.session['staff']:
            return self.page('Not found', '<p class="muted">That page does not exist.</p>', status=404)
        status = self.query.get('status', 'open')
        if status not in STATUS:
            status = 'open'
        counts = {r['status']: r['n'] for r in query(auth_db(), 'SELECT status, COUNT(*) AS n FROM support_tickets GROUP BY status')}
        tickets = query(auth_db(), 'SELECT id, characterName, category, subject, status, updated FROM support_tickets WHERE status = %s '
                                   'ORDER BY updated ' + ('ASC' if status == 'open' else 'DESC') + ' LIMIT 200', (status,))
        tabs = '<div class="seg">%s</div>' % ''.join('<a href="/staff?%s" class="%s">%s<span class="n">%d</span></a>' % (
            urlencode({'status': k}), 'on' if k == status else '', esc(label), counts.get(k, 0)) for k, (label, _) in STATUS.items())
        self.page('Staff', panel('Tickets', self.ticket_rows(tickets, True), right=tabs, flush=True),
                  'Every ticket from every player. Oldest waiting first, so nobody is forgotten.')

    # shop (player): points, premium, selling gold on the Gold Market

    def shop_page(self, note=''):
        s = self.session
        acc = shop_account(s['account'])
        percent = gold_fee_percent()
        max_listings = int(CFG.get('gold_market_max_listings', 10))
        cdb = characters_db(s['realm'])

        status = kv_rows([
            ('Shop points', '<span class="big-num">%s</span>' % '{:,}'.format(acc['Points'])),
            ('Premium', '<span class="tag green">Premium</span>' if acc['Premium'] else '<span class="tag">Not premium</span>'),
            ('Realm', esc(realm_name(s['realm']))),
        ])
        status += '<p class="muted small">Spend points in the in-game Shop (the Shop button in the main menu or on the character screen). %s</p>' % (
            CFG.get('shop_points_text') or 'Points come from supporting the server.')

        listings = query(cdb, 'SELECT l.ID, l.Gold, l.Price, UNIX_TIMESTAMP(l.Created) AS created, c.name FROM shop_gold_listing l '
                              'LEFT JOIN characters c ON c.guid = l.SellerGuid WHERE l.SellerAccount = %s ORDER BY l.Gold DESC', (s['account'],))
        if listings:
            rows = ''.join(
                '<tr><td class="num"><b>%s</b> gold</td><td class="num">%d</td><td class="num">%d</td><td>%s</td><td class="hide-sm">%s</td>'
                '<td><form method="post" action="/shop/gold/cancel" class="inline">%s<input type="hidden" name="id" value="%d">'
                '<button class="btn sm ghost" type="submit">Cancel</button></form></td></tr>' % (
                    '{:,}'.format(l['Gold']), l['Price'], l['Price'] - gold_fee(l['Price'], percent), esc(l['name'] or '?'), esc(fmt_ago(l['created'])),
                    self.csrf_field(), l['ID']) for l in listings)
            mine = ('<div class="tbl-wrap"><table class="tbl"><thead><tr><th class="num">Gold</th><th class="num">Price</th><th class="num">You get</th>'
                    '<th>Character</th><th class="hide-sm">Listed</th><th></th></tr></thead><tbody>%s</tbody></table></div>') % rows
        else:
            mine = '<div class="empty">You have no gold listed.</div>'

        if acc['GoldMarketBanned']:
            sell = alert('bad', 'You may not sell gold on the Gold Market.%s' % (
                ' Reason: %s' % esc(acc['GoldMarketBanReason']) if acc['GoldMarketBanReason'] else ''))
        elif not acc['Premium']:
            sell = ('<p>Selling gold is for <b>premium</b> accounts. Your account becomes premium with any point purchase.</p>'
                    '<p class="muted">Other players buy your gold in the in-game Shop with their points; you get the points, minus the service fee.</p>')
        elif not ra_available(s['realm']):
            sell = alert('info', 'Selling gold is not available on this realm right now.')
        elif len(listings) >= max_listings:
            sell = alert('info', 'You have %d listings, the most you can have at once. Cancel one or wait until one sells.' % len(listings))
        else:
            chars = query(cdb, 'SELECT guid, name, class, money FROM characters WHERE account = %s AND deleteInfos_Account IS NULL ORDER BY name',
                          (s['account'],))
            options = ''.join('<option value="%d"%s>%s (%s gold)</option>' % (
                c['guid'], ' selected' if c['guid'] == s['char'] else '', esc(c['name']), '{:,}'.format(int(c['money']) // 10000)) for c in chars)
            sell = (
                '<form method="post" action="/shop/gold/list">%s'
                '<div class="field"><span>Character (the gold is taken from it right away)</span><select name="guid" required>%s</select></div>'
                '<div class="form-row"><div class="field"><span>Gold</span><input type="number" name="gold" min="1" max="%d" step="1" required></div>'
                '<div class="field"><span>Price in points (at least %d)</span><input type="number" id="gm-price" name="price" min="%d" step="1" required></div></div>'
                '<div class="calc">You get <b id="gm-net">-</b> when it sells. Service fee: %d%%, at least 1 point.</div>'
                '<div class="actions"><button class="btn primary" type="submit">%sList gold</button>'
                '<span class="muted small">Cancel any time: the gold comes back by mail.</span></div></form>'
                '<script>(function(){var f=document.getElementById("gm-price"),o=document.getElementById("gm-net"),p=%d;'
                'function u(){var v=parseInt(f.value,10)||0,fee=Math.floor(v*p/100);if(p&&fee<1)fee=1;'
                'o.textContent=v>=%d?(v-fee)+" points (fee "+fee+")":"-";}f.addEventListener("input",u);u();})();</script>') % (
                self.csrf_field(), options, GOLD_MARKET_MAX_GOLD, GOLD_MARKET_MIN_PRICE, GOLD_MARKET_MIN_PRICE, percent, icon('coin', 'i sm'),
                percent, GOLD_MARKET_MIN_PRICE) if chars else '<div class="empty">No characters on this realm.</div>'

        # realm transfers of this account (bought in the in-game Shop at the character screen)
        realm_names = {r['id']: r['name'] for r in query(auth_db(), 'SELECT id, name FROM realmlist')}
        transfers = query(auth_db(), 'SELECT Name, SourceRealm, TargetRealm, Status, Message, UNIX_TIMESTAMP(Created) AS created FROM character_transfer '
                                     'WHERE AccountID = %s ORDER BY ID DESC LIMIT 20', (s['account'],))
        if transfers:
            trs = ''.join('<tr><td><b>%s</b></td><td>%s &rarr; %s</td><td>%s</td><td class="hide-sm">%s</td></tr>' % (
                esc(t['Name']), esc(realm_names.get(t['SourceRealm'], t['SourceRealm'])), esc(realm_names.get(t['TargetRealm'], t['TargetRealm'])),
                '<span class="tag green">Done</span>' if t['Status'] == 1 else
                ('<span class="tag red">Failed</span> <span class="muted small">%s - a GM will help</span>' % esc(t['Message']) if t['Status'] == 2 else
                 '<span class="tag amber">On its way</span>'), esc(fmt_ago(t['created']))) for t in transfers)
            moves = ('<div class="tbl-wrap"><table class="tbl"><thead><tr><th>Character</th><th>Realms</th><th>Status</th><th class="hide-sm">When</th></tr>'
                     '</thead><tbody>%s</tbody></table></div>') % trs
        else:
            moves = '<div class="empty">No realm transfers. Buy one in the Shop on the character screen (Services).</div>'

        body = note + '<div class="cols"><div class="stack">%s%s</div><div class="stack">%s%s</div></div>' % (
            panel('Sell gold', sell, sub='Gold Market'),
            panel('My listings', mine, sub='%d of %d' % (len(listings), max_listings), flush=True),
            panel('Shop account', status, right='<span class="tag green">Premium</span>' if acc['Premium'] else ''),
            panel('My realm transfers', moves, flush=True))
        self.page('Shop', body, 'Your shop points, and gold you sell to other players for points.')

    def shop_gold_list(self):
        s = self.session
        acc = shop_account(s['account'])
        if not acc['Premium'] or acc['GoldMarketBanned'] or not ra_available(s['realm']):
            return self.shop_page(alert('bad', 'Your account can not sell gold.'))
        if not rate_ok(self.client_address[0], 'goldlist', 10):
            return self.shop_page(alert('bad', 'Too many tries. Wait a minute and try again.'))
        guid, gold, price = to_int(self.form.get('guid')), to_int(self.form.get('gold')), to_int(self.form.get('price'))
        char = query_one(characters_db(s['realm']), 'SELECT guid, name, money FROM characters WHERE guid = %s AND account = %s AND deleteInfos_Account IS NULL',
                         (guid, s['account']))
        if not char:
            return self.shop_page(alert('bad', 'Please pick one of your characters.'))
        if not 1 <= gold <= GOLD_MARKET_MAX_GOLD:
            return self.shop_page(alert('bad', 'The gold must be between 1 and %s.' % '{:,}'.format(GOLD_MARKET_MAX_GOLD)))
        if price < GOLD_MARKET_MIN_PRICE:
            return self.shop_page(alert('bad', 'The price must be at least %d points (the service fee is at least 1 point).' % GOLD_MARKET_MIN_PRICE))
        count = query_one(characters_db(s['realm']), 'SELECT COUNT(*) AS n FROM shop_gold_listing WHERE SellerAccount = %s', (s['account'],))['n']
        if count >= int(CFG.get('gold_market_max_listings', 10)):
            return self.shop_page(alert('bad', 'You already have the most listings you can have at once.'))
        out = ra_command(s['realm'], 'shop goldlist %s %d %d' % (char['name'], gold, price))
        log('gold market list', char['name'], gold, price, '->', out)
        if out.startswith('OK'):
            return self.shop_page(alert('ok', '%s gold of %s is on the Gold Market for %d points.' % ('{:,}'.format(gold), esc(char['name']), price)))
        self.shop_page(alert('bad', 'That did not work: %s.' % esc(out.replace('ERROR', '').strip() or 'unknown error')))

    def shop_gold_cancel(self):
        s = self.session
        listing = query_one(characters_db(s['realm']), 'SELECT ID, Gold FROM shop_gold_listing WHERE ID = %s AND SellerAccount = %s',
                            (to_int(self.form.get('id')), s['account']))
        if not listing or not ra_available(s['realm']):
            return self.shop_page(alert('bad', 'That listing is gone (sold already?).'))
        out = ra_command(s['realm'], 'shop goldcancel %d' % listing['ID'])
        log('gold market cancel', listing['ID'], '->', out)
        if out.startswith('OK'):
            return self.shop_page(alert('ok', 'Listing cancelled. %s gold was sent back to the character by mail.' % '{:,}'.format(listing['Gold'])))
        self.shop_page(alert('bad', 'That did not work: %s.' % esc(out.replace('ERROR', '').strip() or 'unknown error')))

    # shop admin (GM): products, tabs, Gold Market, accounts (points, premium, bans), settings

    def is_shop_admin(self):
        s = self.session
        return bool(s and s['staff'] and s.get('level', 0) >= int(CFG.get('shop_admin_level', 3)))

    def shop_admin_page(self, tab, body, note=''):
        tabs = (('products', 'Products'), ('market', 'Gold Market'), ('transfers', 'Transfers'), ('accounts', 'Accounts'), ('settings', 'Settings'))
        nav = '<div class="seg">%s</div>' % ''.join('<a href="/staff/shop?tab=%s" class="%s">%s</a>' % (k, 'on' if k == tab else '', l) for k, l in tabs)
        self.page('Shop admin', note + '<div class="actions" style="margin:0 0 16px">%s</div>' % nav + body,
                  'Products of the in-game Shop, the Gold Market and shop accounts. Changes to products reach the game at once.')

    def staff_shop(self, note=''):
        if not self.is_shop_admin():
            return self.page('Not found', '<p class="muted">That page does not exist.</p>', status=404)
        tab = self.query.get('tab', 'products')
        if tab == 'market':
            return self.shop_admin_market(note)
        if tab == 'accounts':
            return self.shop_admin_accounts(note)
        if tab == 'settings':
            return self.shop_admin_settings(note)
        if tab == 'transfers':
            return self.shop_admin_transfers(note)
        return self.shop_admin_products(note)

    # realm transfers (worldserver TokenHandler.cpp: auth.character_transfer written by the source realm, loaded by the target realm
    # within a minute; auth.character_transfer_rule = allowed source -> target pairs)

    def shop_admin_transfers(self, note=''):
        realms = query(auth_db(), 'SELECT id, name, flag FROM realmlist ORDER BY id')
        names = {r['id']: r['name'] for r in realms}
        rules = {(r['SourceRealm'], r['TargetRealm']): r['DeadOnly'] for r in query(auth_db(), 'SELECT * FROM character_transfer_rule')}

        head = ''.join('<th class="num">%s</th>' % esc(r['name']) for r in realms)
        rows = ''
        for s in realms:
            cells = ''
            for t in realms:
                if s['id'] == t['id']:
                    cells += '<td class="num muted">-</td>'
                    continue
                key = '%d_%d' % (s['id'], t['id'])
                allowed = (s['id'], t['id']) in rules
                cells += ('<td class="num"><label class="pick" title="Allowed"><input type="checkbox" name="allow_%s" value="1"%s> Allow</label><br>'
                          '<label class="pick small" title="Only characters that died"><input type="checkbox" name="dead_%s" value="1"%s> Dead only</label></td>') % (
                    key, ' checked' if allowed else '', key, ' checked' if rules.get((s['id'], t['id'])) else '')
            rows += '<tr><td><b>%s</b></td>%s</tr>' % (esc(s['name']), cells)
        grid = ('<form method="post" action="/staff/shop/transfers/rules">%s<div class="tbl-wrap"><table class="tbl"><thead><tr><th>From \\ To</th>%s</tr>'
                '</thead><tbody>%s</tbody></table></div><div class="panel-b"><p class="muted small">Only ticked transfers are possible. "Dead only": the '
                'character must have died (released ghost, or lying dead) - for Hardcore.</p><div class="actions"><button class="btn primary" type="submit">'
                'Save rules</button></div></div></form>') % (self.csrf_field(), head, rows)

        status = self.query.get('status', '')
        where, args = ('WHERE Status = %s', (int(status),)) if status in ('0', '1', '2') else ('', ())
        transfers = query(auth_db(), 'SELECT ID, AccountID, SourceRealm, TargetRealm, Name, Status, Message, UNIX_TIMESTAMP(Created) AS created, '
                                     'UNIX_TIMESTAMP(Finished) AS finished FROM character_transfer ' + where + ' ORDER BY ID DESC LIMIT 200', args)
        counts = {r['Status']: r['n'] for r in query(auth_db(), 'SELECT Status, COUNT(*) AS n FROM character_transfer GROUP BY Status')}
        filters = '<div class="seg">%s</div>' % ''.join('<a href="/staff/shop?tab=transfers&status=%s" class="%s">%s<span class="n">%d</span></a>' % (
            k, 'on' if k == status else '', l, sum(counts.values()) if k == '' else counts.get(int(k), 0))
            for k, l in (('', 'All'), ('0', 'Waiting'), ('2', 'Failed'), ('1', 'Done')))
        trs = ''
        for t in transfers:
            if t['Status'] == 1:
                state = '<span class="tag green">Done</span>'
            elif t['Status'] == 2:
                state = '<span class="tag red">Failed</span> <span class="muted small">%s</span>' % esc(t['Message'])
            else:
                state = '<span class="tag amber">Waiting</span>'
            actions = ''
            if t['Status'] in (0, 2):
                actions = ('<form method="post" action="/staff/shop/transfers/action" class="inline">%s<input type="hidden" name="id" value="%d">'
                           '%s<button class="btn sm ghost" type="submit" name="do" value="return" title="Load the character back on the realm it came from">'
                           'Return</button></form>') % (
                    self.csrf_field(), t['ID'], '<button class="btn sm" type="submit" name="do" value="retry">Retry</button> ' if t['Status'] == 2 else '')
            trs += '<tr><td class="num">%d</td><td><b>%s</b></td><td><a href="/staff/shop?tab=accounts&id=%d">%s</a></td><td>%s &rarr; %s</td>' \
                   '<td>%s</td><td class="hide-sm">%s</td><td>%s</td></tr>' % (
                       t['ID'], esc(t['Name']), t['AccountID'], esc(self.account_label(t['AccountID'])), esc(names.get(t['SourceRealm'], t['SourceRealm'])),
                       esc(names.get(t['TargetRealm'], t['TargetRealm'])), state, esc(fmt_ago(t['created'])), actions)
        history = ('<div class="tbl-wrap"><table class="tbl"><thead><tr><th class="num">#</th><th>Character</th><th>Account</th><th>Realms</th>'
                   '<th>Status</th><th class="hide-sm">When</th><th></th></tr></thead><tbody>%s</tbody></table></div>') % trs \
            if transfers else '<div class="empty">No transfers.</div>'
        body = '<div class="stack">%s%s</div>' % (
            panel('Allowed transfers', grid, flush=True),
            panel('Transfers', history, sub='The target realm loads a character within a minute of the purchase (or when it starts).',
                  right=filters, flush=True))
        self.shop_admin_page('transfers', body, note)

    def staff_shop_transfer_rules(self):
        if not self.is_shop_admin():
            return self.redirect('/')
        realms = [r['id'] for r in query(auth_db(), 'SELECT id FROM realmlist')]
        execute(auth_db(), 'DELETE FROM character_transfer_rule')
        saved = 0
        for s in realms:
            for t in realms:
                if s != t and self.form.get('allow_%d_%d' % (s, t)):
                    execute(auth_db(), 'INSERT INTO character_transfer_rule (SourceRealm, TargetRealm, DeadOnly) VALUES (%s, %s, %s)',
                            (s, t, 1 if self.form.get('dead_%d_%d' % (s, t)) else 0))
                    saved += 1
        log('shop admin: transfer rules saved,', saved, 'allowed, by account', self.session['account'])
        self.query = {'tab': 'transfers'}
        self.staff_shop(alert('ok', 'Transfer rules saved: %d allowed.' % saved))

    def staff_shop_transfer_action(self):
        if not self.is_shop_admin():
            return self.redirect('/')
        tid, action = to_int(self.form.get('id')), self.form.get('do')
        transfer = query_one(auth_db(), 'SELECT ID, Name, SourceRealm, TargetRealm, Status FROM character_transfer WHERE ID = %s', (tid,))
        self.query = {'tab': 'transfers'}
        if not transfer or transfer['Status'] == 1:
            return self.staff_shop(alert('bad', 'That transfer is finished already.'))
        if action == 'retry':
            execute(auth_db(), "UPDATE character_transfer SET Status = 0, Message = '', Finished = NULL WHERE ID = %s AND Status = 2", (tid,))
            text = '%s will be loaded on the target realm again (within a minute while it runs).' % esc(transfer['Name'])
        else:
            # the source realm loads its own dump back like any arriving transfer
            execute(auth_db(), "UPDATE character_transfer SET TargetRealm = SourceRealm, Status = 0, Message = 'returned by a GM', Finished = NULL "
                               "WHERE ID = %s AND Status <> 1", (tid,))
            text = '%s goes back to %s (within a minute while that realm runs).' % (esc(transfer['Name']), esc(realm_name(transfer['SourceRealm'])))
        log('shop admin: transfer', tid, action, 'by account', self.session['account'])
        self.staff_shop(alert('ok', text))

    def shop_admin_products(self, note=''):
        wdb = world_db()
        groups = query(wdb, 'SELECT ID, Name, IconFileDataID, Ordering FROM shop_group ORDER BY Ordering, ID')
        products = query(wdb, 'SELECT ID, GroupID, Name, Price, NormalPrice, Enabled FROM shop_product ORDER BY GroupID, Ordering, ID')
        actions = {}
        for a in query(wdb, 'SELECT ProductID, Action, Param1, Param2 FROM shop_product_action ORDER BY ProductID, `Index`'):
            actions.setdefault(a['ProductID'], []).append(shop_action_text(a['Action'], a['Param1'], a['Param2']))
        group_names = {g['ID']: g['Name'] for g in groups}

        rows = ''.join(
            '<tr><td>%s</td><td><a href="/staff/shop/product?id=%d"><b>%s</b></a></td><td class="num">%d%s</td><td class="hide-sm">%s</td><td>%s</td></tr>' % (
                esc(group_names.get(p['GroupID'], '?')), p['ID'], esc(p['Name']), p['Price'],
                ' <span class="muted"><s>%d</s></span>' % p['NormalPrice'] if p['NormalPrice'] > p['Price'] else '',
                esc(', '.join(actions.get(p['ID'], [])) or 'nothing'),
                '<span class="tag green">On sale</span>' if p['Enabled'] else '<span class="tag">Off</span>') for p in products)
        table = ('<div class="tbl-wrap"><table class="tbl"><thead><tr><th>Tab</th><th>Product</th><th class="num">Points</th><th class="hide-sm">Gives</th>'
                 '<th>Status</th></tr></thead><tbody>%s</tbody></table></div>') % rows if products else '<div class="empty">No products yet.</div>'
        presets = ''.join('<option value="%s">%s</option>' % (k, esc(v[0])) for k, v in SHOP_PRESETS.items())
        new_form = ('<form method="get" action="/staff/shop/product" class="actions" style="margin:0"><select name="preset" style="width:auto">%s</select>'
                    '<button class="btn primary" type="submit">%sNew product</button></form>') % (presets, icon('plus', 'i sm'))

        group_rows = ''.join(
            '<tr><td colspan="4"><form method="post" action="/staff/shop/group/save" class="form-row" style="align-items:end">%s'
            '<input type="hidden" name="id" value="%d"><div class="field"><span>Name</span><input type="text" name="name" value="%s" maxlength="255" required></div>'
            '<div class="field"><span>Icon (FileDataID)</span><input type="number" name="icon" value="%d"></div>'
            '<div class="field"><span>Order</span><input type="number" name="ordering" value="%d"></div>'
            '<div class="field"><span>&nbsp;</span><div class="actions" style="margin:0"><button class="btn sm" type="submit">Save</button>'
            '<a class="btn sm danger" href="/staff/shop/confirm?what=group&id=%d">Delete</a></div></div></form></td></tr>' % (
                self.csrf_field(), g['ID'], esc(g['Name']), g['IconFileDataID'], g['Ordering'], g['ID']) for g in groups)
        add_group = ('<tr><td colspan="4"><form method="post" action="/staff/shop/group/save" class="form-row" style="align-items:end">%s'
                     '<input type="hidden" name="id" value="0"><div class="field"><span>New tab</span><input type="text" name="name" maxlength="255" required></div>'
                     '<div class="field"><span>Icon (FileDataID)</span><input type="number" name="icon" value="134400"></div>'
                     '<div class="field"><span>Order</span><input type="number" name="ordering" value="%d"></div>'
                     '<div class="field"><span>&nbsp;</span><div class="actions" style="margin:0"><button class="btn sm primary" type="submit">Add tab</button></div></div>'
                     '</form></td></tr>') % (self.csrf_field(), len(groups) + 1)
        groups_table = '<div class="tbl-wrap"><table class="tbl"><tbody>%s%s</tbody></table></div>' % (group_rows, add_group)

        body = '<div class="stack">%s%s</div>' % (
            panel('Products', table, sub='%d' % len(products), right=new_form, flush=True),
            panel('Tabs', groups_table, sub='The "Gold Market" tab is added by the server when gold is listed.', flush=True))
        self.shop_admin_page('products', body, note)

    def staff_shop_product(self, note='', values=None):
        if not self.is_shop_admin():
            return self.page('Not found', '<p class="muted">That page does not exist.</p>', status=404)
        wdb = world_db()
        groups = query(wdb, 'SELECT ID, Name FROM shop_group ORDER BY Ordering, ID')
        if not groups:
            return self.staff_shop(alert('bad', 'Add a tab first.'))
        pid = to_int(self.query.get('id'))
        if values is None:
            product = query_one(wdb, 'SELECT * FROM shop_product WHERE ID = %s', (pid,)) if pid else None
            if product:
                acts = [(a['Action'], a['Param1'], a['Param2']) for a in
                        query(wdb, 'SELECT Action, Param1, Param2 FROM shop_product_action WHERE ProductID = %s ORDER BY `Index`', (pid,))]
                values = dict(product, actions=acts)
            else:
                label, name, desc, ic, acts = SHOP_PRESETS.get(self.query.get('preset', 'item'), SHOP_PRESETS['item'])
                values = {'ID': 0, 'GroupID': groups[0]['ID'], 'Name': name, 'Description': desc, 'IconFileDataID': ic, 'Price': 10, 'NormalPrice': 0,
                          'Ordering': 0, 'Enabled': 1, 'actions': acts}
        acts = list(values['actions']) + [(0, 0, 0)] * (6 - len(values['actions']))

        group_opts = ''.join('<option value="%d"%s>%s</option>' % (g['ID'], ' selected' if g['ID'] == values['GroupID'] else '', esc(g['Name'])) for g in groups)

        def options(items, current):
            return ''.join('<option value="%d"%s>%s</option>' % (k, ' selected' if k == current else '', esc(v)) for k, v in items)

        # one row per thing the product gives; the labels and controls follow the type (script below), every control is in the form
        act_rows = ''
        for i, (a, p1, p2) in enumerate(acts[:max(6, len(acts))]):
            label1, label2 = SHOP_ACTION_HINTS.get(a, ('', ''))
            act_rows += (
                '<tr class="act-row"><td><select name="a%d" class="act-type">%s</select></td>'
                '<td><span class="cap c1">%s</span><input type="number" name="p1_%d" class="v-num" value="%d">'
                '<select name="svc_%d" class="v-svc">%s</select><select name="prof_%d" class="v-prof">%s</select></td>'
                '<td><span class="cap c2">%s</span><input type="number" name="p2_%d" class="v2" value="%d"></td></tr>') % (
                i, '<option value="0">Nothing</option>' + options(SHOP_ACTIONS.items(), a), esc(label1), i, p1 // 10000 if a == 2 else p1,
                i, options(SHOP_SERVICES.items(), p1), i, options(PROFESSIONS.items(), p1), esc(label2), i, p2 or (300 if a == 5 else 0))
        script = ('<script>(function(){var L=%s;function u(r){var a=r.querySelector(".act-type").value,l=L[a]||["",""];'
                  'r.querySelector(".c1").textContent=l[0];r.querySelector(".c2").textContent=l[1];'
                  'r.querySelector(".v-num").style.display=(a=="0"||a=="4"||a=="5"||a=="9")?"none":"";'
                  'r.querySelector(".v-svc").style.display=a=="4"?"":"none";r.querySelector(".v-prof").style.display=a=="5"?"":"none";'
                  'r.querySelector(".v2").style.display=l[1]?"":"none";}'
                  'var rows=document.querySelectorAll(".act-row");for(var i=0;i<rows.length;++i){(function(r){u(r);'
                  'r.querySelector(".act-type").addEventListener("change",function(){u(r);});})(rows[i]);}})();</script>') % json.dumps(
            {str(k): list(v) for k, v in SHOP_ACTION_HINTS.items()})
        hints = ''.join('<li><b>%s</b>: %s</li>' % (SHOP_ACTIONS[k], esc(' / '.join(h for h in v if h))) for k, v in SHOP_ACTION_HINTS.items())

        form = (
            '<form method="post" action="/staff/shop/product/save">%s<input type="hidden" name="id" value="%d">'
            '<div class="form-row"><div class="field"><span>Name</span><input type="text" name="name" value="%s" maxlength="255" required></div>'
            '<div class="field"><span>Tab</span><select name="group">%s</select></div></div>'
            '<div class="field"><span>Description</span><textarea name="description" maxlength="8000" style="min-height:70px">%s</textarea></div>'
            '<div class="form-row"><div class="field"><span>Price (points)</span><input type="number" name="price" min="0" value="%d" required></div>'
            '<div class="field"><span>Was (crossed out, 0 = none)</span><input type="number" name="normal" min="0" value="%d"></div>'
            '<div class="field"><span>Icon (FileDataID)</span><input type="number" name="icon" value="%d"></div>'
            '<div class="field"><span>Order in the tab</span><input type="number" name="ordering" value="%d"></div></div>'
            '<div class="field"><label class="pick"><input type="checkbox" name="enabled" value="1"%s> On sale</label></div>'
            '<div class="tbl-wrap"><table class="tbl acts"><thead><tr><th>Gives</th><th>Value</th><th></th></tr></thead>'
            '<tbody>%s</tbody></table></div>'
            '<div class="actions"><button class="btn primary" type="submit">Save</button><a class="btn ghost" href="/staff/shop">Back</a>%s</div></form>%s') % (
            self.csrf_field(), values['ID'], esc(values['Name']), group_opts, esc(values['Description']), values['Price'], values['NormalPrice'],
            values['IconFileDataID'], values['Ordering'], ' checked' if values['Enabled'] else '', act_rows,
            '<a class="btn danger" href="/staff/shop/confirm?what=product&id=%d">Delete</a>' % values['ID'] if values['ID'] else '', script)
        help_text = ('<ul class="perks">%s</ul><p class="muted small">Anything but items is applied to the character the player is logged in with, '
                     'so those products can only be bought in game. Items are mailed. A profession is only raised when the character already has it.</p>'
                     '<p class="muted small">Icons are FileDataIDs (wowhead shows them on an icon\'s page).</p>') % hints
        self.page('Edit product' if values['ID'] else 'New product', note + '<div class="cols"><div class="stack">%s</div><div class="stack">%s</div></div>' % (
            panel('Product', form), panel('What a product can give', help_text)), section='Support')

    def staff_shop_confirm(self):
        """Are you sure? page before deleting a product or a tab (the in-game browser has no confirm dialogs)."""
        if not self.is_shop_admin():
            return self.page('Not found', '<p class="muted">That page does not exist.</p>', status=404)
        what, oid = self.query.get('what'), to_int(self.query.get('id'))
        table, action, back = {'product': ('shop_product', '/staff/shop/product/delete', '/staff/shop/product?id=%d' % oid),
                               'group': ('shop_group', '/staff/shop/group/delete', '/staff/shop')}.get(what, (None, None, None))
        row = query_one(world_db(), 'SELECT Name FROM %s WHERE ID = %%s' % table, (oid,)) if table else None
        if not row:
            return self.staff_shop(alert('bad', 'That is gone already.'))
        body = ('<p>Delete the %s <b>%s</b>? This can not be undone.%s</p><form method="post" action="%s">%s<input type="hidden" name="id" value="%d">'
                '<div class="actions"><button class="btn danger" type="submit">Yes, delete</button><a class="btn ghost" href="%s">Go back</a></div></form>') % (
            'product' if what == 'product' else 'tab', esc(row['Name']),
            ' Players who bought it keep what they got.' if what == 'product' else '', action, self.csrf_field(), oid, back)
        self.page('Delete', panel('Are you sure?', body))

    def staff_shop_product_save(self):
        if not self.is_shop_admin():
            return self.redirect('/')
        f = self.form
        values = {'ID': to_int(f.get('id')), 'GroupID': to_int(f.get('group')), 'Name': f.get('name', '').strip()[:255],
                  'Description': f.get('description', '').strip()[:8000], 'IconFileDataID': to_int(f.get('icon')), 'Price': max(0, to_int(f.get('price'))),
                  'NormalPrice': max(0, to_int(f.get('normal'))), 'Ordering': to_int(f.get('ordering')), 'Enabled': 1 if f.get('enabled') else 0,
                  'actions': []}
        for i in range(12):
            a = to_int(f.get('a%d' % i))
            if a not in SHOP_ACTIONS:
                continue
            p1, p2 = to_int(f.get('p1_%d' % i)), to_int(f.get('p2_%d' % i))
            if a == 2:
                p1 *= 10000                     # entered in gold, stored in copper
            elif a == 4:
                p1 = to_int(f.get('svc_%d' % i))
            elif a == 5:
                p1, p2 = to_int(f.get('prof_%d' % i)), p2 or 300
            if not SHOP_ACTION_HINTS[a][1]:
                p2 = 0
            if a == 1:
                p2 = max(1, p2)
            values['actions'].append((a, p1, p2))
        wdb = world_db()
        if not values['Name']:
            return self.staff_shop_product(alert('bad', 'The product needs a name.'), values)
        if not query_one(wdb, 'SELECT ID FROM shop_group WHERE ID = %s', (values['GroupID'],)):
            return self.staff_shop_product(alert('bad', 'Pick a tab.'), values)
        if not values['actions']:
            return self.staff_shop_product(alert('bad', 'The product must give something.'), values)
        # actions are checked by the worldserver when it loads them (invalid ones are logged and skipped)
        pid = values['ID']
        if not pid:
            pid = int(query_one(wdb, 'SELECT COALESCE(MAX(ID), 0) + 1 AS id FROM shop_product')['id'])
        execute(wdb, 'REPLACE INTO shop_product (ID, GroupID, Name, Description, IconFileDataID, Price, NormalPrice, Ordering, Enabled) '
                     'VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s)',
                (pid, values['GroupID'], values['Name'], values['Description'], values['IconFileDataID'], values['Price'], values['NormalPrice'],
                 values['Ordering'], values['Enabled']))
        execute(wdb, 'DELETE FROM shop_product_action WHERE ProductID = %s', (pid,))
        for i, (a, p1, p2) in enumerate(values['actions']):
            execute(wdb, 'INSERT INTO shop_product_action (ProductID, `Index`, Action, Param1, Param2) VALUES (%s, %s, %s, %s, %s)', (pid, i, a, p1, p2))
        log('shop admin: product', pid, values['Name'], 'saved by account', self.session['account'])
        self.after_shop_change('Saved "%s".' % esc(values['Name']))

    def after_shop_change(self, text):
        failed = shop_reload_all()
        note = alert('ok', text + ' It is in the game now.') if not failed else alert(
            'info', text + ' Not reloaded on: %s (no console access); it shows there after a restart.' % esc(', '.join(failed)))
        self.query = {'tab': 'products'}
        self.staff_shop(note)

    def staff_shop_product_delete(self):
        if not self.is_shop_admin():
            return self.redirect('/')
        pid = to_int(self.form.get('id'))
        execute(world_db(), 'DELETE FROM shop_product_action WHERE ProductID = %s', (pid,))
        execute(world_db(), 'DELETE FROM shop_product WHERE ID = %s', (pid,))
        log('shop admin: product', pid, 'deleted by account', self.session['account'])
        self.after_shop_change('Product deleted.')

    def staff_shop_group_save(self):
        if not self.is_shop_admin():
            return self.redirect('/')
        gid, name = to_int(self.form.get('id')), self.form.get('name', '').strip()[:255]
        if not name:
            return self.staff_shop(alert('bad', 'The tab needs a name.'))
        if not gid:
            gid = int(query_one(world_db(), 'SELECT COALESCE(MAX(ID), 0) + 1 AS id FROM shop_group')['id'])
        execute(world_db(), 'REPLACE INTO shop_group (ID, Name, IconFileDataID, Ordering) VALUES (%s, %s, %s, %s)',
                (gid, name, to_int(self.form.get('icon')), to_int(self.form.get('ordering'))))
        self.after_shop_change('Tab "%s" saved.' % esc(name))

    def staff_shop_group_delete(self):
        if not self.is_shop_admin():
            return self.redirect('/')
        gid = to_int(self.form.get('id'))
        if query_one(world_db(), 'SELECT COUNT(*) AS n FROM shop_product WHERE GroupID = %s', (gid,))['n']:
            return self.staff_shop(alert('bad', 'That tab still has products. Move or delete them first.'))
        execute(world_db(), 'DELETE FROM shop_group WHERE ID = %s', (gid,))
        self.after_shop_change('Tab deleted.')

    def account_label(self, account_id):
        row = query_one(auth_db(), 'SELECT a.username, b.email FROM account a LEFT JOIN battlenet_accounts b ON b.id = a.battlenet_account WHERE a.id = %s',
                        (account_id,))
        if not row:
            return 'account %d' % account_id
        return '%s (#%d)' % (row['email'] or row['username'], account_id)

    def shop_admin_market(self, note=''):
        percent = gold_fee_percent()
        search = self.query.get('q', '').strip()
        rows = []
        for realm_id in CFG['realms']:
            try:
                for l in query(characters_db(realm_id), 'SELECT l.ID, l.SellerAccount, l.Gold, l.Price, UNIX_TIMESTAMP(l.Created) AS created, c.name '
                                                        'FROM shop_gold_listing l LEFT JOIN characters c ON c.guid = l.SellerGuid ORDER BY l.Gold DESC'):
                    l['realm'] = realm_id
                    rows.append(l)
            except pymysql.MySQLError:
                continue
        labels = {a: self.account_label(a) for a in {r['SellerAccount'] for r in rows} if a}
        if search:
            rows = [r for r in rows if search.lower() in (r['name'] or '').lower() or search.lower() in labels.get(r['SellerAccount'], '').lower()
                    or search == str(r['SellerAccount'])]
        total_gold = sum(r['Gold'] for r in rows)
        trs = ''.join(
            '<tr><td class="num"><b>%s</b></td><td class="num">%d</td><td>%s</td><td>%s</td><td class="hide-sm">%s</td><td class="hide-sm">%s</td>'
            '<td><form method="post" action="/staff/shop/listing/remove" class="inline">%s<input type="hidden" name="realm" value="%s">'
            '<input type="hidden" name="id" value="%d"><button class="btn sm danger" type="submit" title="Gold is mailed back to the seller">Remove</button></form></td></tr>' % (
                '{:,}'.format(r['Gold']), r['Price'], esc(r['name'] or 'server'),
                '<a href="/staff/shop?tab=accounts&id=%d">%s</a>' % (r['SellerAccount'], esc(labels[r['SellerAccount']])) if r['SellerAccount'] else
                '<span class="muted">server</span>', esc(realm_name(r['realm'])), esc(fmt_ago(r['created'])), self.csrf_field(), esc(r['realm']), r['ID'])
            for r in rows)
        table = ('<div class="tbl-wrap"><table class="tbl"><thead><tr><th class="num">Gold</th><th class="num">Points</th><th>Character</th><th>Account</th>'
                 '<th class="hide-sm">Realm</th><th class="hide-sm">Listed</th><th></th></tr></thead><tbody>%s</tbody></table></div>') % trs \
            if rows else '<div class="empty">No gold listed.</div>'
        find = ('<form method="get" action="/staff/shop" class="actions" style="margin:0"><input type="hidden" name="tab" value="market">'
                '<input type="search" name="q" value="%s" placeholder="Character, account or e-mail" style="width:220px"><button class="btn sm" type="submit">Find</button></form>') % esc(search)
        body = panel('Listings', table, sub='%d listings, %s gold, fee %d%%' % (len(rows), '{:,}'.format(total_gold), percent), right=find, flush=True)
        self.shop_admin_page('market', body, note)

    def staff_shop_listing_remove(self):
        if not self.is_shop_admin():
            return self.redirect('/')
        realm_id, lid = self.form.get('realm', ''), to_int(self.form.get('id'))
        if not realm_cfg(realm_id) or not ra_available(realm_id):
            return self.staff_shop(alert('bad', 'No console access to that realm.'))
        out = ra_command(realm_id, 'shop goldcancel %d' % lid)
        log('shop admin: listing', realm_id, lid, 'removed by account', self.session['account'], '->', out)
        self.query['tab'] = 'market'
        self.staff_shop(alert('ok', 'Listing removed, the gold was mailed back to the seller.') if out.startswith('OK') else
                        alert('bad', 'That did not work: %s.' % esc(out)))

    def shop_admin_accounts(self, note=''):
        aid = to_int(self.query.get('id'))
        search = self.query.get('q', '').strip()
        if aid:
            if not query_one(auth_db(), 'SELECT id FROM account WHERE id = %s', (aid,)):
                return self.shop_admin_page('accounts', alert('bad', 'No such account.'), note)
            acc = shop_account(aid)
            listings = 0
            for realm_id in CFG['realms']:
                try:
                    listings += query_one(characters_db(realm_id), 'SELECT COUNT(*) AS n FROM shop_gold_listing WHERE SellerAccount = %s', (aid,))['n']
                except pymysql.MySQLError:
                    pass
            bought = query(auth_db(), 'SELECT ProductID, Price, UNIX_TIMESTAMP(Time) AS t FROM shop_purchase_log WHERE AccountID = %s ORDER BY ID DESC LIMIT 15', (aid,))
            log_rows = ''.join('<tr><td>%d</td><td class="num">%d</td><td>%s</td></tr>' % (b['ProductID'], b['Price'], esc(fmt_ago(b['t']))) for b in bought)
            history = ('<div class="tbl-wrap"><table class="tbl"><thead><tr><th>Product</th><th class="num">Points</th><th>When</th></tr></thead><tbody>%s</tbody>'
                       '</table></div>') % log_rows if bought else '<div class="empty">No purchases.</div>'
            form = (
                '<form method="post" action="/staff/shop/account/save">%s<input type="hidden" name="id" value="%d">%s'
                '<div class="form-row"><div class="field"><span>Add points (negative to take)</span><input type="number" name="delta" value="0"></div></div>'
                '<div class="field"><label class="pick"><input type="checkbox" name="premium" value="1"%s> Premium (may sell gold)</label></div>'
                '<div class="field"><label class="pick"><input type="checkbox" name="banned" value="1"%s> Banned from the Gold Market</label></div>'
                '<div class="field"><span>Ban reason (shown to the player)</span><input type="text" name="reason" value="%s" maxlength="255"></div>'
                '<div class="field"><label class="pick"><input type="checkbox" name="remove" value="1"> Remove all their listings now (gold is mailed back)</label></div>'
                '<div class="actions"><button class="btn primary" type="submit">Save</button><a class="btn ghost" href="/staff/shop?tab=accounts">Back</a></div></form>') % (
                self.csrf_field(), aid, kv_rows([('Points', '<span class="big-num">%s</span>' % '{:,}'.format(acc['Points'])),
                                                 ('Gold listed', '%d listings' % listings)]),
                ' checked' if acc['Premium'] else '', ' checked' if acc['GoldMarketBanned'] else '', esc(acc['GoldMarketBanReason']))
            body = '<div class="cols"><div class="stack">%s</div><div class="stack">%s</div></div>' % (
                panel(esc(self.account_label(aid)), form), panel('Recent purchases', history, flush=True))
            return self.shop_admin_page('accounts', body, note)

        if search:
            like = '%' + search + '%'
            found = query(auth_db(), 'SELECT a.id FROM account a LEFT JOIN battlenet_accounts b ON b.id = a.battlenet_account '
                                     'WHERE a.id = %s OR a.username LIKE %s OR b.email LIKE %s ORDER BY a.id LIMIT 50', (to_int(search, -1), like, like))
            title = 'Search: %s' % esc(search)
        else:
            found = query(auth_db(), 'SELECT AccountID AS id FROM account_shop_points WHERE Premium <> 0 OR GoldMarketBanned <> 0 OR Points <> 0 '
                                     'ORDER BY AccountID LIMIT 200')
            title = 'Accounts with points, premium or a ban'
        trs = ''
        for f in found:
            acc = shop_account(f['id'])
            trs += '<tr><td><a href="/staff/shop?tab=accounts&id=%d"><b>%s</b></a></td><td class="num">%s</td><td>%s%s</td></tr>' % (
                f['id'], esc(self.account_label(f['id'])), '{:,}'.format(acc['Points']),
                '<span class="tag green">Premium</span> ' if acc['Premium'] else '', '<span class="tag red">Banned</span>' if acc['GoldMarketBanned'] else '')
        table = ('<div class="tbl-wrap"><table class="tbl"><thead><tr><th>Account</th><th class="num">Points</th><th>Status</th></tr></thead><tbody>%s</tbody>'
                 '</table></div>') % trs if found else '<div class="empty">Nothing found.</div>'
        find = ('<form method="get" action="/staff/shop" class="actions" style="margin:0"><input type="hidden" name="tab" value="accounts">'
                '<input type="search" name="q" value="%s" placeholder="Account id, name or e-mail" style="width:220px"><button class="btn sm" type="submit">Find</button></form>') % esc(search)
        self.shop_admin_page('accounts', panel(title, table, right=find, flush=True), note)

    def staff_shop_account_save(self):
        if not self.is_shop_admin():
            return self.redirect('/')
        f = self.form
        aid = to_int(f.get('id'))
        if not query_one(auth_db(), 'SELECT id FROM account WHERE id = %s', (aid,)):
            return self.staff_shop(alert('bad', 'No such account.'))
        acc = shop_account(aid)
        points = max(0, acc['Points'] + to_int(f.get('delta')))
        execute(auth_db(), 'INSERT INTO account_shop_points (AccountID, Points, Premium, GoldMarketBanned, GoldMarketBanReason) VALUES (%s, %s, %s, %s, %s) '
                           'ON DUPLICATE KEY UPDATE Points = VALUES(Points), Premium = VALUES(Premium), GoldMarketBanned = VALUES(GoldMarketBanned), '
                           'GoldMarketBanReason = VALUES(GoldMarketBanReason)',
                (aid, points, 1 if f.get('premium') else 0, 1 if f.get('banned') else 0, f.get('reason', '').strip()[:255]))
        removed = 0
        if f.get('remove'):
            for realm_id in CFG['realms']:
                if not ra_available(realm_id):
                    continue
                for l in query(characters_db(realm_id), 'SELECT ID FROM shop_gold_listing WHERE SellerAccount = %s', (aid,)):
                    if ra_command(realm_id, 'shop goldcancel %d' % l['ID']).startswith('OK'):
                        removed += 1
        log('shop admin: account', aid, 'points', points, 'premium', bool(f.get('premium')), 'banned', bool(f.get('banned')), 'removed', removed,
            'by account', self.session['account'])
        self.query.update({'tab': 'accounts', 'id': str(aid)})
        self.staff_shop(alert('ok', 'Saved. Points: %s.%s' % ('{:,}'.format(points), ' %d listings removed.' % removed if removed else '')))

    def shop_admin_settings(self, note=''):
        form = ('<form method="post" action="/staff/shop/settings">%s<div class="form-row"><div class="field"><span>Gold Market service fee (%%)</span>'
                '<input type="number" name="fee" min="0" max="100" value="%d"></div></div>'
                '<p class="muted small">Kept back from the seller when gold sells; at least 1 point while it is not 0. Used from the next sale on.</p>'
                '<div class="actions"><button class="btn primary" type="submit">Save</button></div></form>') % (self.csrf_field(), gold_fee_percent())
        self.shop_admin_page('settings', panel('Gold Market', form), note)

    def staff_shop_settings(self):
        if not self.is_shop_admin():
            return self.redirect('/')
        fee = max(0, min(100, to_int(self.form.get('fee'), 10)))
        execute(auth_db(), "REPLACE INTO shop_config (Name, Value) VALUES ('GoldMarketFee', %s)", (str(fee),))
        log('shop admin: fee', fee, 'by account', self.session['account'])
        self.query['tab'] = 'settings'
        self.staff_shop(alert('ok', 'The service fee is now %d%%.' % fee))


# URLs the client itself opens (GlobalStrings VISITABLE_URL50/6/7 hotfix) -> page
DIRECT_ENTRY = {
    '/kb': '/',
    '/help/ticket': '/ticket/new',
    '/help/tickets': '/tickets',
}

ROUTES = {
    ('GET', '/'): Handler.home,
    ('GET', '/faq'): Handler.faq,
    ('GET', '/account'): Handler.account_page,
    ('POST', '/account/authenticator/start'): Handler.authenticator_start,
    ('POST', '/account/authenticator/cancel'): Handler.authenticator_cancel,
    ('POST', '/account/authenticator/confirm'): Handler.authenticator_confirm,
    ('POST', '/account/authenticator/remove'): Handler.authenticator_remove,
    ('GET', '/unstuck'): Handler.unstuck_form,
    ('POST', '/unstuck'): Handler.unstuck,
    ('GET', '/ticket/new'): Handler.ticket_new_form,
    ('POST', '/ticket/new'): Handler.ticket_create,
    ('GET', '/tickets'): Handler.ticket_list,
    ('GET', '/ticket'): Handler.ticket_view,
    ('POST', '/ticket/reply'): Handler.ticket_reply,
    ('POST', '/ticket/status'): Handler.ticket_status,
    ('GET', '/staff'): Handler.staff,
    ('GET', '/shop'): Handler.shop_page,
    ('POST', '/shop/gold/list'): Handler.shop_gold_list,
    ('POST', '/shop/gold/cancel'): Handler.shop_gold_cancel,
    ('GET', '/staff/shop'): Handler.staff_shop,
    ('GET', '/staff/shop/product'): Handler.staff_shop_product,
    ('GET', '/staff/shop/confirm'): Handler.staff_shop_confirm,
    ('POST', '/staff/shop/product/save'): Handler.staff_shop_product_save,
    ('POST', '/staff/shop/product/delete'): Handler.staff_shop_product_delete,
    ('POST', '/staff/shop/group/save'): Handler.staff_shop_group_save,
    ('POST', '/staff/shop/group/delete'): Handler.staff_shop_group_delete,
    ('POST', '/staff/shop/listing/remove'): Handler.staff_shop_listing_remove,
    ('POST', '/staff/shop/account/save'): Handler.staff_shop_account_save,
    ('POST', '/staff/shop/settings'): Handler.staff_shop_settings,
    ('POST', '/staff/shop/transfers/rules'): Handler.staff_shop_transfer_rules,
    ('POST', '/staff/shop/transfers/action'): Handler.staff_shop_transfer_action,
}


def load_config(path):
    global CFG
    with open(path, encoding='utf-8') as f:
        CFG = json.load(f)
    # paths in the config may be relative to the config file (repack: web\support_site.json -> ..\server\forever.cert.pem)
    base = os.path.dirname(os.path.abspath(path))
    for key in ('cert', 'key'):
        if CFG.get(key) and not os.path.isabs(CFG[key]):
            CFG[key] = os.path.normpath(os.path.join(base, CFG[key]))
    # no RA account of its own: use the launcher's console account (localservers.json "consoleUser"/"consolePassword")
    ra = CFG.setdefault('ra', {})
    if not ra.get('user') and ra.get('localservers'):
        try:
            with open(os.path.normpath(os.path.join(base, ra['localservers'])), encoding='utf-8-sig') as f:
                for server in json.load(f).values():
                    if server.get('consoleUser'):
                        ra['user'], ra['password'] = server['consoleUser'], server.get('consolePassword', '')
                        break
        except (OSError, ValueError) as e:
            print('RA account from localservers.json:', e)


def ensure_armory_export():
    """The armory needs world.armory_item/armory_name (".armory export" on a worldserver); run it through RA if they are empty."""
    try:
        if query_one(world_db(), 'SELECT COUNT(*) AS n FROM armory_item')['n']:
            return
        for realm_id in CFG['realms']:
            if ra_available(realm_id):
                log('armory: tables empty, running "armory export" on realm', realm_id, '->', ra_command(realm_id, 'armory export'))
                return
    except Exception as e:
        log('armory export failed:', e)


def make_token(bnet_account_id, realm_id):
    account = query_one(auth_db(), 'SELECT id FROM account WHERE battlenet_account = %s ORDER BY battlenet_index LIMIT 1', (bnet_account_id,))
    if not account:
        sys.exit('no game account for battle.net account %s' % bnet_account_id)
    token = 'TUS-%s-%s' % (secrets.token_hex(16), bnet_account_id)
    now = int(time.time())
    execute(auth_db(), 'INSERT INTO battlenet_sso_tokens (token, battlenetAccountId, accountId, realmId, characterGuid, issued, expires, ip) '
                       'VALUES (%s, %s, %s, %s, 0, %s, %s, %s)', (token, bnet_account_id, account['id'], realm_id, now, now + 3600, '127.0.0.1'))
    print('https://%s:%d/s?%s' % (CFG.get('public_host', 'trinity.actual.battle.net'), int(CFG.get('port', 443)),
                                  urlencode({'token': token, 'ref': 'test'})))


class NewsHandler(Handler):
    """Plain HTTP (news_http_port, default 80): only the Breaking News; the game refuses certificates outside the Windows root store."""

    def route(self, method):
        if method == 'GET' and urlsplit(self.path).path.startswith('/service/'):
            return self.breaking_news()
        return self.send(404, b'', 'text/plain')


class TLSServer(ThreadingHTTPServer):
    """TLS handshake per connection, in its thread: a failed handshake is logged (wrapping the listening socket drops it silently)."""

    def __init__(self, address, handler, ctx):
        super().__init__(address, handler)
        self.ctx = ctx

    def finish_request(self, request, client_address):
        try:
            conn = self.ctx.wrap_socket(request, server_side=True)
        except (ssl.SSLError, OSError) as e:
            log('TLS handshake from', client_address[0], 'failed:', e)
            return
        try:
            super().finish_request(conn, client_address)
        finally:
            conn.close()


def main():
    args = sys.argv[1:]
    if args and args[0] == '--make-token':
        load_config(args[3] if len(args) > 3 else os.path.join(HERE, 'support_site.json'))
        return make_token(int(args[1]), int(args[2]))
    load_config(args[0] if args else os.path.join(HERE, 'support_site.json'))
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(CFG['cert'], CFG['key'])
    threading.Thread(target=ensure_armory_export, daemon=True).start()
    srv = TLSServer((CFG.get('listen', '0.0.0.0'), int(CFG.get('port', 443))), Handler, ctx)
    news_port = int(CFG.get('news_http_port', 80))
    if news_port:
        try:
            news = ThreadingHTTPServer((CFG.get('listen', '0.0.0.0'), news_port), NewsHandler)
            threading.Thread(target=news.serve_forever, daemon=True).start()
            log('breaking news (plain http) on port', news_port)
        except OSError as e:
            log('breaking news: port', news_port, 'not available:', e)
    log('support site on port', CFG.get('port', 443), '- RA', 'configured' if (CFG.get('ra') or {}).get('user') else 'not configured')
    srv.serve_forever()


if __name__ == '__main__':
    main()
