# Bosses and mobs whose casts live in a VMaNGOS C++ script (ScriptName), not in the EventAI / spell lists the converted smart_scripts carry.
# Each entry was read from the script source named in `src` (github.com/vmangos/core, src/scripts/...). Times are milliseconds.
# Row: dict(spell=id, target='self'|'victim'|'random'|'friendly', first=(min,max), repeat=(min,max), hp=(max_pct,min_pct), note='')
M = {
    # src/scripts/eastern_kingdoms/westfall/deadmines/boss_mr_smite.cpp
    646: dict(src='eastern_kingdoms/westfall/deadmines/boss_mr_smite.cpp', rows=[
        dict(spell=6432, target='self', first=(0, 0), repeat=(0, 0), hp=(66, 0),
             note='Smite Stomp at 66% and again at 33%: he stops fighting, walks to his chest and re-equips for about 5 s (axes, then hammer)'),
        dict(spell=3391, target='victim', first=(4000, 4000), repeat=(1500, 4000), hp=(66, 33), note='Thrash on the tank in phase 2 (two axes)'),
        dict(spell=6435, target='victim', first=(9000, 9000), repeat=(11000, 11000), hp=(33, 0), note='Smite Slam on the tank every 11 s in phase 3 (hammer)'),
    ]),
    # src/scripts/eastern_kingdoms/tirisfal_glades/scarlet_monastery/*.cpp
    3974: dict(src='eastern_kingdoms/tirisfal_glades/scarlet_monastery/boss_houndmaster_loksey.cpp', rows=[
        dict(spell=6742, target='self', first=(20000, 20000), repeat=(20000, 20000), hp=(100, 0), note='Bloodlust every 20 s'),
    ]),
    6487: dict(src='eastern_kingdoms/tirisfal_glades/scarlet_monastery/boss_arcanist_doan.cpp', rows=[
        dict(spell=13323, target='random', first=(20000, 20000), repeat=(20000, 20000), hp=(100, 0), note='Polymorph on a player every 20 s'),
        dict(spell=8988, target='self', first=(15000, 15000), repeat=(15000, 20000), hp=(100, 0), note='Silence around him every 15-20 s'),
        dict(spell=9433, target='self', first=(3000, 3000), repeat=(8000, 8000), hp=(100, 0), note='Arcane Explosion every 8 s'),
        dict(spell=9435, target='self', first=(0, 0), repeat=(0, 0), hp=(50, 0), note='Fire nova at 50% as he shields'),
        dict(spell=9438, target='self', first=(0, 0), repeat=(0, 0), hp=(50, 0), note='Arcane Bubble at 50%: immune, hold fire on him'),
    ]),
    4542: dict(src='eastern_kingdoms/tirisfal_glades/scarlet_monastery/boss_high_inquisitor_fairbanks.cpp', rows=[
        dict(spell=8282, target='victim', first=(10000, 10000), repeat=(25000, 25000), hp=(100, 0), note='Curse of Blood'),
        dict(spell=15090, target='friendly', first=(20000, 20000), repeat=(30000, 30000), hp=(100, 0), note='Dispels magic'),
        dict(spell=12096, target='random', first=(40000, 40000), repeat=(40000, 40000), hp=(100, 0), note='Fear'),
        dict(spell=8399, target='random', first=(30000, 30000), repeat=(30000, 30000), hp=(100, 0), note='Sleep'),
        dict(spell=12039, target='self', first=(0, 0), repeat=(30000, 30000), hp=(25, 0), note='Heal below 25%'),
        dict(spell=11647, target='self', first=(0, 0), repeat=(0, 0), hp=(25, 0), note='Power Word: Shield at 25%'),
    ]),
    3975: dict(src='eastern_kingdoms/tirisfal_glades/scarlet_monastery/boss_herod.cpp', rows=[
        dict(spell=8260, target='victim', first=(1500, 1500), repeat=(4500, 4500), hp=(100, 0), note='Rushing Charge'),
        dict(spell=15496, target='victim', first=(12000, 12000), repeat=(12000, 12000), hp=(100, 0), note='Cleave'),
        dict(spell=8989, target='self', first=(10000, 20000), repeat=(20000, 30000), hp=(100, 0), note='Whirlwind for about 11 s: melee step out'),
        dict(spell=8269, target='self', first=(0, 0), repeat=(0, 0), hp=(50, 0), note='Frenzy at 50%'),
    ]),
    # src/scripts/eastern_kingdoms/dun_morogh/gnomeregan/boss_thermaplugg.cpp
    7800: dict(src='eastern_kingdoms/dun_morogh/gnomeregan/boss_thermaplugg.cpp', rows=[
        dict(spell=10101, target='victim', first=(17000, 20000), repeat=(17000, 20000), hp=(100, 50), note='Knock Away on the tank'),
        dict(spell=10101, target='victim', first=(12000, 12000), repeat=(17000, 20000), hp=(50, 0), note='Knock Away on the tank, sooner in phase 2'),
        dict(spell=11511, target='self', first=(10000, 15000), repeat=(12000, 17000), hp=(100, 50), note='Activates a bomb face: walking bombs run at the party'),
        dict(spell=11511, target='self', first=(10000, 15000), repeat=(6000, 12000), hp=(50, 0), note='Bombs twice as often in phase 2'),
    ]),
    # src/scripts/eastern_kingdoms/uldaman/*.cpp
    2748: dict(src='eastern_kingdoms/uldaman/boss_archaedas.cpp', rows=[
        dict(spell=6524, target='victim', first=(45000, 45000), repeat=(45000, 45000), hp=(100, 0), note='Ground Tremor; he wakes his guardians at 66% and his vault warders at 33%'),
    ]),
    7228: dict(src='eastern_kingdoms/uldaman/boss_ironaya.cpp', rows=[
        dict(spell=8374, target='self', first=(13000, 13000), repeat=(13000, 13000), hp=(100, 0), note='Arcing Smash every 13 s: melee step out'),
        dict(spell=10101, target='victim', first=(0, 0), repeat=(0, 0), hp=(50, 0), note='Knock Away once at 50%: the tank loses all threat, the next in line takes over'),
        dict(spell=11876, target='self', first=(0, 0), repeat=(0, 0), hp=(25, 0), note='War Stomp once at 25%'),
    ]),
    # src/scripts/eastern_kingdoms/burning_steppes/blackrock_depths/*.cpp
    9018: dict(src='eastern_kingdoms/burning_steppes/blackrock_depths/boss_high_interrogator_gerstahn.cpp', rows=[
        dict(spell=14032, target='victim', first=(4000, 4000), repeat=(7000, 7000), hp=(100, 0), note='Shadow Word: Pain'),
        dict(spell=14033, target='random', first=(14000, 14000), repeat=(10000, 10000), hp=(100, 0), note='Mana Burn: casters should be out of reach'),
        dict(spell=13704, target='self', first=(32000, 32000), repeat=(30000, 30000), hp=(100, 0), note='Psychic Scream: fear'),
        dict(spell=12040, target='self', first=(8000, 8000), repeat=(25000, 25000), hp=(100, 0), note='Shadow Shield: hold shadow back, purge it'),
    ]),
    9033: dict(src='eastern_kingdoms/burning_steppes/blackrock_depths/boss_general_angerforge.cpp', rows=[
        dict(spell=15572, target='victim', first=(5000, 10000), repeat=(5000, 15000), hp=(100, 0), note='Sunder Armor on the tank; at 30% he calls adds every 3 minutes'),
    ]),
    9938: dict(src='eastern_kingdoms/burning_steppes/blackrock_depths/boss_magmus.cpp', rows=[
        dict(spell=13900, target='self', first=(5000, 5000), repeat=(6000, 6000), hp=(100, 0), note='Fiery Burst'),
        dict(spell=24375, target='self', first=(0, 0), repeat=(8000, 8000), hp=(51, 0), note='War Stomp every 8 s below 51%'),
    ]),
    9019: dict(src='eastern_kingdoms/burning_steppes/blackrock_depths/boss_emperor_dagran_thaurissan.cpp', rows=[
        dict(spell=17492, target='random', first=(5000, 7500), repeat=(10000, 15000), hp=(100, 0), note='Hand of Thaurissan'),
        dict(spell=15636, target='self', first=(18000, 18000), repeat=(18000, 18000), hp=(100, 0), note='Avatar of Flame'),
        dict(spell=15642, target='self', first=(9000, 9000), repeat=(20000, 25000), hp=(100, 0), note='Ironfoe'),
    ]),
    # Razorfen Downs: Amnennar's EventAI (735804 / 735805, sql/custom/world/2026_09_28_15_world_vanilla_smartai.sql). The summons are
    # linked actions of health events, whose window the reader leaves at 100-0: the windows by hand.
    7358: dict(src='creature_ai_scripts 735804 / 735805 (EventAI, the linked summon actions)', rows=[
        dict(spell=12642, target='self', first=(0, 0), repeat=(0, 0), hp=(66, 33), note='Frost Spectres at 66%: kill the adds, then back on him'),
        dict(spell=12642, target='self', first=(0, 0), repeat=(0, 0), hp=(33, 1), note='Frost Spectres again at 33%'),
    ]),
}
