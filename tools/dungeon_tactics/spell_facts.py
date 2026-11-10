# Hand-written facts for the creature spells the generator cannot read out of the data: what the spell is, its school, its cast time and
# how the bots should treat it. The creature spell lists, timers and targets come from VMaNGOS data (vmdata.py); this file is only the
# meaning of a spell id.
#
# conf: 'k' = known vanilla spell, recalled with confidence; 'u' = recalled, check against the client data before relying on it.
# cast: 0 = instant, ms = cast time, None = not known. A spell with a cast time can be interrupted; an instant one cannot.
# Names that classic_spell_value already holds (every damage / heal / absorb spell) are not repeated here - that table wins.
#
# tags (what it does):
#   nuke      single-target damage cast        aoe     area damage around the caster or a target ground area
#   dot       damage over time                  heal    heals (self or ally)           hot   heal over time
#   fear      fear / horror                     stun    stun                           root  root
#   snare     movement slow                     sleep   sleep / incapacitate           silence silence / interrupt lock
#   charm     mind control                      summon  adds or pets                   buff  self or ally buff
#   shield    absorb / damage shield / immunity debuff  debuff on the victim (armor, curse, disease...)
#   curse     a curse                           enrage  enrage / berserk                charge charge / pull
#   drain     mana burn / drain                 stealth stealth                          cleave  hits several targets in front
#   knock     knockback                         disarm  disarm                          interrupt  spell interrupt
#   immune    makes the caster immune
# dispel: 'magic' | 'curse' | 'poison' | 'disease' | None
# hold: SpellSchoolMask bits (1 physical, 2 holy, 4 fire, 8 nature, 16 frost, 32 shadow, 64 arcane) the party should not use on the caster while
#       this aura is up (an absorb ward of one school, Divine Protection's physical immunity, Divine Shield's everything).
F = {}
def f(sid, name, school, cast, tags, dispel=None, conf='k', hold=0):
    F[sid] = dict(name=name, school=school, cast=cast, tags=set(tags.split()), dispel=dispel, conf=conf, hold=hold)

