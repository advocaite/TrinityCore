"""Turns client_diff.py's report.json into one self-contained HTML page."""
import html

QCLASS = {'Poor': 'q0', 'Common': 'q1', 'Uncommon': 'q2', 'Rare': 'q3', 'Epic': 'q4', 'Legendary': 'q5'}

CSS = """
/* Layout: one reading column, a summary ledger up top, then findings grouped by what they mean for players. */
:root {
  --bg: #f3f4f7; --panel: #ffffff; --ink: #1c2230; --muted: #5c6577; --rule: #d9dde6;
  --accent: #2f5fa8; --miss: #b4232c; --diff: #a86200; --new: #1d7a55;
  --q0: #6b6b6b; --q1: #2a2a2a; --q2: #1a7d12; --q3: #0a5bb5; --q4: #8a2fc9; --q5: #c45f00;
  --display: "Alegreya Sans SC", "Trebuchet MS", sans-serif;
  --body: "IBM Plex Sans", "Segoe UI", system-ui, sans-serif;
  --mono: "IBM Plex Mono", ui-monospace, Consolas, monospace;
}
@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) {
  --bg: #12151c; --panel: #1a1f29; --ink: #e3e7ef; --muted: #98a1b3; --rule: #2d3442;
  --accent: #7ea6e8; --miss: #ef6b6b; --diff: #e3a54a; --new: #4fc28e;
  --q0: #9d9d9d; --q1: #e6e6e6; --q2: #3fd43a; --q3: #4a9eff; --q4: #b874f2; --q5: #ff9a3c; color-scheme: dark } }
:root[data-theme="dark"] {
  --bg: #12151c; --panel: #1a1f29; --ink: #e3e7ef; --muted: #98a1b3; --rule: #2d3442;
  --accent: #7ea6e8; --miss: #ef6b6b; --diff: #e3a54a; --new: #4fc28e;
  --q0: #9d9d9d; --q1: #e6e6e6; --q2: #3fd43a; --q3: #4a9eff; --q4: #b874f2; --q5: #ff9a3c; color-scheme: dark }
body { background: var(--bg); color: var(--ink); font: 15px/1.55 var(--body); }
.wrap { max-width: 1080px; margin: 0 auto; padding-block: 28px 64px;
  padding-left: max(16px, env(safe-area-inset-left, 0px)); padding-right: max(16px, env(safe-area-inset-right, 0px)); }
h1 { font: 600 2.1rem/1.15 var(--display); letter-spacing: .02em; margin: 0; text-wrap: balance; }
h2 { font: 600 1.45rem/1.2 var(--display); letter-spacing: .03em; margin: 0; text-wrap: balance; }
h3 { font: 600 1.02rem/1.3 var(--body); margin: 0; }
p { margin: 0; max-width: 72ch; }
a { color: var(--accent); }
.meta { color: var(--muted); font-size: .9rem; display: flex; flex-wrap: wrap; gap: 4px 18px; }
.mono, td.n, .id { font-family: var(--mono); font-variant-numeric: tabular-nums; }
.stack { display: grid; gap: 14px; }
header.top { display: grid; gap: 10px; padding-bottom: 22px; border-bottom: 1px solid var(--rule); }
.ledger { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 12px; margin-block: 22px; }
.ledger a { text-decoration: none; color: inherit; background: var(--panel); border: 1px solid var(--rule);
  border-top: 4px solid var(--c); padding: 14px 16px; display: grid; gap: 4px; }
.ledger a:hover, .ledger a:focus-visible { border-color: var(--c); outline: none; }
.ledger .big { font: 600 2rem/1 var(--mono); color: var(--c); }
.ledger .lbl { font: 600 .78rem/1.3 var(--body); letter-spacing: .08em; text-transform: uppercase; color: var(--muted); }
.miss { --c: var(--miss); } .diff { --c: var(--diff); } .new { --c: var(--new); } .chg { --c: var(--accent); }
.notice { background: var(--panel); border: 1px solid var(--rule); padding: 18px 20px; display: grid; gap: 10px; }
.notice ol { margin: 0; padding-left: 1.3em; display: grid; gap: 8px; }
.notice li::marker { color: var(--muted); font-family: var(--mono); }
section.group { margin-top: 38px; display: grid; gap: 16px; }
section.group > .gh { display: grid; gap: 6px; padding-left: 14px; border-left: 4px solid var(--c); }
details { background: var(--panel); border: 1px solid var(--rule); }
summary { cursor: pointer; padding: 12px 16px; display: flex; flex-wrap: wrap; align-items: baseline; gap: 6px 12px; }
summary:focus-visible { outline: 2px solid var(--accent); outline-offset: -2px; }
summary .cnt { font: 600 .95rem var(--mono); color: var(--c); min-width: 3.5ch; }
summary .why { color: var(--muted); font-size: .9rem; flex-basis: 100%; }
.body { padding: 0 16px 16px; display: grid; gap: 10px; }
.tools { display: flex; gap: 10px; align-items: center; flex-wrap: wrap; }
.tools input { font: inherit; padding: 6px 10px; border: 1px solid var(--rule); background: var(--bg); color: var(--ink); min-width: 0; width: 260px; max-width: 100%; }
.tools input:focus-visible { outline: 2px solid var(--accent); }
.tools .shown { color: var(--muted); font-size: .85rem; }
.tbl { overflow-x: auto; max-height: 560px; overflow-y: auto; border: 1px solid var(--rule); }
table { border-collapse: collapse; width: 100%; font-size: .86rem; }
th { position: sticky; top: 0; background: var(--panel); text-align: left; font-weight: 600; color: var(--muted);
  border-bottom: 1px solid var(--rule); padding: 6px 10px; white-space: nowrap; }
td { border-bottom: 1px solid var(--rule); padding: 5px 10px; vertical-align: top; }
td.wide { min-width: 22ch; }
.q0 { color: var(--q0); } .q1 { color: var(--q1); } .q2 { color: var(--q2); } .q3 { color: var(--q3); }
.q4 { color: var(--q4); } .q5 { color: var(--q5); }
.pill { font: 600 .72rem/1 var(--body); letter-spacing: .05em; text-transform: uppercase; padding: 3px 6px;
  border: 1px solid currentColor; white-space: nowrap; }
.foot { margin-top: 40px; color: var(--muted); font-size: .85rem; display: grid; gap: 6px; }
code { font-family: var(--mono); font-size: .85em; }
@media (max-width: 560px) { h1 { font-size: 1.6rem; } .ledger .big { font-size: 1.6rem; } }
"""