# ---- Ragefire Chasm
f(11976, 'Strike', 'phys', 0, 'dmg')
f(11986, 'Healing Wave', 'nature', 3000, 'heal')
f(18070, 'Earthborer Acid', 'nature', 0, 'debuff', None, 'u')
f(18968, 'Fire Shield', 'fire', 0, 'shield buff', 'magic')
f(18266, 'Curse of Agony', 'shadow', 0, 'dot curse', 'curse')
f(18267, 'Curse of Weakness', 'shadow', 0, 'debuff curse', 'curse')
f(8242, 'Shield Slam', 'phys', 0, 'dmg')
f(12746, 'Summon Voidwalker', 'shadow', 5000, 'summon')
f(14873, 'Sinister Strike', 'phys', 0, 'dmg')
f(18072, 'Uppercut', 'phys', 0, 'dmg knock')
# ---- Deadmines
f(6016, 'Pierce Armor', 'phys', 0, 'debuff')
f(12097, 'Pierce Armor', 'phys', 0, 'debuff')
f(5115, 'Battle Command', 'phys', 0, 'buff', 'magic')
f(6685, 'Piercing Howl', 'phys', 0, 'aoe snare')
f(6466, 'Axe Toss', 'phys', 0, 'stun', None, 'u')
f(5532, 'Cleave', 'phys', 0, 'dmg cleave')
f(3391, 'Thrash', 'phys', 0, 'buff')
f(3603, 'Distracting Pain', 'phys', 0, 'debuff', None, 'u')
f(7399, 'Terrify', 'shadow', 0, 'fear', 'magic', 'u')
f(5141, 'Eject Sneed', 'phys', 0, 'summon')
f(5200, "VanCleef's Allies", 'phys', 0, 'summon', None, 'u')
f(5174, "Cookie's Cooking", 'phys', 0, 'buff heal', None, 'u')
f(5208, 'Poisoned Harpoon', 'nature', 0, 'dot charge', 'poison')
f(6432, 'Smite Stomp', 'phys', 0, 'aoe stun', None, 'u')
f(6435, 'Smite Slam', 'phys', 0, 'dmg stun', None, 'u')
f(4979, 'Quick Flame Ward', 'fire', 0, 'shield buff', 'magic', 'u', hold=4)
f(12544, 'Frost Armor', 'frost', 0, 'buff', 'magic')
f(113, 'Chains of Ice', 'frost', 1500, 'root', 'magic')
f(122, 'Frost Nova', 'frost', 0, 'aoe root', 'magic')
f(865, 'Frost Nova', 'frost', 0, 'aoe root', 'magic')
# ---- Wailing Caverns
f(700, 'Sleep', 'nature', 1500, 'sleep', 'magic')
f(8040, "Druid's Slumber", 'nature', 1500, 'sleep', 'magic')
f(8041, 'Serpent Form', 'nature', 0, 'buff', None, 'u')
f(7965, 'Serpent Form', 'nature', 0, 'buff', None, 'u')
f(8142, 'Grasping Vines', 'nature', 0, 'root', None, 'u')
f(8147, 'Thunderclap', 'phys', 0, 'aoe snare')
f(8148, 'Thunderclap', 'phys', 0, 'aoe snare', None, 'u')
f(7948, 'Wild Regeneration', 'nature', 0, 'hot buff', None, 'k')
f(3604, 'Tendon Rip', 'phys', 0, 'snare')
f(3616, 'Poison Proc', 'nature', 0, 'buff')
f(13236, 'Lightning Shield', 'nature', 0, 'shield buff', 'magic', 'u')
f(5187, 'Healing Touch', 'nature', 3000, 'heal')
f(6778, 'Healing Touch', 'nature', 3000, 'heal')
# ---- Shadowfang Keep
f(7121, 'Anti-Magic Shield', 'arcane', 0, 'shield buff', 'magic', hold=126)
f(7106, 'Dark Restore', 'shadow', 2500, 'heal')
f(7054, 'Forsaken Skills', 'shadow', 0, 'buff', None, 'u')
f(7057, 'Haunting Spirits', 'shadow', 0, 'dot', 'magic', 'u')
f(7074, 'Screams of the Past', 'shadow', 0, 'aoe silence', 'magic', 'u')
f(7068, 'Veil of Shadow', 'shadow', 0, 'debuff', 'magic')
f(7127, 'Wavering Will', 'shadow', 0, 'fear', 'magic', 'u')
f(7132, 'Summon Lupine Delusions', 'shadow', 0, 'summon', None, 'u')
f(7122, 'Blood Tap', 'shadow', 0, 'debuff', None, 'u')
f(7072, 'Enrage', 'phys', 0, 'enrage', None, 'u')
f(7487, 'Summon Dread Wolf', 'phys', 0, 'summon', None, 'u')
f(7488, 'Summon Dread Wolf', 'phys', 0, 'summon', None, 'u')
f(7489, 'Summon Dread Wolf', 'phys', 0, 'summon', None, 'u')
f(7481, 'Howling Rage', 'phys', 0, 'buff', None, 'k')
f(7483, 'Howling Rage', 'phys', 0, 'buff', None, 'k')
f(7484, 'Howling Rage', 'phys', 0, 'buff', None, 'k')
f(7621, "Arugal's Curse", 'shadow', 0, 'curse charm', 'curse', 'u')
f(7586, 'Shadow Port', 'shadow', 0, 'knock', None, 'u')
f(7587, 'Shadow Port', 'shadow', 0, 'knock', None, 'u')
f(1026, 'Holy Light', 'holy', 2500, 'heal')
f(498, 'Divine Protection', 'holy', 0, 'immune buff', None, 'k', hold=1)
f(5588, 'Hammer of Justice', 'holy', 0, 'stun', 'magic')
# ---- Blackfathom Deeps
f(6205, 'Curse of Weakness', 'shadow', 0, 'debuff curse', 'curse')
f(8129, 'Mana Burn', 'shadow', 3000, 'drain', 'magic')
f(6143, 'Frost Ward', 'frost', 0, 'shield buff', 'magic', hold=16)
f(8379, 'Disarm', 'phys', 0, 'disarm')
f(8374, 'Arcing Smash', 'phys', 0, 'dmg cleave', None, 'u')
f(8365, 'Enlarge', 'phys', 0, 'buff', None, 'u')
f(18972, 'Slow', 'arcane', 0, 'snare', 'magic', 'u')
f(7645, 'Dominate Mind', 'shadow', 2000, 'charm', 'magic')
f(8391, 'Ravage', 'phys', 0, 'dmg')
f(246, 'Slow', 'arcane', 0, 'snare', 'magic')
f(8399, 'Sleep', 'nature', 1500, 'sleep', 'magic', 'u')
f(12024, 'Net', 'phys', 0, 'root', None, 'k')
f(12550, 'Lightning Shield', 'nature', 0, 'shield buff', 'magic')
f(5568, 'Trample', 'phys', 0, 'aoe')
f(8372, 'Ice Armor', 'frost', 0, 'buff', 'magic', 'u')

# ---- added after the first generator run
f(6660, 'Shoot', 'phys', 0, 'dmg')
f(6304, "Rhahk'Zor Slam", 'phys', 0, 'dmg')
f(5213, 'Molten Metal', 'fire', 0, 'dmg dot', None, 'u')
f(5159, 'Melt Ore', 'fire', 0, 'dmg dot', None, 'u')
f(6306, 'Acid Splash', 'nature', 0, 'debuff', None, 'u')
f(1020, 'Divine Shield', 'holy', 0, 'immune buff', None, 'k', hold=127)
f(7139, 'Fel Stomp', 'phys', 0, 'dmg', None, 'u')
f(7405, 'Sunder Armor', 'phys', 0, 'debuff')