JS = """
document.querySelectorAll('[data-filter]').forEach(function (inp) {
  var tb = document.getElementById(inp.dataset.filter), out = inp.parentNode.querySelector('.shown');
  inp.addEventListener('input', function () {
    var q = inp.value.toLowerCase(), n = 0;
    tb.querySelectorAll('tr').forEach(function (tr) {
      var hit = !q || tr.textContent.toLowerCase().indexOf(q) >= 0; tr.hidden = !hit; if (hit) n++;
    });
    out.textContent = n + ' shown';
  });
});
"""


def e(x):
    return html.escape('' if x is None else str(x))


def link_item(i, name, quality=None):
    q = QCLASS.get(quality, '')
    nm = e(name) if name and name != '?' else 'item %d' % i
    return '<a class="%s" href="https://www.wowhead.com/classic/item=%d" target="_blank" rel="noopener">%s</a>' % (q, i, nm)


def link_spell(i, name):
    nm = e(name) if name and name != '?' else 'spell %d' % i
    return '<a href="https://www.wowhead.com/classic/spell=%d" target="_blank" rel="noopener">%s</a>' % (i, nm)


class Page:
    def __init__(self):
        self.n = 0

    def table(self, cols, rows, cls):
        """cols: [(header, fn(row)->html, td class)]"""
        self.n += 1
        tid = 't%d' % self.n
        head = ''.join('<th>%s</th>' % e(c[0]) for c in cols)
        body = ''.join('<tr>%s</tr>' % ''.join('<td class="%s">%s</td>' % (c[2], c[1](r)) for c in cols) for r in rows)
        tools = ''
        if len(rows) > 12:
            tools = ('<div class="tools"><input id="f%s" type="search" placeholder="Filter rows" data-filter="%s" '
                     'aria-label="Filter rows"><span class="shown">%d shown</span></div>' % (tid, tid, len(rows)))
        return '%s<div class="tbl"><table><thead><tr>%s</tr></thead><tbody id="%s">%s</tbody></table></div>' % (
            tools, head, tid, body)

    def finding(self, kind, title, count, why, inner, open_=False):
        return ('<details class="%s"%s><summary><span class="cnt">%s</span><h3>%s</h3><span class="why">%s</span>'
                '</summary><div class="body">%s</div></details>') % (kind, ' open' if open_ else '', e(count), e(title),
                                                                     why, inner)


def render(d):
    P = Page()
    old, new = d['old_label'], d['new_label']
    q = d.get('quests', {})
    loot = d.get('loot_missing_items', [])
    loot_sp = [x for x in loot if x['spawned_sources']]
    info_p = [x for x in d.get('classic_info_differ', []) if x['player']]
    cost_p = [x for x in d.get('classic_cost_differ', []) if x['player']]
    untaught = d.get('class_spells_untaught', [])
    sp_ch_p = [x for x in d.get('spells_changed', []) if x['player']]
    epic_ch = [x for x in d.get('items_changed', []) if x['quality'] in ('Epic', 'Legendary') and x.get('ilvl_delta')]
    hot_tbl = d.get('hotfix_tables', [])
    hot_diff = d.get('hotfix_other_differ', []) + d.get('hotfix_items_differ', [])
    tr_lvl = d.get('trainer_level_differs', [])
    masking = d.get('value_rows_masking_change', [])

    def cnt(*xs):
        return sum(len(x) if isinstance(x, list) else x for x in xs)

    n_missing = cnt(loot_sp, d.get('vendor_missing_items', []), q.get('reward_missing', []), q.get('no_giver', []),
                    untaught, d.get('trainer_unknown_spells', []), d.get('creature_display_missing', []))
    n_differ = cnt(info_p, cost_p, masking, tr_lvl, hot_diff)
    n_new = cnt(d.get('new_spells_unused', []), d.get('new_items_unobtainable', []))
    n_changed = cnt(d.get('items_changed', []), d.get('spells_changed', []), d.get('talents_changed', []))

    out = ['<title>%s Client Audit</title>' % e(new),
           '<link rel="preconnect" href="https://fonts.googleapis.com"><link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>',
           '<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Alegreya+Sans+SC:wght@600&family=IBM+Plex+Mono:wght@400;600&family=IBM+Plex+Sans:wght@400;600&display=swap">',
           '<style>%s</style><div class="wrap">' % CSS]
    out.append('<header class="top"><h1>Client %s audit</h1>'
               '<p>What the %s client changed since %s, and where our server data (hotfixes, vanilla overrides, loot, '
               'trainers, quests) is missing something, holds a different value, or leaves new client content unused.</p>'
               '<div class="meta"><span>Generated %s</span><span>Old build %s</span><span>New build %s</span>'
               '<span>Data: server backup at build 245, plus the queued 246 duration fixes</span></div></header>'
               % (e(new), e(new), e(old), e(d['generated']), e(old), e(new)))
    out.append('<nav class="ledger" aria-label="Summary">'
               '<a class="miss" href="#missing"><span class="lbl">Missing from our server</span><span class="big">%d</span>'
               '<span>things the client has or expects that players cannot get here</span></a>'
               '<a class="diff" href="#differ"><span class="lbl">We have it, but different</span><span class="big">%d</span>'
               '<span>our value overrides what %s says</span></a>'
               '<a class="new" href="#unused"><span class="lbl">New in %s, unused</span><span class="big">%d</span>'
               '<span>new client spells and items nothing on our server gives out</span></a>'
               '<a class="chg" href="#changed"><span class="lbl">Changed in the client</span><span class="big">%d</span>'
               '<span>items, spells and talents that moved from %s to %s</span></a></nav>'
               % (n_missing, n_differ, e(new), e(new), n_new, n_changed, e(old), e(new)))

    # what players notice
    tops = []
    if loot_sp:
        ex = ', '.join(link_item(x['id'], x['name']) for x in loot_sp[:6])
        tops.append('<b>%d items never drop.</b> Loot rows on spawned creatures and chests point at items that are in '
                    'neither the %s client item data nor our hotfixes, so the server skips the row at startup. Mostly '
                    'vanilla world drops: %s.' % (len(loot_sp), e(new), ex))
    if info_p:
        ex = '; '.join('%s (%s)' % (link_spell(x['id'], x['name']), e(x['changes'])) for x in
                       _pick_spread(info_p, 5))
        tops.append('<b>%d class spells keep a vanilla cast time, cooldown or range</b> that %s changed. Examples: %s.'
                    % (len(info_p), e(new), ex))
    if epic_ch:
        down = [x for x in epic_ch if x['ilvl_delta'] < 0]
        masked = [x for x in epic_ch if x['masked_by_hotfix']]
        tops.append('<b>%d epics changed item level in %s</b> (%d went down, e.g. %s). %s' % (
            len(epic_ch), e(new), len(down), ', '.join(link_item(x['id'], x['name'], x['quality']) for x in epic_ch[:4]),
            '%d of them are overridden by our hotfixes.' % len(masked) if masked else
            'No hotfix of ours overrides them, so players get the new values.'))
    if untaught:
        newc = [x for x in untaught if x['new_in_build']]
        tops.append('<b>%d class spells the client marks as trainer-learned have no trainer here</b>%s.'
                    % (len(untaught), (', including %d new in %s: %s' % (
                        len(newc), e(new), ', '.join(sorted({e(x['name']) for x in newc})))) if newc else ''))
    if cost_p:
        tops.append('<b>%d class spells keep a vanilla mana cost</b> different from %s.' % (len(cost_p), e(new)))
    if q.get('no_giver'):
        tops.append('<b>%d quests the client knows have no giver that is spawned</b> (no NPC, object or starting item).'
                    % len(q['no_giver']))
    if masking:
        tops.append('<b>%d vanilla value overrides sit on spell effects %s just changed</b>, so the %s change never '
                    'reaches the game (e.g. %s).' % (len(masking), e(new), e(new),
                                                     ', '.join(sorted({e(x['name']) for x in masking})[:5])))
    stale = [x for x in d.get('hotfix_other_differ', []) if x['table'] != 'Item']
    if stale:
        tops.append('<b>%d old hotfix rows override newer %s data</b> (%s).' % (
            len(stale), e(new), ', '.join(sorted({e(x['what'].split('(')[-1].rstrip(')')) for x in stale})[:4])))
    out.append('<section class="notice"><h2>What players would notice</h2><ol>%s</ol></section>'
               % ''.join('<li>%s</li>' % t for t in tops))

    # ---- missing
    m = ['<section class="group miss" id="missing"><div class="gh"><h2>Missing from our server</h2>'
         '<p>The client has it or expects it; players on our server cannot get it.</p></div>']
    m.append(P.finding('miss', 'Loot rows that point at items the game does not have', len(loot_sp),
                       'The server builds items only from the client item data (ItemSparse) plus our hotfixes. Rows '
                       'for anything else are skipped at startup, so the drop never happens. "Sources" counts spawned '
                       'creatures and chests that roll it. %d more missing items sit only on loot tables no spawned creature or chest uses.'
                       % (len(loot) - len(loot_sp)),
                       P.table([('Item', lambda r: link_item(r['id'], r['name']), ''), ('Id', lambda r: r['id'], 'n'),
                                ('Sources', lambda r: r['spawned_sources'], 'n'),
                                ('Loot rows', lambda r: r['loot_rows'], 'n'),
                                ('Example sources', lambda r: e(r['example']), 'wide')], loot_sp, 'miss'), True))
    m.append(P.finding('miss', 'Vendor items that do not exist', len(d.get('vendor_missing_items', [])),
                       'Spawned vendors list them; the vendor window leaves them out.',
                       P.table([('Item', lambda r: link_item(r['id'], r['name']), ''), ('Id', lambda r: r['id'], 'n'),
                                ('Vendors', lambda r: r['vendors'], 'n'), ('Example', lambda r: e(r['example']), 'wide')],
                               d.get('vendor_missing_items', []), 'miss')))
    m.append(P.finding('miss', 'Quest rewards that do not exist', len(q.get('reward_missing', [])),
                       'The quest offers an item the game cannot build.',
                       P.table([('Quest', lambda r: '%s <span class="id">%d</span>' % (e(r['quest_name']), r['quest']), 'wide'),
                                ('Reward', lambda r: link_item(r['id'], r['name']), '')], q.get('reward_missing', []), 'miss')))
    m.append(P.finding('miss', 'Quests with no giver in the world', len(q.get('no_giver', [])),
                       'The client knows the quest and our quest data has it, but no spawned NPC or object starts it '
                       'and no item does. Placeholder quests (&lt;UNUSED&gt;, NYI, TEST) are left out (%d). %d client '
                       'quest ids (all %d and above) have no quest at all on our side; they look like hidden tracking '
                       'flags.' % (q.get('placeholder', 0), q.get('no_template', 0), q.get('no_template_min_id', 0)),
                       P.table([('Quest', lambda r: e(r['name']), 'wide'), ('Id', lambda r: r['id'], 'n'),
                                ('Giver we have', lambda r: e(r['giver']), '')], q.get('no_giver', []), 'miss')))
    m.append(P.finding('miss', 'Class spells no trainer teaches', len(untaught),
                       'The client lists them as learned from a trainer (class skill line, not passive, level 1-60). No '
                       'spawned trainer, talent, item or quest gives them. Spells that another spell triggers are left out.',
                       P.table([('Spell', lambda r: link_spell(r['id'], r['name']), ''), ('Id', lambda r: r['id'], 'n'),
                                ('Class', lambda r: e(r['cls']), ''), ('Tree', lambda r: e(r['line']), ''),
                                ('Level', lambda r: r['level'], 'n'),
                                ('New in %s' % new, lambda r: '<span class="pill">new</span>' if r['new_in_build'] else '', '')],
                               untaught, 'miss')))
    m.append(P.finding('miss', 'Trainer spells the game does not have', len(d.get('trainer_unknown_spells', [])),
                       'A spawned trainer lists a spell id that is in neither the client nor our hotfixes.',
                       P.table([('Trainer', lambda r: r['trainer'], 'n'), ('Spell id', lambda r: r['spell'], 'n'),
                                ('NPCs', lambda r: r['npcs'], 'n')], d.get('trainer_unknown_spells', []), 'miss')))
    m.append(P.finding('miss', 'Spawned NPCs with no model the client has', len(d.get('creature_display_missing', [])),
                       'Every display id of the creature is missing from the client and our hotfixes, so it shows as '
                       'a placeholder or not at all.',
                       P.table([('NPC', lambda r: e(r['name']), ''), ('Entry', lambda r: r['id'], 'n'),
                                ('Spawns', lambda r: r['spawns'], 'n'), ('Display ids', lambda r: e(r['displays']), 'mono')],
                               d.get('creature_display_missing', []), 'miss')))
    m.append('</section>')
    out += m

    # ---- differ
    g = ['<section class="group diff" id="differ"><div class="gh"><h2>We have it, but different</h2>'
         '<p>Our server data replaces what %s says. Some of these are deliberate vanilla choices; under the rule '
         '"follow Forever\'s live %s design" they are candidates to drop.</p></div>' % (e(new), e(new))]
    g.append(P.finding('diff', 'Cast time, cooldown, duration or range overrides on class spells', len(info_p),
                       'From the vanilla table <code>classic_spell_info</code>. Durations the queued 246 changes already '
                       'clear (%d rows) are not counted. Cooldown means the longer of the spell and category cooldown. '
                       '%d more rows differ on non-class spells (companions, items, creatures); they are in '
                       'classic_info_differ.tsv.' % (d.get('pending_duration_rows', 0),
                                                     len(d.get('classic_info_differ', [])) - len(info_p)),
                       P.table([('Spell', lambda r: link_spell(r['id'], r['name']), ''), ('Id', lambda r: r['id'], 'n'),
                                ('Class', lambda r: e(r['cls']), ''), ('Server vs client', lambda r: e(r['changes']), 'wide')],
                               info_p, 'diff'), True))
    g.append(P.finding('diff', 'Mana cost overrides on class spells', len(cost_p),
                       'From <code>classic_spell_cost</code>, compared with the client power cost.',
                       P.table([('Spell', lambda r: link_spell(r['id'], r['name']), ''), ('Id', lambda r: r['id'], 'n'),
                                ('Class', lambda r: e(r['cls']), ''), ('Server vs client', lambda r: e(r['changes']), 'wide')],
                               cost_p, 'diff')))
    g.append(P.finding('diff', 'Value overrides that hide a %s change' % new, len(masking),
                       'A <code>classic_spell_value</code> row sits on a spell effect whose base points, coefficient '
                       'or aura changed from %s to %s. The server keeps the vanilla number. The full value audit '
                       '(%d rows) is in audit-70532/overrides-vs-70532.md.' % (e(old), e(new),
                                                                               d.get('classic_counts', {}).get('classic_spell_value', 0)),
                       P.table([('Spell', lambda r: link_spell(r['id'], r['name']), ''), ('Id', lambda r: r['id'], 'n'),
                                ('Effect', lambda r: r['effect'], 'n'), ('Class', lambda r: e(r['cls']), ''),
                                ('What %s changed' % new, lambda r: e(r['changes']), 'wide')], masking, 'diff')))
    g.append(P.finding('diff', 'Trainer level differs from the client', len(tr_lvl),
                       'The trainer offers the spell at another level than the client spell level.',
                       P.table([('Spell', lambda r: link_spell(r['spell'], r['name']), ''), ('Id', lambda r: r['spell'], 'n'),
                                ('Class', lambda r: e(r['cls']), ''), ('Trainer level', lambda r: r['trainer_level'], 'n'),
                                ('Client level', lambda r: r['client_level'], 'n')], tr_lvl, 'diff')))
    g.append(P.finding('diff', 'Hotfix rows that differ from the %s record' % new, len(hot_diff),
                       'Our hotfix database replaces the client record on the server and is sent to the client. These '
                       'rows come from older sniffs (build %s) and hold different values than %s.' % (
                           '/'.join(sorted({str(x.get('hotfix_build')) for x in hot_diff})), e(new)),
                       P.table([('Record', lambda r: e(r.get('what') or '%s %s' % (r.get('name'), r['id'])), 'wide'),
                                ('Hotfix build', lambda r: e(r.get('hotfix_build')), 'n'),
                                ('Client vs server', lambda r: e(r['changes']), 'wide')], hot_diff, 'diff')))
    g.append(P.finding('diff', 'Hotfix tables at a glance', len(hot_tbl),
                       '"Same" rows are harmless copies of the client. "Hotfix only" rows add records the client lacks '
                       '(most of the Forever items). "Not sent" rows change the server but are not listed in '
                       'hotfix_data, so the client never hears of them.',
                       P.table([('Client table', lambda r: e(r['table']), ''), ('Rows', lambda r: r['rows'], 'n'),
                                ('Same', lambda r: r['same'], 'n'), ('Differ', lambda r: r['differ'], 'n'),
                                ('Hotfix only', lambda r: r['hotfix_only'], 'n'), ('Not sent', lambda r: r['not_sent'], 'n'),
                                ('Builds', lambda r: e(', '.join('%s: %s' % kv for kv in r['builds'].items())), 'mono')],
                               hot_tbl, 'diff')))
    g.append('</section>')
    out += g

    # ---- new unused
    u = ['<section class="group new" id="unused"><div class="gh"><h2>New in %s, not used by us</h2>'
         '<p>Client content added since %s that no trainer, talent, loot table, vendor or quest on our server hands '
         'out.</p></div>' % (e(new), e(old))]
    u.append(P.finding('new', 'New class spells nobody teaches', len(d.get('new_spells_unused', [])), '',
                       P.table([('Spell', lambda r: link_spell(r['id'], r['name']), ''), ('Id', lambda r: r['id'], 'n'),
                                ('Class', lambda r: e(r['cls']), '')], d.get('new_spells_unused', []), 'new'), True))
    u.append(P.finding('new', 'New items nothing gives out', len(d.get('new_items_unobtainable', [])), '',
                       P.table([('Item', lambda r: link_item(r['id'], r['name'], r['quality']), ''),
                                ('Id', lambda r: r['id'], 'n'), ('Item level', lambda r: r['ilvl'], 'n')],
                               d.get('new_items_unobtainable', []), 'new'), True))
    u.append(P.finding('new', 'Every spell added in %s' % new, len(d.get('spells_added', [])),
                       'Including creature and quest spells. "Used" says what on our server references it.',
                       P.table([('Spell', lambda r: link_spell(r['id'], r['name']), ''), ('Id', lambda r: r['id'], 'n'),
                                ('Class', lambda r: e(r['cls']), ''), ('Used', lambda r: e(r['used']) or '-', '')],
                               d.get('spells_added', []), 'new')))
    u.append('</section>')
    out += u

    # ---- changed in client
    c = ['<section class="group chg" id="changed"><div class="gh"><h2>Changed in the client, %s to %s</h2>'
         '<p>What moved between the two client builds. These reach players unless a row above overrides them '
         '(the "Our override" column).</p></div>' % (e(old), e(new))]
    c.append(P.finding('chg', 'Items with changed level, stats or rules', len(d.get('items_changed', [])),
                       'Stat values are computed the way the server does it (allocation x item-level points). Price-only '
                       'and flag-only changes are left out.',
                       P.table([('Item', lambda r: link_item(r['id'], r['name'], r['quality']), ''),
                                ('Id', lambda r: r['id'], 'n'), ('ilvl', lambda r: r['ilvl'], 'n'),
                                ('Req', lambda r: r['req'], 'n'), ('Change', lambda r: e(r['changes']), 'wide')],
                               d.get('items_changed', []), 'chg'), True))
    c.append(P.finding('chg', 'Class spells that changed', len(sp_ch_p),
                       'Effects, cast/duration/range indexes, procs and gameplay flags. Client-only display flags and '
                       'icons are left out. %d more non-class spells changed (spells_changed.tsv).'
                       % (len(d.get('spells_changed', [])) - len(sp_ch_p)),
                       P.table([('Spell', lambda r: link_spell(r['id'], r['name']), ''), ('Id', lambda r: r['id'], 'n'),
                                ('Class', lambda r: e(r['cls']), ''), ('Change', lambda r: e(r['changes']), 'wide'),
                                ('Our override', lambda r: e(r['overridden']) or '-', '')], sp_ch_p, 'chg')))
    c.append(P.finding('chg', 'Talent definitions that changed', len(d.get('talents_changed', [])), '',
                       P.table([('Talent', lambda r: link_spell(r['spell'], r['name']), ''),
                                ('Definition', lambda r: r['id'], 'n'), ('Change', lambda r: e(r['change']), 'wide')],
                               d.get('talents_changed', []), 'chg')))
    c.append(P.finding('chg', 'Items and spells removed from the client',
                       len(d.get('items_removed', [])) + len(d.get('spells_removed', [])), '',
                       P.table([('Name', lambda r: e(r['name']), ''), ('Id', lambda r: r['id'], 'n'),
                                ('Kind', lambda r: 'item' if 'ilvl' in r else 'spell', '')],
                               d.get('items_removed', []) + d.get('spells_removed', []), 'chg')))
    bd = d.get('build_diff', {})
    c.append(P.finding('chg', 'Client tables compared', len(bd.get('tables', [])),
                       'Record counts per table. "Hidden" are records in encrypted sections the client cannot read '
                       'until Forever sends the key; they can hold unreleased content. Tables only in %s (not '
                       'compared between builds): %s.' % (e(new), e(', '.join(bd.get('not_compared', [])))),
                       P.table([('Table', lambda r: e(r['table']), ''),
                                ('State', lambda r: 'identical' if r['identical'] else 'changed', ''),
                                ('Added', lambda r: r['added'], 'n'), ('Removed', lambda r: r['removed'], 'n'),
                                ('Changed', lambda r: r['changed'], 'n'),
                                ('Hidden %s/%s' % (old, new), lambda r: '%s / %s' % (r['enc_old'], r['enc_new']), 'n')],
                               bd.get('tables', []), 'chg')))
    c.append('</section>')
    out += c
    out.append('<div class="foot"><p>Made by <code>tools/client_diff/client_diff.py</code>. Every list is also a TSV '
               'next to this page\'s report.json. Item and spell names link to Wowhead Classic for items the game '
               'here cannot name.</p></div></div><script>%s</script>' % JS)
    return '\n'.join(out)


def _pick_spread(rows, n):
    """One example per class first, so the summary is not all one class."""
    seen, out = set(), []
    for r in rows:
        k = (r['cls'], r['name'])
        if r['cls'] in {x['cls'] for x in out} or k in seen:
            continue
        seen.add(k)
        out.append(r)
        if len(out) == n:
            break
    return out