# batch 2 - Scarlet Monastery / Gnomeregan C++ bosses (spell ids read from vmangos src/scripts)
f(13323, 'Polymorph', 'arcane', 1500, 'sleep', 'magic', 'u')
f(8988, 'Silence', 'shadow', 0, 'silence aoe', 'magic', 'u')
f(9433, 'Arcane Explosion', 'arcane', 2000, 'aoe nuke', None, 'u')
f(9435, 'Fire Nova', 'fire', 1500, 'aoe nuke', None, 'u')
f(9438, 'Arcane Bubble', 'arcane', 0, 'immune shield', None, 'u', hold=127)
f(8282, 'Curse of Blood', 'shadow', 0, 'curse debuff', 'curse', 'u')
f(15090, 'Dispel Magic', 'holy', 0, 'buff', None, 'u')
f(12096, 'Fear', 'shadow', 1500, 'fear', 'magic', 'u')
f(12039, 'Heal', 'holy', 2500, 'heal', None, 'u')
# Mograine / Whitemane (instance-driven, EventAI 3976 / 3977) and Amnennar (EventAI 7358): schools, cast indexes and effects read from the
# VMaNGOS spell_template rows (tools/classic_spell_values); the 2 s of Scarlet Resurrection is cast index 5 by recall.
f(9257, 'Lay on Hands', 'holy', 0, 'heal')                           # Mograine, on himself, once Whitemane has raised him
f(9256, 'Deep Sleep', 'shadow', 0, 'sleep')                          # Whitemane at 50%: everyone within 100 y stunned 10 s
f(9232, 'Scarlet Resurrection', 'phys', 2000, 'heal', None, 'u')     # Whitemane on Mograine after the sleep: kick it when awake
f(12642, 'Summon Frost Spectres', 'phys', 0, 'summon')              # Amnennar at 66% and 33%: three Frost Spectres
f(13009, "Amnennar's Wrath", 'phys', 0, 'knock')                   # Amnennar every 12 s on the tank: a knockback
f(11647, 'Power Word: Shield', 'holy', 0, 'shield', 'magic', 'u')
f(17164, 'Summon Scarlet Hound', 'physical', 0, 'summon', None, 'u')
f(6742, 'Bloodlust', 'physical', 0, 'buff', None, 'u')
f(8260, 'Rushing Charge', 'physical', 0, 'charge', None, 'u')
f(15496, 'Cleave', 'physical', 0, 'cleave', None, 'u')
f(8989, 'Whirlwind', 'physical', 0, 'aoe', None, 'u')
f(8269, 'Frenzy', 'physical', 0, 'enrage', None, 'u')
f(10101, 'Knock Away', 'physical', 0, 'knock', None, 'u')
f(11130, 'Knock Away', 'physical', 0, 'knock aoe', None, 'u')
f(11511, 'Activate Bomb', 'fire', 0, 'summon', None, 'u')
f(11795, 'Activate Bomb', 'fire', 0, 'summon', None, 'u')
f(676, 'Disarm', 'physical', 0, 'disarm', None, 'k')
f(1543, 'Flare', 'physical', 0, 'buff', None, 'k')

# batch 3 - Uldaman / Blackrock Depths C++ bosses
f(6524, 'Ground Tremor', 'physical', 0, 'aoe stun', None, 'u')
f(8374, 'Arcing Smash', 'physical', 0, 'cleave', None, 'u')
f(11876, 'War Stomp', 'physical', 0, 'aoe stun', None, 'k')
f(24375, 'War Stomp', 'physical', 0, 'aoe stun', None, 'k')
f(14032, 'Shadow Word: Pain', 'shadow', 0, 'dot', 'magic', 'k')
f(14033, 'Mana Burn', 'shadow', 3000, 'drain', None, 'k')
f(13704, 'Psychic Scream', 'shadow', 0, 'fear aoe', 'magic', 'k')
f(12040, 'Shadow Shield', 'shadow', 0, 'shield', 'magic', 'u', hold=32)
f(15572, 'Sunder Armor', 'physical', 0, 'debuff', None, 'k')
f(13900, 'Fiery Burst', 'fire', 0, 'aoe nuke', None, 'u')
f(17492, 'Hand of Thaurissan', 'fire', 0, 'nuke aoe', None, 'u')
f(15636, 'Avatar of Flame', 'fire', 0, 'aoe nuke', None, 'u')
f(15642, 'Ironfoe', 'physical', 0, 'buff', None, 'u')
