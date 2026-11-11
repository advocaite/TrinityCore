-- WoW Forever: trainer prices as live trainers show them (build 70862). Live sells at the neutral price up to Friendly and
-- 10% off at Honored, so the price here is the Friendly one (or Honored / 0.9). Many class spells cost less on live than the
-- vanilla prices we had; most profession recipes cost a little more (ours were 10% under vanilla).
-- Data from Efficient Games (https://efficient.games), licensed under CC BY 4.0
-- (https://creativecommons.org/licenses/by/4.0/). Source: github.com/JIVESCORP/eg-link-output-wowf, build 1.60.1.70862,
-- collected 2026-11-07..10 from 4 players on the live realm. Changed: picked and converted to our tables; only rows we lacked
-- or had different from live. Where our own live captures (2026_11_10_60/61) cover a record, ours is kept.
-- Safe to run again.

-- trainer 1 (Anastasia Hartwell, Archmage Shymm, Cain Firesong, Enyo): 140 spells
UPDATE `trainer_spell` SET `MoneyCost`=2100 WHERE `TrainerId`=1 AND `SpellId`=10; -- Blizzard Rank 1, was 2000
UPDATE `trainer_spell` SET `MoneyCost`=4800 WHERE `TrainerId`=1 AND `SpellId`=120; -- Cone of Cold Rank 1, was 5000
UPDATE `trainer_spell` SET `MoneyCost`=300 WHERE `TrainerId`=1 AND `SpellId`=122; -- Frost Nova Rank 1, was 400
UPDATE `trainer_spell` SET `MoneyCost`=2100 WHERE `TrainerId`=1 AND `SpellId`=543; -- Fire Ward Rank 1, was 2000
UPDATE `trainer_spell` SET `MoneyCost`=5500 WHERE `TrainerId`=1 AND `SpellId`=759; -- Conjure Mana Agate , was 7000
UPDATE `trainer_spell` SET `MoneyCost`=4800 WHERE `TrainerId`=1 AND `SpellId`=865; -- Frost Nova Rank 2, was 5000
UPDATE `trainer_spell` SET `MoneyCost`=3300 WHERE `TrainerId`=1 AND `SpellId`=990; -- Conjure Food Rank 3, was 3000
UPDATE `trainer_spell` SET `MoneyCost`=5500 WHERE `TrainerId`=1 AND `SpellId`=1461; -- Arcane Intellect Rank 3, was 7000
UPDATE `trainer_spell` SET `MoneyCost`=2100 WHERE `TrainerId`=1 AND `SpellId`=1463; -- Mana Shield Rank 1, was 2000
UPDATE `trainer_spell` SET `MoneyCost`=2100 WHERE `TrainerId`=1 AND `SpellId`=1953; -- Blink , was 2000
UPDATE `trainer_spell` SET `MoneyCost`=1700 WHERE `TrainerId`=1 AND `SpellId`=2120; -- Flamestrike Rank 1, was 1500
UPDATE `trainer_spell` SET `MoneyCost`=4300 WHERE `TrainerId`=1 AND `SpellId`=2121; -- Flamestrike Rank 2, was 4000
UPDATE `trainer_spell` SET `MoneyCost`=3300 WHERE `TrainerId`=1 AND `SpellId`=2138; -- Fire Blast Rank 3, was 3000
UPDATE `trainer_spell` SET `MoneyCost`=4300 WHERE `TrainerId`=1 AND `SpellId`=2139; -- Counterspell , was 4000
UPDATE `trainer_spell` SET `MoneyCost`=3300 WHERE `TrainerId`=1 AND `SpellId`=2948; -- Scorch Rank 1, was 3000
UPDATE `trainer_spell` SET `MoneyCost`=12000 WHERE `TrainerId`=1 AND `SpellId`=3552; -- Conjure Mana Jade , was 14000
UPDATE `trainer_spell` SET `MoneyCost`=1700 WHERE `TrainerId`=1 AND `SpellId`=5144; -- Arcane Missiles Rank 2, was 1500
UPDATE `trainer_spell` SET `MoneyCost`=4300 WHERE `TrainerId`=1 AND `SpellId`=5145; -- Arcane Missiles Rank 3, was 4000
UPDATE `trainer_spell` SET `MoneyCost`=300 WHERE `TrainerId`=1 AND `SpellId`=5505; -- Conjure Water Rank 2, was 400
UPDATE `trainer_spell` SET `MoneyCost`=2100 WHERE `TrainerId`=1 AND `SpellId`=5506; -- Conjure Water Rank 3, was 2000
UPDATE `trainer_spell` SET `MoneyCost`=9000 WHERE `TrainerId`=1 AND `SpellId`=6117; -- Mage Armor Rank 1, was 12000
UPDATE `trainer_spell` SET `MoneyCost`=6200 WHERE `TrainerId`=1 AND `SpellId`=6127; -- Conjure Water Rank 4, was 8000
UPDATE `trainer_spell` SET `MoneyCost`=8000 WHERE `TrainerId`=1 AND `SpellId`=6129; -- Conjure Food Rank 4, was 10000
UPDATE `trainer_spell` SET `MoneyCost`=13000 WHERE `TrainerId`=1 AND `SpellId`=6131; -- Frost Nova Rank 3, was 15000
UPDATE `trainer_spell` SET `MoneyCost`=5500 WHERE `TrainerId`=1 AND `SpellId`=6141; -- Blizzard Rank 2, was 7000
UPDATE `trainer_spell` SET `MoneyCost`=3300 WHERE `TrainerId`=1 AND `SpellId`=6143; -- Frost Ward Rank 1, was 3000
UPDATE `trainer_spell` SET `MoneyCost`=300 WHERE `TrainerId`=1 AND `SpellId`=7300; -- Frost Armor Rank 2, was 400
UPDATE `trainer_spell` SET `MoneyCost`=2100 WHERE `TrainerId`=1 AND `SpellId`=7301; -- Frost Armor Rank 3, was 2000
UPDATE `trainer_spell` SET `MoneyCost`=6200 WHERE `TrainerId`=1 AND `SpellId`=7302; -- Ice Armor Rank 1, was 8000
UPDATE `trainer_spell` SET `MoneyCost`=13000 WHERE `TrainerId`=1 AND `SpellId`=7320; -- Ice Armor Rank 2, was 15000
UPDATE `trainer_spell` SET `MoneyCost`=2100 WHERE `TrainerId`=1 AND `SpellId`=7322; -- Frostbolt Rank 4, was 2000
UPDATE `trainer_spell` SET `MoneyCost`=4300 WHERE `TrainerId`=1 AND `SpellId`=8400; -- Fireball Rank 5, was 4000
UPDATE `trainer_spell` SET `MoneyCost`=6200 WHERE `TrainerId`=1 AND `SpellId`=8401; -- Fireball Rank 6, was 8000
UPDATE `trainer_spell` SET `MoneyCost`=11000 WHERE `TrainerId`=1 AND `SpellId`=8402; -- Fireball Rank 7, was 13000
UPDATE `trainer_spell` SET `MoneyCost`=4800 WHERE `TrainerId`=1 AND `SpellId`=8406; -- Frostbolt Rank 5, was 5000
UPDATE `trainer_spell` SET `MoneyCost`=8000 WHERE `TrainerId`=1 AND `SpellId`=8407; -- Frostbolt Rank 6, was 10000
UPDATE `trainer_spell` SET `MoneyCost`=12000 WHERE `TrainerId`=1 AND `SpellId`=8408; -- Frostbolt Rank 7, was 14000
UPDATE `trainer_spell` SET `MoneyCost`=6200 WHERE `TrainerId`=1 AND `SpellId`=8412; -- Fire Blast Rank 4, was 8000
UPDATE `trainer_spell` SET `MoneyCost`=12000 WHERE `TrainerId`=1 AND `SpellId`=8413; -- Fire Blast Rank 5, was 14000
UPDATE `trainer_spell` SET `MoneyCost`=8000 WHERE `TrainerId`=1 AND `SpellId`=8416; -- Arcane Missiles Rank 4, was 10000
UPDATE `trainer_spell` SET `MoneyCost`=13000 WHERE `TrainerId`=1 AND `SpellId`=8417; -- Arcane Missiles Rank 5, was 15000
UPDATE `trainer_spell` SET `MoneyCost`=8000 WHERE `TrainerId`=1 AND `SpellId`=8422; -- Flamestrike Rank 3, was 10000
UPDATE `trainer_spell` SET `MoneyCost`=13000 WHERE `TrainerId`=1 AND `SpellId`=8423; -- Flamestrike Rank 4, was 15000
UPDATE `trainer_spell` SET `MoneyCost`=11000 WHERE `TrainerId`=1 AND `SpellId`=8427; -- Blizzard Rank 3, was 13000
UPDATE `trainer_spell` SET `MoneyCost`=3300 WHERE `TrainerId`=1 AND `SpellId`=8437; -- Arcane Explosion Rank 2, was 3000
UPDATE `trainer_spell` SET `MoneyCost`=6200 WHERE `TrainerId`=1 AND `SpellId`=8438; -- Arcane Explosion Rank 3, was 8000
UPDATE `trainer_spell` SET `MoneyCost`=12000 WHERE `TrainerId`=1 AND `SpellId`=8439; -- Arcane Explosion Rank 4, was 14000
UPDATE `trainer_spell` SET `MoneyCost`=5500 WHERE `TrainerId`=1 AND `SpellId`=8444; -- Scorch Rank 2, was 7000
UPDATE `trainer_spell` SET `MoneyCost`=9000 WHERE `TrainerId`=1 AND `SpellId`=8445; -- Scorch Rank 3, was 12000
UPDATE `trainer_spell` SET `MoneyCost`=13000 WHERE `TrainerId`=1 AND `SpellId`=8446; -- Scorch Rank 4, was 15000
UPDATE `trainer_spell` SET `MoneyCost`=4300 WHERE `TrainerId`=1 AND `SpellId`=8450; -- Dampen Magic Rank 2, was 4000
UPDATE `trainer_spell` SET `MoneyCost`=11000 WHERE `TrainerId`=1 AND `SpellId`=8451; -- Dampen Magic Rank 3, was 13000
UPDATE `trainer_spell` SET `MoneyCost`=6200 WHERE `TrainerId`=1 AND `SpellId`=8455; -- Amplify Magic Rank 2, was 8000
UPDATE `trainer_spell` SET `MoneyCost`=6200 WHERE `TrainerId`=1 AND `SpellId`=8457; -- Fire Ward Rank 2, was 8000
UPDATE `trainer_spell` SET `MoneyCost`=13000 WHERE `TrainerId`=1 AND `SpellId`=8458; -- Fire Ward Rank 3, was 15000
UPDATE `trainer_spell` SET `MoneyCost`=8000 WHERE `TrainerId`=1 AND `SpellId`=8461; -- Frost Ward Rank 2, was 10000
UPDATE `trainer_spell` SET `MoneyCost`=14000 WHERE `TrainerId`=1 AND `SpellId`=8462; -- Frost Ward Rank 3, was 18000
UPDATE `trainer_spell` SET `MoneyCost`=9000 WHERE `TrainerId`=1 AND `SpellId`=8492; -- Cone of Cold Rank 2, was 12000
UPDATE `trainer_spell` SET `MoneyCost`=5500 WHERE `TrainerId`=1 AND `SpellId`=8494; -- Mana Shield Rank 2, was 7000
UPDATE `trainer_spell` SET `MoneyCost`=11000 WHERE `TrainerId`=1 AND `SpellId`=8495; -- Mana Shield Rank 3, was 13000
UPDATE `trainer_spell` SET `MoneyCost`=21000 WHERE `TrainerId`=1 AND `SpellId`=10053; -- Conjure Mana Citrine , was 28000
UPDATE `trainer_spell` SET `MoneyCost`=29000 WHERE `TrainerId`=1 AND `SpellId`=10054; -- Conjure Mana Ruby , was 40000
UPDATE `trainer_spell` SET `MoneyCost`=13000 WHERE `TrainerId`=1 AND `SpellId`=10138; -- Conjure Water Rank 5, was 15000
UPDATE `trainer_spell` SET `MoneyCost`=23000 WHERE `TrainerId`=1 AND `SpellId`=10139; -- Conjure Water Rank 6, was 32000
UPDATE `trainer_spell` SET `MoneyCost`=14000 WHERE `TrainerId`=1 AND `SpellId`=10144; -- Conjure Food Rank 5, was 18000
UPDATE `trainer_spell` SET `MoneyCost`=24000 WHERE `TrainerId`=1 AND `SpellId`=10145; -- Conjure Food Rank 6, was 35000
UPDATE `trainer_spell` SET `MoneyCost`=14000 WHERE `TrainerId`=1 AND `SpellId`=10148; -- Fireball Rank 8, was 18000
UPDATE `trainer_spell` SET `MoneyCost`=21000 WHERE `TrainerId`=1 AND `SpellId`=10149; -- Fireball Rank 9, was 28000
UPDATE `trainer_spell` SET `MoneyCost`=26000 WHERE `TrainerId`=1 AND `SpellId`=10150; -- Fireball Rank 10, was 36000
UPDATE `trainer_spell` SET `MoneyCost`=30000 WHERE `TrainerId`=1 AND `SpellId`=10151; -- Fireball Rank 11, was 42000
UPDATE `trainer_spell` SET `MoneyCost`=14000 WHERE `TrainerId`=1 AND `SpellId`=10156; -- Arcane Intellect Rank 4, was 18000
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=1 AND `SpellId`=10157; -- Arcane Intellect Rank 5, was 38000
UPDATE `trainer_spell` SET `MoneyCost`=14000 WHERE `TrainerId`=1 AND `SpellId`=10159; -- Cone of Cold Rank 3, was 18000
UPDATE `trainer_spell` SET `MoneyCost`=23000 WHERE `TrainerId`=1 AND `SpellId`=10160; -- Cone of Cold Rank 4, was 32000
UPDATE `trainer_spell` SET `MoneyCost`=29000 WHERE `TrainerId`=1 AND `SpellId`=10161; -- Cone of Cold Rank 5, was 40000
UPDATE `trainer_spell` SET `MoneyCost`=14000 WHERE `TrainerId`=1 AND `SpellId`=10169; -- Amplify Magic Rank 3, was 18000
UPDATE `trainer_spell` SET `MoneyCost`=26000 WHERE `TrainerId`=1 AND `SpellId`=10170; -- Amplify Magic Rank 4, was 36000
UPDATE `trainer_spell` SET `MoneyCost`=21000 WHERE `TrainerId`=1 AND `SpellId`=10173; -- Dampen Magic Rank 4, was 28000
UPDATE `trainer_spell` SET `MoneyCost`=30000 WHERE `TrainerId`=1 AND `SpellId`=10174; -- Dampen Magic Rank 5, was 42000
UPDATE `trainer_spell` SET `MoneyCost`=24000 WHERE `TrainerId`=1 AND `SpellId`=10177; -- Frost Ward Rank 4, was 35000
UPDATE `trainer_spell` SET `MoneyCost`=15000 WHERE `TrainerId`=1 AND `SpellId`=10179; -- Frostbolt Rank 8, was 23000
UPDATE `trainer_spell` SET `MoneyCost`=23000 WHERE `TrainerId`=1 AND `SpellId`=10180; -- Frostbolt Rank 9, was 32000
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=1 AND `SpellId`=10181; -- Frostbolt Rank 10, was 38000
UPDATE `trainer_spell` SET `MoneyCost`=15000 WHERE `TrainerId`=1 AND `SpellId`=10185; -- Blizzard Rank 4, was 23000
UPDATE `trainer_spell` SET `MoneyCost`=24000 WHERE `TrainerId`=1 AND `SpellId`=10186; -- Blizzard Rank 5, was 35000
UPDATE `trainer_spell` SET `MoneyCost`=30000 WHERE `TrainerId`=1 AND `SpellId`=10187; -- Blizzard Rank 6, was 42000
UPDATE `trainer_spell` SET `MoneyCost`=15000 WHERE `TrainerId`=1 AND `SpellId`=10191; -- Mana Shield Rank 4, was 23000
UPDATE `trainer_spell` SET `MoneyCost`=24000 WHERE `TrainerId`=1 AND `SpellId`=10192; -- Mana Shield Rank 5, was 35000
UPDATE `trainer_spell` SET `MoneyCost`=30000 WHERE `TrainerId`=1 AND `SpellId`=10193; -- Mana Shield Rank 6, was 42000
UPDATE `trainer_spell` SET `MoneyCost`=20000 WHERE `TrainerId`=1 AND `SpellId`=10197; -- Fire Blast Rank 6, was 26000
UPDATE `trainer_spell` SET `MoneyCost`=26000 WHERE `TrainerId`=1 AND `SpellId`=10199; -- Fire Blast Rank 7, was 36000
UPDATE `trainer_spell` SET `MoneyCost`=20000 WHERE `TrainerId`=1 AND `SpellId`=10201; -- Arcane Explosion Rank 5, was 26000
UPDATE `trainer_spell` SET `MoneyCost`=26000 WHERE `TrainerId`=1 AND `SpellId`=10202; -- Arcane Explosion Rank 6, was 36000
UPDATE `trainer_spell` SET `MoneyCost`=20000 WHERE `TrainerId`=1 AND `SpellId`=10205; -- Scorch Rank 5, was 26000
UPDATE `trainer_spell` SET `MoneyCost`=24000 WHERE `TrainerId`=1 AND `SpellId`=10206; -- Scorch Rank 6, was 35000
UPDATE `trainer_spell` SET `MoneyCost`=29000 WHERE `TrainerId`=1 AND `SpellId`=10207; -- Scorch Rank 7, was 40000
UPDATE `trainer_spell` SET `MoneyCost`=21000 WHERE `TrainerId`=1 AND `SpellId`=10211; -- Arcane Missiles Rank 6, was 28000
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=1 AND `SpellId`=10212; -- Arcane Missiles Rank 7, was 38000
UPDATE `trainer_spell` SET `MoneyCost`=21000 WHERE `TrainerId`=1 AND `SpellId`=10215; -- Flamestrike Rank 5, was 28000
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=1 AND `SpellId`=10216; -- Flamestrike Rank 6, was 38000
UPDATE `trainer_spell` SET `MoneyCost`=23000 WHERE `TrainerId`=1 AND `SpellId`=10219; -- Ice Armor Rank 3, was 32000
UPDATE `trainer_spell` SET `MoneyCost`=30000 WHERE `TrainerId`=1 AND `SpellId`=10220; -- Ice Armor Rank 4, was 42000
UPDATE `trainer_spell` SET `MoneyCost`=23000 WHERE `TrainerId`=1 AND `SpellId`=10223; -- Fire Ward Rank 4, was 32000
UPDATE `trainer_spell` SET `MoneyCost`=30000 WHERE `TrainerId`=1 AND `SpellId`=10225; -- Fire Ward Rank 5, was 42000
UPDATE `trainer_spell` SET `MoneyCost`=26000 WHERE `TrainerId`=1 AND `SpellId`=10230; -- Frost Nova Rank 4, was 36000
UPDATE `trainer_spell` SET `MoneyCost`=2100 WHERE `TrainerId`=1 AND `SpellId`=12051; -- Evocation , was 2000
UPDATE `trainer_spell` SET `MoneyCost`=4300 WHERE `TrainerId`=1 AND `SpellId`=12505; -- Pyroblast Rank 2, was 200
UPDATE `trainer_spell` SET `MoneyCost`=6200 WHERE `TrainerId`=1 AND `SpellId`=12522; -- Pyroblast Rank 3, was 400
UPDATE `trainer_spell` SET `MoneyCost`=11000 WHERE `TrainerId`=1 AND `SpellId`=12523; -- Pyroblast Rank 4, was 650
UPDATE `trainer_spell` SET `MoneyCost`=14000 WHERE `TrainerId`=1 AND `SpellId`=12524; -- Pyroblast Rank 5, was 900
UPDATE `trainer_spell` SET `MoneyCost`=21000 WHERE `TrainerId`=1 AND `SpellId`=12525; -- Pyroblast Rank 6, was 1400
UPDATE `trainer_spell` SET `MoneyCost`=26000 WHERE `TrainerId`=1 AND `SpellId`=12526; -- Pyroblast Rank 7, was 1800
UPDATE `trainer_spell` SET `MoneyCost`=2100 WHERE `TrainerId`=1 AND `SpellId`=12824; -- Polymorph Rank 2, was 2000
UPDATE `trainer_spell` SET `MoneyCost`=13000 WHERE `TrainerId`=1 AND `SpellId`=12825; -- Polymorph Rank 3, was 15000
UPDATE `trainer_spell` SET `MoneyCost`=30000 WHERE `TrainerId`=1 AND `SpellId`=12826; -- Polymorph Rank 4, was 42000
UPDATE `trainer_spell` SET `MoneyCost`=11000 WHERE `TrainerId`=1 AND `SpellId`=13018; -- Blast Wave Rank 2, was 650
UPDATE `trainer_spell` SET `MoneyCost`=15000 WHERE `TrainerId`=1 AND `SpellId`=13019; -- Blast Wave Rank 3, was 1150
UPDATE `trainer_spell` SET `MoneyCost`=24000 WHERE `TrainerId`=1 AND `SpellId`=13020; -- Blast Wave Rank 4, was 1750
UPDATE `trainer_spell` SET `MoneyCost`=30000 WHERE `TrainerId`=1 AND `SpellId`=13021; -- Blast Wave Rank 5, was 2100
UPDATE `trainer_spell` SET `MoneyCost`=20000 WHERE `TrainerId`=1 AND `SpellId`=13031; -- Ice Barrier Rank 2, was 1300
UPDATE `trainer_spell` SET `MoneyCost`=24000 WHERE `TrainerId`=1 AND `SpellId`=13032; -- Ice Barrier Rank 3, was 1750
UPDATE `trainer_spell` SET `MoneyCost`=29000 WHERE `TrainerId`=1 AND `SpellId`=13033; -- Ice Barrier Rank 4, was 2000
UPDATE `trainer_spell` SET `MoneyCost`=30000 WHERE `TrainerId`=1 AND `SpellId`=18809; -- Pyroblast Rank 8, was 2100
UPDATE `trainer_spell` SET `MoneyCost`=21000 WHERE `TrainerId`=1 AND `SpellId`=22782; -- Mage Armor Rank 2, was 28000
UPDATE `trainer_spell` SET `MoneyCost`=29000 WHERE `TrainerId`=1 AND `SpellId`=22783; -- Mage Armor Rank 3, was 40000
UPDATE `trainer_spell` SET `MoneyCost`=5500 WHERE `TrainerId`=1 AND `SpellId`=400640; -- Ice Lance Rank 2, was 3000
UPDATE `trainer_spell` SET `MoneyCost`=13000 WHERE `TrainerId`=1 AND `SpellId`=401502; -- Frostfire Bolt Rank 1, was 15000
UPDATE `trainer_spell` SET `MoneyCost`=23000 WHERE `TrainerId`=1 AND `SpellId`=1237312; -- Frostfire Bolt Rank 2, was 32000
UPDATE `trainer_spell` SET `MoneyCost`=30000 WHERE `TrainerId`=1 AND `SpellId`=1237313; -- Frostfire Bolt Rank 3, was 42000
UPDATE `trainer_spell` SET `MoneyCost`=6200 WHERE `TrainerId`=1 AND `SpellId`=1239696; -- Arcane Blast Rank 2, was 8000
UPDATE `trainer_spell` SET `MoneyCost`=13000 WHERE `TrainerId`=1 AND `SpellId`=1239697; -- Arcane Blast Rank 3, was 15000
UPDATE `trainer_spell` SET `MoneyCost`=23000 WHERE `TrainerId`=1 AND `SpellId`=1239699; -- Arcane Blast Rank 4, was 32000
UPDATE `trainer_spell` SET `MoneyCost`=30000 WHERE `TrainerId`=1 AND `SpellId`=1239700; -- Arcane Blast Rank 5, was 42000
UPDATE `trainer_spell` SET `MoneyCost`=9000 WHERE `TrainerId`=1 AND `SpellId`=1240044; -- Ice Lance Rank 3, was 14500
UPDATE `trainer_spell` SET `MoneyCost`=14000 WHERE `TrainerId`=1 AND `SpellId`=1240045; -- Ice Lance Rank 4, was 18000
UPDATE `trainer_spell` SET `MoneyCost`=21000 WHERE `TrainerId`=1 AND `SpellId`=1240046; -- Ice Lance Rank 5, was 32000
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=1 AND `SpellId`=1240047; -- Ice Lance Rank 6, was 40000
UPDATE `trainer_spell` SET `MoneyCost`=11000 WHERE `TrainerId`=1 AND `SpellId`=1326080; -- Arcane Wrath Rank 2, was 11578
UPDATE `trainer_spell` SET `MoneyCost`=22000 WHERE `TrainerId`=1 AND `SpellId`=1326081; -- Arcane Wrath Rank 3, was 23157
UPDATE `trainer_spell` SET `MoneyCost`=30000 WHERE `TrainerId`=1 AND `SpellId`=1326082; -- Arcane Wrath Rank 4, was 32631

-- trainer 8 (Brother Benjamin, Dark Cleric Beryl, Priestess Josetta): 155 spells
UPDATE `trainer_spell` SET `MoneyCost`=2100 WHERE `TrainerId`=8 AND `SpellId`=453; -- Mind Soothe Rank 1, was 3000
UPDATE `trainer_spell` SET `MoneyCost`=1500 WHERE `TrainerId`=8 AND `SpellId`=527; -- Dispel Magic Rank 1, was 2000
UPDATE `trainer_spell` SET `MoneyCost`=900 WHERE `TrainerId`=8 AND `SpellId`=528; -- Cure Disease , was 1200
UPDATE `trainer_spell` SET `MoneyCost`=8000 WHERE `TrainerId`=8 AND `SpellId`=552; -- Abolish Disease , was 11000
UPDATE `trainer_spell` SET `MoneyCost`=5600 WHERE `TrainerId`=8 AND `SpellId`=596; -- Prayer of Healing Rank 1, was 10000
UPDATE `trainer_spell` SET `MoneyCost`=900 WHERE `TrainerId`=8 AND `SpellId`=598; -- Smite Rank 3, was 1200
UPDATE `trainer_spell` SET `MoneyCost`=1500 WHERE `TrainerId`=8 AND `SpellId`=600; -- Power Word: Shield Rank 3, was 2000
UPDATE `trainer_spell` SET `MoneyCost`=5600 WHERE `TrainerId`=8 AND `SpellId`=602; -- Inner Fire Rank 3, was 10000
UPDATE `trainer_spell` SET `MoneyCost`=5600 WHERE `TrainerId`=8 AND `SpellId`=605; -- Mind Control Rank 1, was 10000
UPDATE `trainer_spell` SET `MoneyCost`=1500 WHERE `TrainerId`=8 AND `SpellId`=970; -- Shadow Word: Pain Rank 3, was 2000
UPDATE `trainer_spell` SET `MoneyCost`=5600 WHERE `TrainerId`=8 AND `SpellId`=976; -- Shadow Protection Rank 1, was 10000
UPDATE `trainer_spell` SET `MoneyCost`=3300 WHERE `TrainerId`=8 AND `SpellId`=984; -- Smite Rank 4, was 4000
UPDATE `trainer_spell` SET `MoneyCost`=10000 WHERE `TrainerId`=8 AND `SpellId`=988; -- Dispel Magic Rank 2, was 14000
UPDATE `trainer_spell` SET `MoneyCost`=5200 WHERE `TrainerId`=8 AND `SpellId`=992; -- Shadow Word: Pain Rank 4, was 6000
UPDATE `trainer_spell` SET `MoneyCost`=13000 WHERE `TrainerId`=8 AND `SpellId`=996; -- Prayer of Healing Rank 2, was 18000
UPDATE `trainer_spell` SET `MoneyCost`=5600 WHERE `TrainerId`=8 AND `SpellId`=1004; -- Smite Rank 5, was 10000
UPDATE `trainer_spell` SET `MoneyCost`=13000 WHERE `TrainerId`=8 AND `SpellId`=1006; -- Inner Fire Rank 4, was 18000
UPDATE `trainer_spell` SET `MoneyCost`=9000 WHERE `TrainerId`=8 AND `SpellId`=1706; -- Levitate , was 12000
UPDATE `trainer_spell` SET `MoneyCost`=3300 WHERE `TrainerId`=8 AND `SpellId`=2010; -- Resurrection Rank 2, was 4000
UPDATE `trainer_spell` SET `MoneyCost`=1400 WHERE `TrainerId`=8 AND `SpellId`=2054; -- Heal Rank 1, was 1600
UPDATE `trainer_spell` SET `MoneyCost`=3300 WHERE `TrainerId`=8 AND `SpellId`=2055; -- Heal Rank 2, was 4000
UPDATE `trainer_spell` SET `MoneyCost`=13000 WHERE `TrainerId`=8 AND `SpellId`=2060; -- Greater Heal Rank 1, was 18000
UPDATE `trainer_spell` SET `MoneyCost`=2100 WHERE `TrainerId`=8 AND `SpellId`=2061; -- Flash Heal Rank 1, was 3000
UPDATE `trainer_spell` SET `MoneyCost`=3300 WHERE `TrainerId`=8 AND `SpellId`=2096; -- Mind Vision Rank 1, was 4000
UPDATE `trainer_spell` SET `MoneyCost`=9000 WHERE `TrainerId`=8 AND `SpellId`=2767; -- Shadow Word: Pain Rank 5, was 12000
UPDATE `trainer_spell` SET `MoneyCost`=10000 WHERE `TrainerId`=8 AND `SpellId`=2791; -- Power Word: Fortitude Rank 4, was 14000
UPDATE `trainer_spell` SET `MoneyCost`=2100 WHERE `TrainerId`=8 AND `SpellId`=2944; -- Devouring Plague Rank 1, was 300
UPDATE `trainer_spell` SET `MoneyCost`=12000 WHERE `TrainerId`=8 AND `SpellId`=6060; -- Smite Rank 6, was 16000
UPDATE `trainer_spell` SET `MoneyCost`=5300 WHERE `TrainerId`=8 AND `SpellId`=6063; -- Heal Rank 3, was 8000
UPDATE `trainer_spell` SET `MoneyCost`=9000 WHERE `TrainerId`=8 AND `SpellId`=6064; -- Heal Rank 4, was 12000
UPDATE `trainer_spell` SET `MoneyCost`=5600 WHERE `TrainerId`=8 AND `SpellId`=6065; -- Power Word: Shield Rank 5, was 10000
UPDATE `trainer_spell` SET `MoneyCost`=10000 WHERE `TrainerId`=8 AND `SpellId`=6066; -- Power Word: Shield Rank 6, was 14000
UPDATE `trainer_spell` SET `MoneyCost`=900 WHERE `TrainerId`=8 AND `SpellId`=6074; -- Renew Rank 2, was 1200
UPDATE `trainer_spell` SET `MoneyCost`=2100 WHERE `TrainerId`=8 AND `SpellId`=6075; -- Renew Rank 3, was 3000
UPDATE `trainer_spell` SET `MoneyCost`=5200 WHERE `TrainerId`=8 AND `SpellId`=6076; -- Renew Rank 4, was 6000
UPDATE `trainer_spell` SET `MoneyCost`=8000 WHERE `TrainerId`=8 AND `SpellId`=6077; -- Renew Rank 5, was 11000
UPDATE `trainer_spell` SET `MoneyCost`=12000 WHERE `TrainerId`=8 AND `SpellId`=6078; -- Renew Rank 6, was 16000
UPDATE `trainer_spell` SET `MoneyCost`=2100 WHERE `TrainerId`=8 AND `SpellId`=6346; -- Fear Ward , was 300
UPDATE `trainer_spell` SET `MoneyCost`=2100 WHERE `TrainerId`=8 AND `SpellId`=7128; -- Inner Fire Rank 2, was 3000
UPDATE `trainer_spell` SET `MoneyCost`=1400 WHERE `TrainerId`=8 AND `SpellId`=8102; -- Mind Blast Rank 2, was 1600
UPDATE `trainer_spell` SET `MoneyCost`=3300 WHERE `TrainerId`=8 AND `SpellId`=8103; -- Mind Blast Rank 3, was 4000
UPDATE `trainer_spell` SET `MoneyCost`=5300 WHERE `TrainerId`=8 AND `SpellId`=8104; -- Mind Blast Rank 4, was 8000
UPDATE `trainer_spell` SET `MoneyCost`=9000 WHERE `TrainerId`=8 AND `SpellId`=8105; -- Mind Blast Rank 5, was 12000
UPDATE `trainer_spell` SET `MoneyCost`=13000 WHERE `TrainerId`=8 AND `SpellId`=8106; -- Mind Blast Rank 6, was 18000
UPDATE `trainer_spell` SET `MoneyCost`=900 WHERE `TrainerId`=8 AND `SpellId`=8122; -- Psychic Scream Rank 1, was 1200
UPDATE `trainer_spell` SET `MoneyCost`=5300 WHERE `TrainerId`=8 AND `SpellId`=8124; -- Psychic Scream Rank 2, was 8000
UPDATE `trainer_spell` SET `MoneyCost`=8000 WHERE `TrainerId`=8 AND `SpellId`=8131; -- Mana Burn Rank 2, was 11000
UPDATE `trainer_spell` SET `MoneyCost`=10000 WHERE `TrainerId`=8 AND `SpellId`=8192; -- Mind Soothe Rank 2, was 14000
UPDATE `trainer_spell` SET `MoneyCost`=5200 WHERE `TrainerId`=8 AND `SpellId`=9472; -- Flash Heal Rank 2, was 6000
UPDATE `trainer_spell` SET `MoneyCost`=8000 WHERE `TrainerId`=8 AND `SpellId`=9473; -- Flash Heal Rank 3, was 11000
UPDATE `trainer_spell` SET `MoneyCost`=12000 WHERE `TrainerId`=8 AND `SpellId`=9474; -- Flash Heal Rank 4, was 16000
UPDATE `trainer_spell` SET `MoneyCost`=2100 WHERE `TrainerId`=8 AND `SpellId`=9484; -- Shackle Undead Rank 1, was 3000
UPDATE `trainer_spell` SET `MoneyCost`=13000 WHERE `TrainerId`=8 AND `SpellId`=9485; -- Shackle Undead Rank 2, was 18000
UPDATE `trainer_spell` SET `MoneyCost`=2100 WHERE `TrainerId`=8 AND `SpellId`=9578; -- Fade Rank 2, was 3000
UPDATE `trainer_spell` SET `MoneyCost`=5600 WHERE `TrainerId`=8 AND `SpellId`=9579; -- Fade Rank 3, was 10000
UPDATE `trainer_spell` SET `MoneyCost`=13000 WHERE `TrainerId`=8 AND `SpellId`=9592; -- Fade Rank 4, was 18000
UPDATE `trainer_spell` SET `MoneyCost`=13000 WHERE `TrainerId`=8 AND `SpellId`=10874; -- Mana Burn Rank 3, was 18000
UPDATE `trainer_spell` SET `MoneyCost`=18000 WHERE `TrainerId`=8 AND `SpellId`=10875; -- Mana Burn Rank 4, was 28000
UPDATE `trainer_spell` SET `MoneyCost`=26000 WHERE `TrainerId`=8 AND `SpellId`=10876; -- Mana Burn Rank 5, was 42000
UPDATE `trainer_spell` SET `MoneyCost`=9000 WHERE `TrainerId`=8 AND `SpellId`=10880; -- Resurrection Rank 3, was 12000
UPDATE `trainer_spell` SET `MoneyCost`=17000 WHERE `TrainerId`=8 AND `SpellId`=10881; -- Resurrection Rank 4, was 26000
UPDATE `trainer_spell` SET `MoneyCost`=14000 WHERE `TrainerId`=8 AND `SpellId`=10888; -- Psychic Scream Rank 3, was 22000
UPDATE `trainer_spell` SET `MoneyCost`=26000 WHERE `TrainerId`=8 AND `SpellId`=10890; -- Psychic Scream Rank 4, was 42000
UPDATE `trainer_spell` SET `MoneyCost`=14000 WHERE `TrainerId`=8 AND `SpellId`=10892; -- Shadow Word: Pain Rank 6, was 22000
UPDATE `trainer_spell` SET `MoneyCost`=20000 WHERE `TrainerId`=8 AND `SpellId`=10893; -- Shadow Word: Pain Rank 7, was 30000
UPDATE `trainer_spell` SET `MoneyCost`=27000 WHERE `TrainerId`=8 AND `SpellId`=10894; -- Shadow Word: Pain Rank 8, was 44000
UPDATE `trainer_spell` SET `MoneyCost`=14000 WHERE `TrainerId`=8 AND `SpellId`=10898; -- Power Word: Shield Rank 7, was 22000
UPDATE `trainer_spell` SET `MoneyCost`=18000 WHERE `TrainerId`=8 AND `SpellId`=10899; -- Power Word: Shield Rank 8, was 28000
UPDATE `trainer_spell` SET `MoneyCost`=25000 WHERE `TrainerId`=8 AND `SpellId`=10900; -- Power Word: Shield Rank 9, was 40000
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=8 AND `SpellId`=10901; -- Power Word: Shield Rank 10, was 46000
UPDATE `trainer_spell` SET `MoneyCost`=16000 WHERE `TrainerId`=8 AND `SpellId`=10909; -- Mind Vision Rank 2, was 24000
UPDATE `trainer_spell` SET `MoneyCost`=16000 WHERE `TrainerId`=8 AND `SpellId`=10911; -- Mind Control Rank 2, was 24000
UPDATE `trainer_spell` SET `MoneyCost`=27000 WHERE `TrainerId`=8 AND `SpellId`=10912; -- Mind Control Rank 3, was 44000
UPDATE `trainer_spell` SET `MoneyCost`=16000 WHERE `TrainerId`=8 AND `SpellId`=10915; -- Flash Heal Rank 5, was 24000
UPDATE `trainer_spell` SET `MoneyCost`=20000 WHERE `TrainerId`=8 AND `SpellId`=10916; -- Flash Heal Rank 6, was 30000
UPDATE `trainer_spell` SET `MoneyCost`=26000 WHERE `TrainerId`=8 AND `SpellId`=10917; -- Flash Heal Rank 7, was 42000
UPDATE `trainer_spell` SET `MoneyCost`=16000 WHERE `TrainerId`=8 AND `SpellId`=10927; -- Renew Rank 7, was 24000
UPDATE `trainer_spell` SET `MoneyCost`=20000 WHERE `TrainerId`=8 AND `SpellId`=10928; -- Renew Rank 8, was 30000
UPDATE `trainer_spell` SET `MoneyCost`=26000 WHERE `TrainerId`=8 AND `SpellId`=10929; -- Renew Rank 9, was 42000
UPDATE `trainer_spell` SET `MoneyCost`=17000 WHERE `TrainerId`=8 AND `SpellId`=10933; -- Smite Rank 7, was 26000
UPDATE `trainer_spell` SET `MoneyCost`=25000 WHERE `TrainerId`=8 AND `SpellId`=10934; -- Smite Rank 8, was 40000
UPDATE `trainer_spell` SET `MoneyCost`=18000 WHERE `TrainerId`=8 AND `SpellId`=10937; -- Power Word: Fortitude Rank 5, was 28000
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=8 AND `SpellId`=10938; -- Power Word: Fortitude Rank 6, was 46000
UPDATE `trainer_spell` SET `MoneyCost`=20000 WHERE `TrainerId`=8 AND `SpellId`=10941; -- Fade Rank 5, was 30000
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=8 AND `SpellId`=10942; -- Fade Rank 6, was 46000
UPDATE `trainer_spell` SET `MoneyCost`=17000 WHERE `TrainerId`=8 AND `SpellId`=10945; -- Mind Blast Rank 7, was 26000
UPDATE `trainer_spell` SET `MoneyCost`=24000 WHERE `TrainerId`=8 AND `SpellId`=10946; -- Mind Blast Rank 8, was 38000
UPDATE `trainer_spell` SET `MoneyCost`=27000 WHERE `TrainerId`=8 AND `SpellId`=10947; -- Mind Blast Rank 9, was 44000
UPDATE `trainer_spell` SET `MoneyCost`=20000 WHERE `TrainerId`=8 AND `SpellId`=10951; -- Inner Fire Rank 5, was 30000
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=8 AND `SpellId`=10952; -- Inner Fire Rank 6, was 46000
UPDATE `trainer_spell` SET `MoneyCost`=24000 WHERE `TrainerId`=8 AND `SpellId`=10953; -- Mind Soothe Rank 3, was 38000
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=8 AND `SpellId`=10955; -- Shackle Undead Rank 3, was 46000
UPDATE `trainer_spell` SET `MoneyCost`=14000 WHERE `TrainerId`=8 AND `SpellId`=10957; -- Shadow Protection Rank 2, was 22000
UPDATE `trainer_spell` SET `MoneyCost`=26000 WHERE `TrainerId`=8 AND `SpellId`=10958; -- Shadow Protection Rank 3, was 42000
UPDATE `trainer_spell` SET `MoneyCost`=20000 WHERE `TrainerId`=8 AND `SpellId`=10960; -- Prayer of Healing Rank 3, was 30000
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=8 AND `SpellId`=10961; -- Prayer of Healing Rank 4, was 46000
UPDATE `trainer_spell` SET `MoneyCost`=17000 WHERE `TrainerId`=8 AND `SpellId`=10963; -- Greater Heal Rank 2, was 26000
UPDATE `trainer_spell` SET `MoneyCost`=24000 WHERE `TrainerId`=8 AND `SpellId`=10964; -- Greater Heal Rank 3, was 38000
UPDATE `trainer_spell` SET `MoneyCost`=27000 WHERE `TrainerId`=8 AND `SpellId`=10965; -- Greater Heal Rank 4, was 44000
UPDATE `trainer_spell` SET `MoneyCost`=5600 WHERE `TrainerId`=8 AND `SpellId`=14752; -- Divine Spirit Rank 1, was 10000
UPDATE `trainer_spell` SET `MoneyCost`=13000 WHERE `TrainerId`=8 AND `SpellId`=14818; -- Divine Spirit Rank 2, was 900
UPDATE `trainer_spell` SET `MoneyCost`=20000 WHERE `TrainerId`=8 AND `SpellId`=14819; -- Divine Spirit Rank 3, was 1500
UPDATE `trainer_spell` SET `MoneyCost`=2100 WHERE `TrainerId`=8 AND `SpellId`=14914; -- Holy Fire Rank 1, was 3000
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=8 AND `SpellId`=15261; -- Holy Fire Rank 8, was 46000
UPDATE `trainer_spell` SET `MoneyCost`=5600 WHERE `TrainerId`=8 AND `SpellId`=15263; -- Holy Fire Rank 3, was 10000
UPDATE `trainer_spell` SET `MoneyCost`=10000 WHERE `TrainerId`=8 AND `SpellId`=15264; -- Holy Fire Rank 4, was 14000
UPDATE `trainer_spell` SET `MoneyCost`=14000 WHERE `TrainerId`=8 AND `SpellId`=15265; -- Holy Fire Rank 5, was 22000
UPDATE `trainer_spell` SET `MoneyCost`=18000 WHERE `TrainerId`=8 AND `SpellId`=15266; -- Holy Fire Rank 6, was 28000
UPDATE `trainer_spell` SET `MoneyCost`=25000 WHERE `TrainerId`=8 AND `SpellId`=15267; -- Holy Fire Rank 7, was 40000
UPDATE `trainer_spell` SET `MoneyCost`=5300 WHERE `TrainerId`=8 AND `SpellId`=15430; -- Holy Nova Rank 2, was 400
UPDATE `trainer_spell` SET `MoneyCost`=10000 WHERE `TrainerId`=8 AND `SpellId`=15431; -- Holy Nova Rank 3, was 700
UPDATE `trainer_spell` SET `MoneyCost`=5300 WHERE `TrainerId`=8 AND `SpellId`=17311; -- Mind Flay Rank 2, was 400
UPDATE `trainer_spell` SET `MoneyCost`=10000 WHERE `TrainerId`=8 AND `SpellId`=17312; -- Mind Flay Rank 3, was 700
UPDATE `trainer_spell` SET `MoneyCost`=16000 WHERE `TrainerId`=8 AND `SpellId`=17313; -- Mind Flay Rank 4, was 1200
UPDATE `trainer_spell` SET `MoneyCost`=24000 WHERE `TrainerId`=8 AND `SpellId`=17314; -- Mind Flay Rank 5, was 1900
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=8 AND `SpellId`=18807; -- Mind Flay Rank 6, was 2300
UPDATE `trainer_spell` SET `MoneyCost`=2000 WHERE `TrainerId`=8 AND `SpellId`=19261; -- Touch of Weakness Rank 2, was 150
UPDATE `trainer_spell` SET `MoneyCost`=6000 WHERE `TrainerId`=8 AND `SpellId`=19262; -- Touch of Weakness Rank 3, was 500
UPDATE `trainer_spell` SET `MoneyCost`=13000 WHERE `TrainerId`=8 AND `SpellId`=19264; -- Touch of Weakness Rank 4, was 900
UPDATE `trainer_spell` SET `MoneyCost`=20000 WHERE `TrainerId`=8 AND `SpellId`=19265; -- Touch of Weakness Rank 5, was 1500
UPDATE `trainer_spell` SET `MoneyCost`=29000 WHERE `TrainerId`=8 AND `SpellId`=19266; -- Touch of Weakness Rank 6, was 2300
UPDATE `trainer_spell` SET `MoneyCost`=5600 WHERE `TrainerId`=8 AND `SpellId`=19271; -- Feedback Rank 2, was 500
UPDATE `trainer_spell` SET `MoneyCost`=13000 WHERE `TrainerId`=8 AND `SpellId`=19273; -- Feedback Rank 3, was 900
UPDATE `trainer_spell` SET `MoneyCost`=20000 WHERE `TrainerId`=8 AND `SpellId`=19274; -- Feedback Rank 4, was 1500
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=8 AND `SpellId`=19275; -- Feedback Rank 5, was 2300
UPDATE `trainer_spell` SET `MoneyCost`=5300 WHERE `TrainerId`=8 AND `SpellId`=19276; -- Devouring Plague Rank 2, was 400
UPDATE `trainer_spell` SET `MoneyCost`=10000 WHERE `TrainerId`=8 AND `SpellId`=19277; -- Devouring Plague Rank 3, was 700
UPDATE `trainer_spell` SET `MoneyCost`=16000 WHERE `TrainerId`=8 AND `SpellId`=19278; -- Devouring Plague Rank 4, was 1200
UPDATE `trainer_spell` SET `MoneyCost`=24000 WHERE `TrainerId`=8 AND `SpellId`=19279; -- Devouring Plague Rank 5, was 1900
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=8 AND `SpellId`=19280; -- Devouring Plague Rank 6, was 2300
UPDATE `trainer_spell` SET `MoneyCost`=27000 WHERE `TrainerId`=8 AND `SpellId`=20770; -- Resurrection Rank 5, was 44000
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=8 AND `SpellId`=27681; -- Prayer of Spirit Rank 1, was 2300
UPDATE `trainer_spell` SET `MoneyCost`=16000 WHERE `TrainerId`=8 AND `SpellId`=27799; -- Holy Nova Rank 4, was 1200
UPDATE `trainer_spell` SET `MoneyCost`=24000 WHERE `TrainerId`=8 AND `SpellId`=27800; -- Holy Nova Rank 5, was 1900
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=8 AND `SpellId`=27801; -- Holy Nova Rank 6, was 2300
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=8 AND `SpellId`=27841; -- Divine Spirit Rank 4, was 2300
UPDATE `trainer_spell` SET `MoneyCost`=13000 WHERE `TrainerId`=8 AND `SpellId`=1240720; -- Penance Rank 2, was 16000
UPDATE `trainer_spell` SET `MoneyCost`=20000 WHERE `TrainerId`=8 AND `SpellId`=1240721; -- Penance Rank 3, was 30000
UPDATE `trainer_spell` SET `MoneyCost`=20000 WHERE `TrainerId`=8 AND `SpellId`=1240826; -- Prayer of Mending Rank 2, was 30000
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=8 AND `SpellId`=1240827; -- Prayer of Mending Rank 3, was 46000
UPDATE `trainer_spell` SET `MoneyCost`=6000 WHERE `TrainerId`=8 AND `SpellId`=1277325; -- Dark Sacrifice Rank 2, was 150
UPDATE `trainer_spell` SET `MoneyCost`=13000 WHERE `TrainerId`=8 AND `SpellId`=1277326; -- Dark Sacrifice Rank 3, was 500
UPDATE `trainer_spell` SET `MoneyCost`=20000 WHERE `TrainerId`=8 AND `SpellId`=1277327; -- Dark Sacrifice Rank 4, was 900
UPDATE `trainer_spell` SET `MoneyCost`=29000 WHERE `TrainerId`=8 AND `SpellId`=1277328; -- Dark Sacrifice Rank 5, was 1500
UPDATE `trainer_spell` SET `MoneyCost`=1500 WHERE `TrainerId`=8 AND `SpellId`=1277371; -- Divine Grace Rank 2, was 100
UPDATE `trainer_spell` SET `MoneyCost`=5200 WHERE `TrainerId`=8 AND `SpellId`=1277372; -- Divine Grace Rank 3, was 300
UPDATE `trainer_spell` SET `MoneyCost`=9000 WHERE `TrainerId`=8 AND `SpellId`=1277374; -- Divine Grace Rank 4, was 600
UPDATE `trainer_spell` SET `MoneyCost`=14000 WHERE `TrainerId`=8 AND `SpellId`=1277376; -- Divine Grace Rank 5, was 1100
UPDATE `trainer_spell` SET `MoneyCost`=20000 WHERE `TrainerId`=8 AND `SpellId`=1277377; -- Divine Grace Rank 6, was 1500
UPDATE `trainer_spell` SET `MoneyCost`=27000 WHERE `TrainerId`=8 AND `SpellId`=1277378; -- Divine Grace Rank 7, was 2200
UPDATE `trainer_spell` SET `MoneyCost`=8000 WHERE `TrainerId`=8 AND `SpellId`=1309595; -- Shadow Word: Death Rank 1, was 11000
UPDATE `trainer_spell` SET `MoneyCost`=13000 WHERE `TrainerId`=8 AND `SpellId`=1309633; -- Shadow Word: Death Rank 2, was 18000
UPDATE `trainer_spell` SET `MoneyCost`=18000 WHERE `TrainerId`=8 AND `SpellId`=1309635; -- Shadow Word: Death Rank 3, was 28000
UPDATE `trainer_spell` SET `MoneyCost`=26000 WHERE `TrainerId`=8 AND `SpellId`=1309636; -- Shadow Word: Death Rank 4, was 42000
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=8 AND `SpellId`=1316995; -- Penance Rank 4, was 46000

-- trainer 10 (Sagorne Creststrider, Tigor Skychaser): 130 spells
UPDATE `trainer_spell` SET `MoneyCost`=3300 WHERE `TrainerId`=10 AND `SpellId`=131; -- Water Breathing , was 3000
UPDATE `trainer_spell` SET `MoneyCost`=1100 WHERE `TrainerId`=10 AND `SpellId`=325; -- Lightning Shield Rank 2, was 1800
UPDATE `trainer_spell` SET `MoneyCost`=700 WHERE `TrainerId`=10 AND `SpellId`=370; -- Purge Rank 1, was 800
UPDATE `trainer_spell` SET `MoneyCost`=7000 WHERE `TrainerId`=10 AND `SpellId`=421; -- Chain Lightning Rank 1, was 8000
UPDATE `trainer_spell` SET `MoneyCost`=1100 WHERE `TrainerId`=10 AND `SpellId`=526; -- Cure Poison , was 1800
UPDATE `trainer_spell` SET `MoneyCost`=5000 WHERE `TrainerId`=10 AND `SpellId`=546; -- Water Walking , was 6000
UPDATE `trainer_spell` SET `MoneyCost`=700 WHERE `TrainerId`=10 AND `SpellId`=547; -- Healing Wave Rank 3, was 800
UPDATE `trainer_spell` SET `MoneyCost`=5700 WHERE `TrainerId`=10 AND `SpellId`=556; -- Astral Recall , was 7000
UPDATE `trainer_spell` SET `MoneyCost`=3400 WHERE `TrainerId`=10 AND `SpellId`=905; -- Lightning Shield Rank 3, was 3500
UPDATE `trainer_spell` SET `MoneyCost`=1200 WHERE `TrainerId`=10 AND `SpellId`=913; -- Healing Wave Rank 4, was 2000
UPDATE `trainer_spell` SET `MoneyCost`=2300 WHERE `TrainerId`=10 AND `SpellId`=915; -- Lightning Bolt Rank 4, was 2200
UPDATE `trainer_spell` SET `MoneyCost`=3400 WHERE `TrainerId`=10 AND `SpellId`=939; -- Healing Wave Rank 5, was 3500
UPDATE `trainer_spell` SET `MoneyCost`=4300 WHERE `TrainerId`=10 AND `SpellId`=943; -- Lightning Bolt Rank 5, was 4000
UPDATE `trainer_spell` SET `MoneyCost`=7000 WHERE `TrainerId`=10 AND `SpellId`=945; -- Lightning Shield Rank 4, was 8000
UPDATE `trainer_spell` SET `MoneyCost`=7000 WHERE `TrainerId`=10 AND `SpellId`=959; -- Healing Wave Rank 6, was 8000
UPDATE `trainer_spell` SET `MoneyCost`=700 WHERE `TrainerId`=10 AND `SpellId`=2008; -- Ancestral Spirit Rank 1, was 800
UPDATE `trainer_spell` SET `MoneyCost`=2300 WHERE `TrainerId`=10 AND `SpellId`=2645; -- Ghost Wolf , was 2200
UPDATE `trainer_spell` SET `MoneyCost`=21000 WHERE `TrainerId`=10 AND `SpellId`=2860; -- Chain Lightning Rank 3, was 22000
UPDATE `trainer_spell` SET `MoneyCost`=3300 WHERE `TrainerId`=10 AND `SpellId`=2870; -- Cure Disease , was 3000
UPDATE `trainer_spell` SET `MoneyCost`=4300 WHERE `TrainerId`=10 AND `SpellId`=5675; -- Mana Spring Totem Rank 1, was 4000
UPDATE `trainer_spell` SET `MoneyCost`=7000 WHERE `TrainerId`=10 AND `SpellId`=6041; -- Lightning Bolt Rank 6, was 8000
UPDATE `trainer_spell` SET `MoneyCost`=4300 WHERE `TrainerId`=10 AND `SpellId`=6196; -- Far Sight , was 4000
UPDATE `trainer_spell` SET `MoneyCost`=2300 WHERE `TrainerId`=10 AND `SpellId`=6363; -- Searing Totem Rank 2, was 2200
UPDATE `trainer_spell` SET `MoneyCost`=5700 WHERE `TrainerId`=10 AND `SpellId`=6364; -- Searing Totem Rank 3, was 7000
UPDATE `trainer_spell` SET `MoneyCost`=5700 WHERE `TrainerId`=10 AND `SpellId`=6375; -- Healing Stream Totem Rank 2, was 7000
UPDATE `trainer_spell` SET `MoneyCost`=1200 WHERE `TrainerId`=10 AND `SpellId`=6390; -- Stoneclaw Totem Rank 2, was 2000
UPDATE `trainer_spell` SET `MoneyCost`=5000 WHERE `TrainerId`=10 AND `SpellId`=6391; -- Stoneclaw Totem Rank 3, was 6000
UPDATE `trainer_spell` SET `MoneyCost`=10000 WHERE `TrainerId`=10 AND `SpellId`=6392; -- Stoneclaw Totem Rank 4, was 11000
UPDATE `trainer_spell` SET `MoneyCost`=8000 WHERE `TrainerId`=10 AND `SpellId`=6495; -- Sentry Totem , was 9000
UPDATE `trainer_spell` SET `MoneyCost`=2300 WHERE `TrainerId`=10 AND `SpellId`=8004; -- Lesser Healing Wave Rank 1, was 2200
UPDATE `trainer_spell` SET `MoneyCost`=5000 WHERE `TrainerId`=10 AND `SpellId`=8008; -- Lesser Healing Wave Rank 2, was 6000
UPDATE `trainer_spell` SET `MoneyCost`=9000 WHERE `TrainerId`=10 AND `SpellId`=8010; -- Lesser Healing Wave Rank 3, was 10000
UPDATE `trainer_spell` SET `MoneyCost`=7000 WHERE `TrainerId`=10 AND `SpellId`=8012; -- Purge Rank 2, was 8000
UPDATE `trainer_spell` SET `MoneyCost`=1100 WHERE `TrainerId`=10 AND `SpellId`=8019; -- Rockbiter Weapon Rank 3, was 1800
UPDATE `trainer_spell` SET `MoneyCost`=1200 WHERE `TrainerId`=10 AND `SpellId`=8027; -- Flametongue Weapon Rank 2, was 2000
UPDATE `trainer_spell` SET `MoneyCost`=4300 WHERE `TrainerId`=10 AND `SpellId`=8030; -- Flametongue Weapon Rank 3, was 4000
UPDATE `trainer_spell` SET `MoneyCost`=2300 WHERE `TrainerId`=10 AND `SpellId`=8033; -- Frostbrand Weapon Rank 1, was 2200
UPDATE `trainer_spell` SET `MoneyCost`=5000 WHERE `TrainerId`=10 AND `SpellId`=8038; -- Frostbrand Weapon Rank 2, was 6000
UPDATE `trainer_spell` SET `MoneyCost`=3400 WHERE `TrainerId`=10 AND `SpellId`=8046; -- Earth Shock Rank 4, was 3500
UPDATE `trainer_spell` SET `MoneyCost`=1200 WHERE `TrainerId`=10 AND `SpellId`=8052; -- Flame Shock Rank 2, was 2000
UPDATE `trainer_spell` SET `MoneyCost`=5000 WHERE `TrainerId`=10 AND `SpellId`=8053; -- Flame Shock Rank 3, was 6000
UPDATE `trainer_spell` SET `MoneyCost`=2300 WHERE `TrainerId`=10 AND `SpellId`=8056; -- Frost Shock Rank 1, was 2200
UPDATE `trainer_spell` SET `MoneyCost`=8000 WHERE `TrainerId`=10 AND `SpellId`=8058; -- Frost Shock Rank 2, was 9000
UPDATE `trainer_spell` SET `MoneyCost`=1200 WHERE `TrainerId`=10 AND `SpellId`=8143; -- Tremor Totem , was 2000
UPDATE `trainer_spell` SET `MoneyCost`=3400 WHERE `TrainerId`=10 AND `SpellId`=8155; -- Stoneskin Totem Rank 3, was 3500
UPDATE `trainer_spell` SET `MoneyCost`=3400 WHERE `TrainerId`=10 AND `SpellId`=8160; -- Strength of Earth Totem Rank 2, was 3500
UPDATE `trainer_spell` SET `MoneyCost`=10000 WHERE `TrainerId`=10 AND `SpellId`=8161; -- Strength of Earth Totem Rank 3, was 11000
UPDATE `trainer_spell` SET `MoneyCost`=3300 WHERE `TrainerId`=10 AND `SpellId`=8166; -- Poison Cleansing Totem , was 3000
UPDATE `trainer_spell` SET `MoneyCost`=10000 WHERE `TrainerId`=10 AND `SpellId`=8170; -- Disease Cleansing Totem , was 11000
UPDATE `trainer_spell` SET `MoneyCost`=5700 WHERE `TrainerId`=10 AND `SpellId`=8177; -- Grounding Totem , was 7000
UPDATE `trainer_spell` SET `MoneyCost`=3400 WHERE `TrainerId`=10 AND `SpellId`=8181; -- Frost Resistance Totem Rank 1, was 3500
UPDATE `trainer_spell` SET `MoneyCost`=5000 WHERE `TrainerId`=10 AND `SpellId`=8184; -- Fire Resistance Totem Rank 1, was 6000
UPDATE `trainer_spell` SET `MoneyCost`=4300 WHERE `TrainerId`=10 AND `SpellId`=8190; -- Magma Totem Rank 1, was 4000
UPDATE `trainer_spell` SET `MoneyCost`=5000 WHERE `TrainerId`=10 AND `SpellId`=8227; -- Flametongue Totem Rank 1, was 6000
UPDATE `trainer_spell` SET `MoneyCost`=5700 WHERE `TrainerId`=10 AND `SpellId`=8232; -- Windfury Weapon Rank 1, was 7000
UPDATE `trainer_spell` SET `MoneyCost`=10000 WHERE `TrainerId`=10 AND `SpellId`=8249; -- Flametongue Totem Rank 2, was 11000
UPDATE `trainer_spell` SET `MoneyCost`=7000 WHERE `TrainerId`=10 AND `SpellId`=8512; -- Windfury Totem Rank 1, was 8000
UPDATE `trainer_spell` SET `MoneyCost`=18000 WHERE `TrainerId`=10 AND `SpellId`=8835; -- Grace of Air Totem Rank 1, was 16000
UPDATE `trainer_spell` SET `MoneyCost`=10000 WHERE `TrainerId`=10 AND `SpellId`=10391; -- Lightning Bolt Rank 7, was 11000
UPDATE `trainer_spell` SET `MoneyCost`=19000 WHERE `TrainerId`=10 AND `SpellId`=10392; -- Lightning Bolt Rank 8, was 18000
UPDATE `trainer_spell` SET `MoneyCost`=21000 WHERE `TrainerId`=10 AND `SpellId`=10395; -- Healing Wave Rank 8, was 22000
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=10 AND `SpellId`=10396; -- Healing Wave Rank 9, was 30000
UPDATE `trainer_spell` SET `MoneyCost`=3400 WHERE `TrainerId`=10 AND `SpellId`=10399; -- Rockbiter Weapon Rank 4, was 3500
UPDATE `trainer_spell` SET `MoneyCost`=8000 WHERE `TrainerId`=10 AND `SpellId`=10406; -- Stoneskin Totem Rank 4, was 9000
UPDATE `trainer_spell` SET `MoneyCost`=19000 WHERE `TrainerId`=10 AND `SpellId`=10407; -- Stoneskin Totem Rank 5, was 18000
UPDATE `trainer_spell` SET `MoneyCost`=27000 WHERE `TrainerId`=10 AND `SpellId`=10408; -- Stoneskin Totem Rank 6, was 29000
UPDATE `trainer_spell` SET `MoneyCost`=9000 WHERE `TrainerId`=10 AND `SpellId`=10412; -- Earth Shock Rank 5, was 10000
UPDATE `trainer_spell` SET `MoneyCost`=21000 WHERE `TrainerId`=10 AND `SpellId`=10413; -- Earth Shock Rank 6, was 22000
UPDATE `trainer_spell` SET `MoneyCost`=33000 WHERE `TrainerId`=10 AND `SpellId`=10414; -- Earth Shock Rank 7, was 34000
UPDATE `trainer_spell` SET `MoneyCost`=21000 WHERE `TrainerId`=10 AND `SpellId`=10427; -- Stoneclaw Totem Rank 5, was 22000
UPDATE `trainer_spell` SET `MoneyCost`=21000 WHERE `TrainerId`=10 AND `SpellId`=10431; -- Lightning Shield Rank 6, was 22000
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=10 AND `SpellId`=10432; -- Lightning Shield Rank 7, was 30000
UPDATE `trainer_spell` SET `MoneyCost`=25000 WHERE `TrainerId`=10 AND `SpellId`=10437; -- Searing Totem Rank 5, was 24000
UPDATE `trainer_spell` SET `MoneyCost`=33000 WHERE `TrainerId`=10 AND `SpellId`=10438; -- Searing Totem Rank 6, was 34000
UPDATE `trainer_spell` SET `MoneyCost`=26000 WHERE `TrainerId`=10 AND `SpellId`=10442; -- Strength of Earth Totem Rank 4, was 27000
UPDATE `trainer_spell` SET `MoneyCost`=26000 WHERE `TrainerId`=10 AND `SpellId`=10448; -- Flame Shock Rank 5, was 27000
UPDATE `trainer_spell` SET `MoneyCost`=10000 WHERE `TrainerId`=10 AND `SpellId`=10456; -- Frostbrand Weapon Rank 3, was 11000
UPDATE `trainer_spell` SET `MoneyCost`=25000 WHERE `TrainerId`=10 AND `SpellId`=10462; -- Healing Stream Totem Rank 4, was 24000
UPDATE `trainer_spell` SET `MoneyCost`=33000 WHERE `TrainerId`=10 AND `SpellId`=10463; -- Healing Stream Totem Rank 5, was 34000
UPDATE `trainer_spell` SET `MoneyCost`=19000 WHERE `TrainerId`=10 AND `SpellId`=10466; -- Lesser Healing Wave Rank 4, was 18000
UPDATE `trainer_spell` SET `MoneyCost`=26000 WHERE `TrainerId`=10 AND `SpellId`=10467; -- Lesser Healing Wave Rank 5, was 27000
UPDATE `trainer_spell` SET `MoneyCost`=33000 WHERE `TrainerId`=10 AND `SpellId`=10468; -- Lesser Healing Wave Rank 6, was 34000
UPDATE `trainer_spell` SET `MoneyCost`=10000 WHERE `TrainerId`=10 AND `SpellId`=10478; -- Frost Resistance Totem Rank 2, was 11000
UPDATE `trainer_spell` SET `MoneyCost`=27000 WHERE `TrainerId`=10 AND `SpellId`=10479; -- Frost Resistance Totem Rank 3, was 29000
UPDATE `trainer_spell` SET `MoneyCost`=25000 WHERE `TrainerId`=10 AND `SpellId`=10486; -- Windfury Weapon Rank 3, was 24000
UPDATE `trainer_spell` SET `MoneyCost`=9000 WHERE `TrainerId`=10 AND `SpellId`=10495; -- Mana Spring Totem Rank 2, was 10000
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=10 AND `SpellId`=10497; -- Mana Spring Totem Rank 4, was 30000
UPDATE `trainer_spell` SET `MoneyCost`=21000 WHERE `TrainerId`=10 AND `SpellId`=10526; -- Flametongue Totem Rank 3, was 22000
UPDATE `trainer_spell` SET `MoneyCost`=18000 WHERE `TrainerId`=10 AND `SpellId`=10537; -- Fire Resistance Totem Rank 2, was 16000
UPDATE `trainer_spell` SET `MoneyCost`=9000 WHERE `TrainerId`=10 AND `SpellId`=10585; -- Magma Totem Rank 2, was 10000
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=10 AND `SpellId`=10587; -- Magma Totem Rank 4, was 30000
UPDATE `trainer_spell` SET `MoneyCost`=5700 WHERE `TrainerId`=10 AND `SpellId`=10595; -- Nature Resistance Totem Rank 1, was 7000
UPDATE `trainer_spell` SET `MoneyCost`=19000 WHERE `TrainerId`=10 AND `SpellId`=10600; -- Nature Resistance Totem Rank 2, was 18000
UPDATE `trainer_spell` SET `MoneyCost`=33000 WHERE `TrainerId`=10 AND `SpellId`=10601; -- Nature Resistance Totem Rank 3, was 34000
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=10 AND `SpellId`=10605; -- Chain Lightning Rank 4, was 30000
UPDATE `trainer_spell` SET `MoneyCost`=18000 WHERE `TrainerId`=10 AND `SpellId`=10613; -- Windfury Totem Rank 2, was 16000
UPDATE `trainer_spell` SET `MoneyCost`=26000 WHERE `TrainerId`=10 AND `SpellId`=10614; -- Windfury Totem Rank 3, was 27000
UPDATE `trainer_spell` SET `MoneyCost`=27000 WHERE `TrainerId`=10 AND `SpellId`=10623; -- Chain Heal Rank 3, was 29000
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=10 AND `SpellId`=10627; -- Grace of Air Totem Rank 2, was 30000
UPDATE `trainer_spell` SET `MoneyCost`=9000 WHERE `TrainerId`=10 AND `SpellId`=15107; -- Windwall Totem Rank 1, was 10000
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=10 AND `SpellId`=15112; -- Windwall Totem Rank 3, was 30000
UPDATE `trainer_spell` SET `MoneyCost`=25000 WHERE `TrainerId`=10 AND `SpellId`=15207; -- Lightning Bolt Rank 9, was 24000
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=10 AND `SpellId`=15208; -- Lightning Bolt Rank 10, was 30000
UPDATE `trainer_spell` SET `MoneyCost`=8000 WHERE `TrainerId`=10 AND `SpellId`=16314; -- Rockbiter Weapon Rank 5, was 9000
UPDATE `trainer_spell` SET `MoneyCost`=19000 WHERE `TrainerId`=10 AND `SpellId`=16315; -- Rockbiter Weapon Rank 6, was 18000
UPDATE `trainer_spell` SET `MoneyCost`=27000 WHERE `TrainerId`=10 AND `SpellId`=16316; -- Rockbiter Weapon Rank 7, was 29000
UPDATE `trainer_spell` SET `MoneyCost`=9000 WHERE `TrainerId`=10 AND `SpellId`=16339; -- Flametongue Weapon Rank 4, was 10000
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=10 AND `SpellId`=16342; -- Flametongue Weapon Rank 6, was 30000
UPDATE `trainer_spell` SET `MoneyCost`=21000 WHERE `TrainerId`=10 AND `SpellId`=16355; -- Frostbrand Weapon Rank 4, was 22000
UPDATE `trainer_spell` SET `MoneyCost`=33000 WHERE `TrainerId`=10 AND `SpellId`=16362; -- Windfury Weapon Rank 4, was 34000
UPDATE `trainer_spell` SET `MoneyCost`=10000 WHERE `TrainerId`=10 AND `SpellId`=17354; -- Mana Tide Totem Rank 2, was 800
UPDATE `trainer_spell` SET `MoneyCost`=21000 WHERE `TrainerId`=10 AND `SpellId`=17359; -- Mana Tide Totem Rank 3, was 1600
UPDATE `trainer_spell` SET `MoneyCost`=5700 WHERE `TrainerId`=10 AND `SpellId`=20608; -- Reincarnation Passive, was 7000
UPDATE `trainer_spell` SET `MoneyCost`=3400 WHERE `TrainerId`=10 AND `SpellId`=20609; -- Ancestral Spirit Rank 2, was 3500
UPDATE `trainer_spell` SET `MoneyCost`=9000 WHERE `TrainerId`=10 AND `SpellId`=20610; -- Ancestral Spirit Rank 3, was 10000
UPDATE `trainer_spell` SET `MoneyCost`=21000 WHERE `TrainerId`=10 AND `SpellId`=20776; -- Ancestral Spirit Rank 4, was 22000
UPDATE `trainer_spell` SET `MoneyCost`=33000 WHERE `TrainerId`=10 AND `SpellId`=20777; -- Ancestral Spirit Rank 5, was 34000
UPDATE `trainer_spell` SET `MoneyCost`=2300 WHERE `TrainerId`=10 AND `SpellId`=36936; -- Totemic Recall , was 7000
UPDATE `trainer_spell` SET `MoneyCost`=2300 WHERE `TrainerId`=10 AND `SpellId`=66842; -- Call of the Elements , was 7000
UPDATE `trainer_spell` SET `MoneyCost`=5700 WHERE `TrainerId`=10 AND `SpellId`=66843; -- Call of the Ancestors , was 7000
UPDATE `trainer_spell` SET `MoneyCost`=12000 WHERE `TrainerId`=10 AND `SpellId`=66844; -- Call of the Spirits , was 7000
UPDATE `trainer_spell` SET `MoneyCost`=700 WHERE `TrainerId`=10 AND `SpellId`=408341; -- Fire Nova Rank 1, was 800
UPDATE `trainer_spell` SET `MoneyCost`=3300 WHERE `TrainerId`=10 AND `SpellId`=408342; -- Fire Nova Rank 2, was 3000
UPDATE `trainer_spell` SET `MoneyCost`=7000 WHERE `TrainerId`=10 AND `SpellId`=408343; -- Fire Nova Rank 3, was 8000
UPDATE `trainer_spell` SET `MoneyCost`=18000 WHERE `TrainerId`=10 AND `SpellId`=408344; -- Fire Nova Rank 4, was 16000
UPDATE `trainer_spell` SET `MoneyCost`=26000 WHERE `TrainerId`=10 AND `SpellId`=408345; -- Fire Nova Rank 5, was 27000
UPDATE `trainer_spell` SET `MoneyCost`=3300 WHERE `TrainerId`=10 AND `SpellId`=437009; -- Totemic Call , was 3000
UPDATE `trainer_spell` SET `MoneyCost`=25000 WHERE `TrainerId`=10 AND `SpellId`=1239242; -- Riptide Rank 2, was 800
UPDATE `trainer_spell` SET `MoneyCost`=33000 WHERE `TrainerId`=10 AND `SpellId`=1239243; -- Riptide Rank 3, was 1600
UPDATE `trainer_spell` SET `MoneyCost`=32000 WHERE `TrainerId`=10 AND `SpellId`=1325256; -- Mana Tide Totem Rank 4, was 1700

-- trainer 14 (Dhugru Gorelust, Grol'dar, Mirket, Zevrost): 136 spells
UPDATE `trainer_spell` SET `MoneyCost`=3800 WHERE `TrainerId`=14 AND `SpellId`=126; -- Eye of Kilrogg Summon, was 2500
UPDATE `trainer_spell` SET `MoneyCost`=5000 WHERE `TrainerId`=14 AND `SpellId`=132; -- Detect Invisibility Rank 1, was 4000
UPDATE `trainer_spell` SET `MoneyCost`=27000 WHERE `TrainerId`=14 AND `SpellId`=603; -- Bane of Doom , was 26000
UPDATE `trainer_spell` SET `MoneyCost`=1000 WHERE `TrainerId`=14 AND `SpellId`=689; -- Drain Life Rank 1, was 900
UPDATE `trainer_spell` SET `MoneyCost`=1700 WHERE `TrainerId`=14 AND `SpellId`=693; -- Create Soulstone Rank 1, was 1500
UPDATE `trainer_spell` SET `MoneyCost`=2500 WHERE `TrainerId`=14 AND `SpellId`=698; -- Ritual of Summoning , was 2000
UPDATE `trainer_spell` SET `MoneyCost`=3800 WHERE `TrainerId`=14 AND `SpellId`=699; -- Drain Life Rank 2, was 2500
UPDATE `trainer_spell` SET `MoneyCost`=1000 WHERE `TrainerId`=14 AND `SpellId`=704; -- Curse of Recklessness Rank 1, was 900
UPDATE `trainer_spell` SET `MoneyCost`=800 WHERE `TrainerId`=14 AND `SpellId`=705; -- Shadow Bolt Rank 3, was 600
UPDATE `trainer_spell` SET `MoneyCost`=2500 WHERE `TrainerId`=14 AND `SpellId`=706; -- Demon Armor Rank 1, was 2000
UPDATE `trainer_spell` SET `MoneyCost`=7000 WHERE `TrainerId`=14 AND `SpellId`=709; -- Drain Life Rank 3, was 6000
UPDATE `trainer_spell` SET `MoneyCost`=6000 WHERE `TrainerId`=14 AND `SpellId`=710; -- Banish Rank 1, was 5000
UPDATE `trainer_spell` SET `MoneyCost`=800 WHERE `TrainerId`=14 AND `SpellId`=755; -- Health Funnel Rank 1, was 600
UPDATE `trainer_spell` SET `MoneyCost`=1700 WHERE `TrainerId`=14 AND `SpellId`=1014; -- Bane of Agony Rank 2, was 1500
UPDATE `trainer_spell` SET `MoneyCost`=7000 WHERE `TrainerId`=14 AND `SpellId`=1086; -- Demon Armor Rank 2, was 6000
UPDATE `trainer_spell` SET `MoneyCost`=2500 WHERE `TrainerId`=14 AND `SpellId`=1088; -- Shadow Bolt Rank 4, was 2000
UPDATE `trainer_spell` SET `MoneyCost`=2500 WHERE `TrainerId`=14 AND `SpellId`=1094; -- Immolate Rank 3, was 2000
UPDATE `trainer_spell` SET `MoneyCost`=7000 WHERE `TrainerId`=14 AND `SpellId`=1098; -- Subjugate Demon Rank 1, was 6000
UPDATE `trainer_spell` SET `MoneyCost`=6000 WHERE `TrainerId`=14 AND `SpellId`=1106; -- Shadow Bolt Rank 5, was 5000
UPDATE `trainer_spell` SET `MoneyCost`=800 WHERE `TrainerId`=14 AND `SpellId`=1108; -- Curse of Weakness Rank 2, was 600
UPDATE `trainer_spell` SET `MoneyCost`=1600 WHERE `TrainerId`=14 AND `SpellId`=1455; -- Life Tap Rank 2, was 1200
UPDATE `trainer_spell` SET `MoneyCost`=5000 WHERE `TrainerId`=14 AND `SpellId`=1456; -- Life Tap Rank 3, was 4000
UPDATE `trainer_spell` SET `MoneyCost`=5000 WHERE `TrainerId`=14 AND `SpellId`=1714; -- Curse of Tongues Rank 1, was 4000
UPDATE `trainer_spell` SET `MoneyCost`=7000 WHERE `TrainerId`=14 AND `SpellId`=1949; -- Hellfire Rank 1, was 6000
UPDATE `trainer_spell` SET `MoneyCost`=10000 WHERE `TrainerId`=14 AND `SpellId`=2362; -- Create Spellstone Rank 1, was 9000
UPDATE `trainer_spell` SET `MoneyCost`=7000 WHERE `TrainerId`=14 AND `SpellId`=2941; -- Immolate Rank 4, was 6000
UPDATE `trainer_spell` SET `MoneyCost`=11000 WHERE `TrainerId`=14 AND `SpellId`=2970; -- Detect Invisibility Rank 2, was 10000
UPDATE `trainer_spell` SET `MoneyCost`=2500 WHERE `TrainerId`=14 AND `SpellId`=3698; -- Health Funnel Rank 2, was 2000
UPDATE `trainer_spell` SET `MoneyCost`=6000 WHERE `TrainerId`=14 AND `SpellId`=3699; -- Health Funnel Rank 3, was 5000
UPDATE `trainer_spell` SET `MoneyCost`=10000 WHERE `TrainerId`=14 AND `SpellId`=3700; -- Health Funnel Rank 4, was 9000
UPDATE `trainer_spell` SET `MoneyCost`=4800 WHERE `TrainerId`=14 AND `SpellId`=5138; -- Drain Mana Rank 1, was 3000
UPDATE `trainer_spell` SET `MoneyCost`=14000 WHERE `TrainerId`=14 AND `SpellId`=5484; -- Howl of Terror Rank 1, was 11000
UPDATE `trainer_spell` SET `MoneyCost`=4800 WHERE `TrainerId`=14 AND `SpellId`=5500; -- Sense Demons , was 3000
UPDATE `trainer_spell` SET `MoneyCost`=1700 WHERE `TrainerId`=14 AND `SpellId`=5676; -- Searing Pain Rank 1, was 1500
UPDATE `trainer_spell` SET `MoneyCost`=1600 WHERE `TrainerId`=14 AND `SpellId`=5697; -- Unending Breath , was 1200
UPDATE `trainer_spell` SET `MoneyCost`=9000 WHERE `TrainerId`=14 AND `SpellId`=5699; -- Create Healthstone Rank 3, was 8000
UPDATE `trainer_spell` SET `MoneyCost`=2500 WHERE `TrainerId`=14 AND `SpellId`=5740; -- Rain of Fire Rank 1, was 2000
UPDATE `trainer_spell` SET `MoneyCost`=3800 WHERE `TrainerId`=14 AND `SpellId`=6202; -- Create Healthstone Rank 2, was 2500
UPDATE `trainer_spell` SET `MoneyCost`=3800 WHERE `TrainerId`=14 AND `SpellId`=6205; -- Curse of Weakness Rank 3, was 2500
UPDATE `trainer_spell` SET `MoneyCost`=8000 WHERE `TrainerId`=14 AND `SpellId`=6213; -- Fear Rank 2, was 7000
UPDATE `trainer_spell` SET `MoneyCost`=25000 WHERE `TrainerId`=14 AND `SpellId`=6215; -- Fear Rank 3, was 22000
UPDATE `trainer_spell` SET `MoneyCost`=6000 WHERE `TrainerId`=14 AND `SpellId`=6217; -- Bane of Agony Rank 3, was 5000
UPDATE `trainer_spell` SET `MoneyCost`=9000 WHERE `TrainerId`=14 AND `SpellId`=6219; -- Rain of Fire Rank 2, was 8000
UPDATE `trainer_spell` SET `MoneyCost`=1000 WHERE `TrainerId`=14 AND `SpellId`=6222; -- Corruption Rank 2, was 900
UPDATE `trainer_spell` SET `MoneyCost`=4800 WHERE `TrainerId`=14 AND `SpellId`=6223; -- Corruption Rank 3, was 3000
UPDATE `trainer_spell` SET `MoneyCost`=9000 WHERE `TrainerId`=14 AND `SpellId`=6226; -- Drain Mana Rank 2, was 8000
UPDATE `trainer_spell` SET `MoneyCost`=8000 WHERE `TrainerId`=14 AND `SpellId`=6229; -- Shadow Ward Rank 1, was 7000
UPDATE `trainer_spell` SET `MoneyCost`=21000 WHERE `TrainerId`=14 AND `SpellId`=6353; -- Soul Fire Rank 1, was 14000
UPDATE `trainer_spell` SET `MoneyCost`=6000 WHERE `TrainerId`=14 AND `SpellId`=6366; -- Create Firestone Rank 1, was 5000
UPDATE `trainer_spell` SET `MoneyCost`=15000 WHERE `TrainerId`=14 AND `SpellId`=6789; -- Death Coil Rank 1, was 11000
UPDATE `trainer_spell` SET `MoneyCost`=10000 WHERE `TrainerId`=14 AND `SpellId`=7641; -- Shadow Bolt Rank 6, was 9000
UPDATE `trainer_spell` SET `MoneyCost`=8000 WHERE `TrainerId`=14 AND `SpellId`=7646; -- Curse of Weakness Rank 4, was 7000
UPDATE `trainer_spell` SET `MoneyCost`=9000 WHERE `TrainerId`=14 AND `SpellId`=7648; -- Corruption Rank 4, was 8000
UPDATE `trainer_spell` SET `MoneyCost`=11000 WHERE `TrainerId`=14 AND `SpellId`=7651; -- Drain Life Rank 4, was 10000
UPDATE `trainer_spell` SET `MoneyCost`=6000 WHERE `TrainerId`=14 AND `SpellId`=7658; -- Curse of Recklessness Rank 2, was 5000
UPDATE `trainer_spell` SET `MoneyCost`=15000 WHERE `TrainerId`=14 AND `SpellId`=7659; -- Curse of Recklessness Rank 3, was 11000
UPDATE `trainer_spell` SET `MoneyCost`=4800 WHERE `TrainerId`=14 AND `SpellId`=8288; -- Drain Soul Rank 2, was 3000
UPDATE `trainer_spell` SET `MoneyCost`=11000 WHERE `TrainerId`=14 AND `SpellId`=8289; -- Drain Soul Rank 3, was 10000
UPDATE `trainer_spell` SET `MoneyCost`=16000 WHERE `TrainerId`=14 AND `SpellId`=11659; -- Shadow Bolt Rank 7, was 12000
UPDATE `trainer_spell` SET `MoneyCost`=23000 WHERE `TrainerId`=14 AND `SpellId`=11660; -- Shadow Bolt Rank 8, was 18000
UPDATE `trainer_spell` SET `MoneyCost`=27000 WHERE `TrainerId`=14 AND `SpellId`=11661; -- Shadow Bolt Rank 9, was 26000
UPDATE `trainer_spell` SET `MoneyCost`=14000 WHERE `TrainerId`=14 AND `SpellId`=11665; -- Immolate Rank 5, was 11000
UPDATE `trainer_spell` SET `MoneyCost`=22000 WHERE `TrainerId`=14 AND `SpellId`=11667; -- Immolate Rank 6, was 15000
UPDATE `trainer_spell` SET `MoneyCost`=27000 WHERE `TrainerId`=14 AND `SpellId`=11668; -- Immolate Rank 7, was 26000
UPDATE `trainer_spell` SET `MoneyCost`=16000 WHERE `TrainerId`=14 AND `SpellId`=11671; -- Corruption Rank 5, was 12000
UPDATE `trainer_spell` SET `MoneyCost`=24000 WHERE `TrainerId`=14 AND `SpellId`=11672; -- Corruption Rank 6, was 20000
UPDATE `trainer_spell` SET `MoneyCost`=23000 WHERE `TrainerId`=14 AND `SpellId`=11675; -- Drain Soul Rank 4, was 18000
UPDATE `trainer_spell` SET `MoneyCost`=20000 WHERE `TrainerId`=14 AND `SpellId`=11677; -- Rain of Fire Rank 3, was 13000
UPDATE `trainer_spell` SET `MoneyCost`=26000 WHERE `TrainerId`=14 AND `SpellId`=11678; -- Rain of Fire Rank 4, was 24000
UPDATE `trainer_spell` SET `MoneyCost`=15000 WHERE `TrainerId`=14 AND `SpellId`=11683; -- Hellfire Rank 2, was 11000
UPDATE `trainer_spell` SET `MoneyCost`=24000 WHERE `TrainerId`=14 AND `SpellId`=11684; -- Hellfire Rank 3, was 20000
UPDATE `trainer_spell` SET `MoneyCost`=10000 WHERE `TrainerId`=14 AND `SpellId`=11687; -- Life Tap Rank 4, was 9000
UPDATE `trainer_spell` SET `MoneyCost`=20000 WHERE `TrainerId`=14 AND `SpellId`=11688; -- Life Tap Rank 5, was 13000
UPDATE `trainer_spell` SET `MoneyCost`=25000 WHERE `TrainerId`=14 AND `SpellId`=11689; -- Life Tap Rank 6, was 22000
UPDATE `trainer_spell` SET `MoneyCost`=16000 WHERE `TrainerId`=14 AND `SpellId`=11693; -- Health Funnel Rank 5, was 12000
UPDATE `trainer_spell` SET `MoneyCost`=23000 WHERE `TrainerId`=14 AND `SpellId`=11694; -- Health Funnel Rank 6, was 18000
UPDATE `trainer_spell` SET `MoneyCost`=27000 WHERE `TrainerId`=14 AND `SpellId`=11695; -- Health Funnel Rank 7, was 26000
UPDATE `trainer_spell` SET `MoneyCost`=20000 WHERE `TrainerId`=14 AND `SpellId`=11699; -- Drain Life Rank 5, was 13000
UPDATE `trainer_spell` SET `MoneyCost`=24000 WHERE `TrainerId`=14 AND `SpellId`=11700; -- Drain Life Rank 6, was 20000
UPDATE `trainer_spell` SET `MoneyCost`=16000 WHERE `TrainerId`=14 AND `SpellId`=11703; -- Drain Mana Rank 3, was 12000
UPDATE `trainer_spell` SET `MoneyCost`=24000 WHERE `TrainerId`=14 AND `SpellId`=11704; -- Drain Mana Rank 4, was 20000
UPDATE `trainer_spell` SET `MoneyCost`=15000 WHERE `TrainerId`=14 AND `SpellId`=11707; -- Curse of Weakness Rank 5, was 11000
UPDATE `trainer_spell` SET `MoneyCost`=23000 WHERE `TrainerId`=14 AND `SpellId`=11708; -- Curse of Weakness Rank 6, was 18000
UPDATE `trainer_spell` SET `MoneyCost`=11000 WHERE `TrainerId`=14 AND `SpellId`=11711; -- Bane of Agony Rank 4, was 10000
UPDATE `trainer_spell` SET `MoneyCost`=21000 WHERE `TrainerId`=14 AND `SpellId`=11712; -- Bane of Agony Rank 5, was 14000
UPDATE `trainer_spell` SET `MoneyCost`=26000 WHERE `TrainerId`=14 AND `SpellId`=11713; -- Bane of Agony Rank 6, was 24000
UPDATE `trainer_spell` SET `MoneyCost`=25000 WHERE `TrainerId`=14 AND `SpellId`=11717; -- Curse of Recklessness Rank 4, was 22000
UPDATE `trainer_spell` SET `MoneyCost`=22000 WHERE `TrainerId`=14 AND `SpellId`=11719; -- Curse of Tongues Rank 2, was 15000
UPDATE `trainer_spell` SET `MoneyCost`=16000 WHERE `TrainerId`=14 AND `SpellId`=11725; -- Subjugate Demon Rank 2, was 12000
UPDATE `trainer_spell` SET `MoneyCost`=26000 WHERE `TrainerId`=14 AND `SpellId`=11726; -- Subjugate Demon Rank 3, was 24000
UPDATE `trainer_spell` SET `MoneyCost`=20000 WHERE `TrainerId`=14 AND `SpellId`=11729; -- Create Healthstone Rank 4, was 13000
UPDATE `trainer_spell` SET `MoneyCost`=26000 WHERE `TrainerId`=14 AND `SpellId`=11730; -- Create Healthstone Rank 5, was 24000
UPDATE `trainer_spell` SET `MoneyCost`=14000 WHERE `TrainerId`=14 AND `SpellId`=11733; -- Demon Armor Rank 3, was 11000
UPDATE `trainer_spell` SET `MoneyCost`=22000 WHERE `TrainerId`=14 AND `SpellId`=11734; -- Demon Armor Rank 4, was 15000
UPDATE `trainer_spell` SET `MoneyCost`=27000 WHERE `TrainerId`=14 AND `SpellId`=11735; -- Demon Armor Rank 5, was 26000
UPDATE `trainer_spell` SET `MoneyCost`=15000 WHERE `TrainerId`=14 AND `SpellId`=11739; -- Shadow Ward Rank 2, was 11000
UPDATE `trainer_spell` SET `MoneyCost`=23000 WHERE `TrainerId`=14 AND `SpellId`=11740; -- Shadow Ward Rank 3, was 18000
UPDATE `trainer_spell` SET `MoneyCost`=22000 WHERE `TrainerId`=14 AND `SpellId`=11743; -- Detect Invisibility Rank 3, was 15000
UPDATE `trainer_spell` SET `MoneyCost`=21000 WHERE `TrainerId`=14 AND `SpellId`=17727; -- Create Spellstone Rank 2, was 14000
UPDATE `trainer_spell` SET `MoneyCost`=27000 WHERE `TrainerId`=14 AND `SpellId`=17728; -- Create Spellstone Rank 3, was 26000
UPDATE `trainer_spell` SET `MoneyCost`=5000 WHERE `TrainerId`=14 AND `SpellId`=17919; -- Searing Pain Rank 2, was 4000
UPDATE `trainer_spell` SET `MoneyCost`=9000 WHERE `TrainerId`=14 AND `SpellId`=17920; -- Searing Pain Rank 3, was 8000
UPDATE `trainer_spell` SET `MoneyCost`=15000 WHERE `TrainerId`=14 AND `SpellId`=17921; -- Searing Pain Rank 4, was 11000
UPDATE `trainer_spell` SET `MoneyCost`=22000 WHERE `TrainerId`=14 AND `SpellId`=17922; -- Searing Pain Rank 5, was 15000
UPDATE `trainer_spell` SET `MoneyCost`=26000 WHERE `TrainerId`=14 AND `SpellId`=17923; -- Searing Pain Rank 6, was 24000
UPDATE `trainer_spell` SET `MoneyCost`=25000 WHERE `TrainerId`=14 AND `SpellId`=17924; -- Soul Fire Rank 2, was 22000
UPDATE `trainer_spell` SET `MoneyCost`=22000 WHERE `TrainerId`=14 AND `SpellId`=17925; -- Death Coil Rank 2, was 15000
UPDATE `trainer_spell` SET `MoneyCost`=26000 WHERE `TrainerId`=14 AND `SpellId`=17926; -- Death Coil Rank 3, was 24000
UPDATE `trainer_spell` SET `MoneyCost`=24000 WHERE `TrainerId`=14 AND `SpellId`=17928; -- Howl of Terror Rank 2, was 20000
UPDATE `trainer_spell` SET `MoneyCost`=10000 WHERE `TrainerId`=14 AND `SpellId`=17951; -- Create Firestone Rank 2, was 9000
UPDATE `trainer_spell` SET `MoneyCost`=20000 WHERE `TrainerId`=14 AND `SpellId`=17952; -- Create Firestone Rank 3, was 13000
UPDATE `trainer_spell` SET `MoneyCost`=25000 WHERE `TrainerId`=14 AND `SpellId`=17953; -- Create Firestone Rank 4, was 22000
UPDATE `trainer_spell` SET `MoneyCost`=14000 WHERE `TrainerId`=14 AND `SpellId`=17962; -- Conflagrate Rank 3, was 500
UPDATE `trainer_spell` SET `MoneyCost`=21000 WHERE `TrainerId`=14 AND `SpellId`=18647; -- Banish Rank 2, was 14000
UPDATE `trainer_spell` SET `MoneyCost`=4800 WHERE `TrainerId`=14 AND `SpellId`=18867; -- Shadowburn Rank 2, was 150
UPDATE `trainer_spell` SET `MoneyCost`=8000 WHERE `TrainerId`=14 AND `SpellId`=18868; -- Shadowburn Rank 3, was 350
UPDATE `trainer_spell` SET `MoneyCost`=14000 WHERE `TrainerId`=14 AND `SpellId`=18869; -- Shadowburn Rank 4, was 550
UPDATE `trainer_spell` SET `MoneyCost`=21000 WHERE `TrainerId`=14 AND `SpellId`=18870; -- Shadowburn Rank 5, was 700
UPDATE `trainer_spell` SET `MoneyCost`=25000 WHERE `TrainerId`=14 AND `SpellId`=18871; -- Shadowburn Rank 6, was 1100
UPDATE `trainer_spell` SET `MoneyCost`=11000 WHERE `TrainerId`=14 AND `SpellId`=18879; -- Siphon Life Rank 2, was 1000
UPDATE `trainer_spell` SET `MoneyCost`=21000 WHERE `TrainerId`=14 AND `SpellId`=18880; -- Siphon Life Rank 3, was 1400
UPDATE `trainer_spell` SET `MoneyCost`=26000 WHERE `TrainerId`=14 AND `SpellId`=18881; -- Siphon Life Rank 4, was 2400
UPDATE `trainer_spell` SET `MoneyCost`=21000 WHERE `TrainerId`=14 AND `SpellId`=18930; -- Conflagrate Rank 4, was 700
UPDATE `trainer_spell` SET `MoneyCost`=24000 WHERE `TrainerId`=14 AND `SpellId`=18931; -- Conflagrate Rank 5, was 1000
UPDATE `trainer_spell` SET `MoneyCost`=27000 WHERE `TrainerId`=14 AND `SpellId`=18932; -- Conflagrate Rank 6, was 1300
UPDATE `trainer_spell` SET `MoneyCost`=7000 WHERE `TrainerId`=14 AND `SpellId`=20752; -- Create Soulstone Rank 2, was 6000
UPDATE `trainer_spell` SET `MoneyCost`=14000 WHERE `TrainerId`=14 AND `SpellId`=20755; -- Create Soulstone Rank 3, was 11000
UPDATE `trainer_spell` SET `MoneyCost`=22000 WHERE `TrainerId`=14 AND `SpellId`=20756; -- Create Soulstone Rank 4, was 15000
UPDATE `trainer_spell` SET `MoneyCost`=27000 WHERE `TrainerId`=14 AND `SpellId`=20757; -- Create Soulstone Rank 5, was 26000
UPDATE `trainer_spell` SET `MoneyCost`=2500 WHERE `TrainerId`=14 AND `SpellId`=440892; -- Curse of the Elements Rank 1, was 2000
UPDATE `trainer_spell` SET `MoneyCost`=22000 WHERE `TrainerId`=14 AND `SpellId`=1293812; -- Incinerate Rank 2, was 900
UPDATE `trainer_spell` SET `MoneyCost`=27000 WHERE `TrainerId`=14 AND `SpellId`=1293813; -- Incinerate Rank 3, was 1100
UPDATE `trainer_spell` SET `MoneyCost`=8000 WHERE `TrainerId`=14 AND `SpellId`=1293818; -- Conflagrate Rank 2, was 300
UPDATE `trainer_spell` SET `MoneyCost`=7000 WHERE `TrainerId`=14 AND `SpellId`=1311676; -- Curse of the Elements Rank 2, was 6000
UPDATE `trainer_spell` SET `MoneyCost`=14000 WHERE `TrainerId`=14 AND `SpellId`=1311677; -- Curse of the Elements Rank 3, was 11000
UPDATE `trainer_spell` SET `MoneyCost`=22000 WHERE `TrainerId`=14 AND `SpellId`=1311680; -- Curse of the Elements Rank 4, was 15000

-- trainer 17 (Loganaar, Turak Runetotem): 12 spells
UPDATE `trainer_spell` SET `MoneyCost`=2400 WHERE `TrainerId`=17 AND `SpellId`=5221; -- Shred Rank 1, was 3300
UPDATE `trainer_spell` SET `MoneyCost`=11000 WHERE `TrainerId`=17 AND `SpellId`=6787; -- Ravage Rank 2, was 13000
UPDATE `trainer_spell` SET `MoneyCost`=11000 WHERE `TrainerId`=17 AND `SpellId`=8951; -- Starfire Rank 4, was 13000
UPDATE `trainer_spell` SET `MoneyCost`=11000 WHERE `TrainerId`=17 AND `SpellId`=9745; -- Maul Rank 5, was 13000
UPDATE `trainer_spell` SET `MoneyCost`=11000 WHERE `TrainerId`=17 AND `SpellId`=9747; -- Demoralizing Roar Rank 4, was 13000
UPDATE `trainer_spell` SET `MoneyCost`=11000 WHERE `TrainerId`=17 AND `SpellId`=9749; -- Faerie Fire Rank 3, was 13000
UPDATE `trainer_spell` SET `MoneyCost`=11000 WHERE `TrainerId`=17 AND `SpellId`=9750; -- Regrowth Rank 6, was 13000
UPDATE `trainer_spell` SET `MoneyCost`=11000 WHERE `TrainerId`=17 AND `SpellId`=414644; -- Lacerate Rank 3, was 13000
UPDATE `trainer_spell` SET `MoneyCost`=18000 WHERE `TrainerId`=17 AND `SpellId`=1238069; -- Primal Bite Rank 2, was 8000
UPDATE `trainer_spell` SET `MoneyCost`=28000 WHERE `TrainerId`=17 AND `SpellId`=1238070; -- Primal Bite Rank 3, was 17000
UPDATE `trainer_spell` SET `MoneyCost`=7000 WHERE `TrainerId`=17 AND `SpellId`=1328616; -- Lacerate Rank 2, was 9000
UPDATE `trainer_spell` SET `MoneyCost`=4300 WHERE `TrainerId`=17 AND `SpellId`=1328618; -- Ferocious Bite Rank 1, was 4000

-- trainer 20 (Thotar): 1 spells
UPDATE `trainer_spell` SET `MoneyCost`=42000 WHERE `TrainerId`=20 AND `SpellId`=1299447; -- Aspect of the Beast Rank 4, was 39000

-- trainer 1001300 (Lawrence Schneider): 23 spells
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1001300 AND `SpellId`=2385; -- Brown Linen Vest , was 45
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1001300 AND `SpellId`=2386; -- Linen Boots , was 180
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1001300 AND `SpellId`=2392; -- Red Linen Shirt , was 45
UPDATE `trainer_spell` SET `MoneyCost`=25 WHERE `TrainerId`=1001300 AND `SpellId`=2393; -- White Linen Shirt , was 22
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1001300 AND `SpellId`=2394; -- Blue Linen Shirt , was 45
UPDATE `trainer_spell` SET `MoneyCost`=300 WHERE `TrainerId`=1001300 AND `SpellId`=2395; -- Barbaric Linen Vest , was 270
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1001300 AND `SpellId`=2396; -- Green Linen Shirt , was 180
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1001300 AND `SpellId`=2397; -- Reinforced Linen Cape , was 180
UPDATE `trainer_spell` SET `MoneyCost`=250 WHERE `TrainerId`=1001300 AND `SpellId`=2402; -- Woolen Cape , was 225
UPDATE `trainer_spell` SET `MoneyCost`=100 WHERE `TrainerId`=1001300 AND `SpellId`=2964; -- Bolt of Woolen Cloth , was 90
UPDATE `trainer_spell` SET `MoneyCost`=100 WHERE `TrainerId`=1001300 AND `SpellId`=3755; -- Linen Bag , was 90
UPDATE `trainer_spell` SET `MoneyCost`=100 WHERE `TrainerId`=1001300 AND `SpellId`=3840; -- Heavy Linen Gloves , was 90
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1001300 AND `SpellId`=3841; -- Green Linen Bracers , was 180
UPDATE `trainer_spell` SET `MoneyCost`=300 WHERE `TrainerId`=1001300 AND `SpellId`=3842; -- Handstitched Linen Britches , was 270
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1001300 AND `SpellId`=3914; -- Brown Linen Pants , was 45
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1001300 AND `SpellId`=7623; -- Brown Linen Robe , was 45
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1001300 AND `SpellId`=7624; -- White Linen Robe , was 45
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1001300 AND `SpellId`=8465; -- Simple Dress , was 45
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1001300 AND `SpellId`=8776; -- Linen Belt , was 45
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1001300 AND `SpellId`=12045; -- Simple Linen Boots , was 45
UPDATE `trainer_spell` SET `MoneyCost`=300 WHERE `TrainerId`=1001300 AND `SpellId`=12046; -- Simple Kilt , was 270
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1001300 AND `SpellId`=1257368; -- Novice Arcanist's Sash , was 526
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1001300 AND `SpellId`=1257369; -- Novice Ardent's Sash , was 526

-- trainer 1001632 (Adele Fielder): 2 spells
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1001632 AND `SpellId`=3759; -- Embossed Leather Pants , was 500
UPDATE `trainer_spell` SET `MoneyCost`=150 WHERE `TrainerId`=1001632 AND `SpellId`=9065; -- Light Leather Bracers , was 450

-- trainer 1003363 (Magar): 43 spells
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1003363 AND `SpellId`=2385; -- Brown Linen Vest , was 45
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1003363 AND `SpellId`=2386; -- Linen Boots , was 180
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1003363 AND `SpellId`=2392; -- Red Linen Shirt , was 45
UPDATE `trainer_spell` SET `MoneyCost`=25 WHERE `TrainerId`=1003363 AND `SpellId`=2393; -- White Linen Shirt , was 22
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1003363 AND `SpellId`=2394; -- Blue Linen Shirt , was 45
UPDATE `trainer_spell` SET `MoneyCost`=300 WHERE `TrainerId`=1003363 AND `SpellId`=2395; -- Barbaric Linen Vest , was 270
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1003363 AND `SpellId`=2396; -- Green Linen Shirt , was 180
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1003363 AND `SpellId`=2397; -- Reinforced Linen Cape , was 180
UPDATE `trainer_spell` SET `MoneyCost`=300 WHERE `TrainerId`=1003363 AND `SpellId`=2399; -- Green Woolen Vest , was 270
UPDATE `trainer_spell` SET `MoneyCost`=300 WHERE `TrainerId`=1003363 AND `SpellId`=2401; -- Woolen Boots , was 270
UPDATE `trainer_spell` SET `MoneyCost`=250 WHERE `TrainerId`=1003363 AND `SpellId`=2402; -- Woolen Cape , was 225
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1003363 AND `SpellId`=2406; -- Gray Woolen Shirt , was 180
UPDATE `trainer_spell` SET `MoneyCost`=100 WHERE `TrainerId`=1003363 AND `SpellId`=2964; -- Bolt of Woolen Cloth , was 90
UPDATE `trainer_spell` SET `MoneyCost`=100 WHERE `TrainerId`=1003363 AND `SpellId`=3755; -- Linen Bag , was 90
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1003363 AND `SpellId`=3757; -- Woolen Bag , was 180
UPDATE `trainer_spell` SET `MoneyCost`=800 WHERE `TrainerId`=1003363 AND `SpellId`=3813; -- Small Silk Pack , was 720
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1003363 AND `SpellId`=3839; -- Bolt of Silk Cloth , was 450
UPDATE `trainer_spell` SET `MoneyCost`=100 WHERE `TrainerId`=1003363 AND `SpellId`=3840; -- Heavy Linen Gloves , was 90
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1003363 AND `SpellId`=3841; -- Green Linen Bracers , was 180
UPDATE `trainer_spell` SET `MoneyCost`=300 WHERE `TrainerId`=1003363 AND `SpellId`=3842; -- Handstitched Linen Britches , was 270
UPDATE `trainer_spell` SET `MoneyCost`=400 WHERE `TrainerId`=1003363 AND `SpellId`=3843; -- Heavy Woolen Gloves , was 360
UPDATE `trainer_spell` SET `MoneyCost`=300 WHERE `TrainerId`=1003363 AND `SpellId`=3845; -- Soft-soled Linen Boots , was 270
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1003363 AND `SpellId`=3848; -- Double-Stitched Woolen Shoulders , was 450
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1003363 AND `SpellId`=3850; -- Heavy Woolen Pants , was 450
UPDATE `trainer_spell` SET `MoneyCost`=750 WHERE `TrainerId`=1003363 AND `SpellId`=3852; -- Gloves of Meditation , was 675
UPDATE `trainer_spell` SET `MoneyCost`=750 WHERE `TrainerId`=1003363 AND `SpellId`=3855; -- Spidersilk Boots , was 675
UPDATE `trainer_spell` SET `MoneyCost`=750 WHERE `TrainerId`=1003363 AND `SpellId`=3859; -- Azure Silk Vest , was 675
UPDATE `trainer_spell` SET `MoneyCost`=250 WHERE `TrainerId`=1003363 AND `SpellId`=3866; -- Stylish Red Shirt , was 225
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1003363 AND `SpellId`=3909; -- Journeyman Tailoring , was 450
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1003363 AND `SpellId`=3914; -- Brown Linen Pants , was 45
UPDATE `trainer_spell` SET `MoneyCost`=400 WHERE `TrainerId`=1003363 AND `SpellId`=6521; -- Pearl-clasped Cloak , was 360
UPDATE `trainer_spell` SET `MoneyCost`=1000 WHERE `TrainerId`=1003363 AND `SpellId`=6690; -- Lesser Wizard's Robe , was 900
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1003363 AND `SpellId`=7623; -- Brown Linen Robe , was 45
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1003363 AND `SpellId`=7624; -- White Linen Robe , was 45
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1003363 AND `SpellId`=8465; -- Simple Dress , was 45
UPDATE `trainer_spell` SET `MoneyCost`=250 WHERE `TrainerId`=1003363 AND `SpellId`=8467; -- White Woolen Dress , was 225
UPDATE `trainer_spell` SET `MoneyCost`=600 WHERE `TrainerId`=1003363 AND `SpellId`=8758; -- Azure Silk Pants , was 540
UPDATE `trainer_spell` SET `MoneyCost`=600 WHERE `TrainerId`=1003363 AND `SpellId`=8760; -- Azure Silk Hood , was 540
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1003363 AND `SpellId`=8776; -- Linen Belt , was 45
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1003363 AND `SpellId`=12045; -- Simple Linen Boots , was 45
UPDATE `trainer_spell` SET `MoneyCost`=300 WHERE `TrainerId`=1003363 AND `SpellId`=12046; -- Simple Kilt , was 270
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1003363 AND `SpellId`=1257368; -- Novice Arcanist's Sash , was 526
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1003363 AND `SpellId`=1257369; -- Novice Ardent's Sash , was 526

-- trainer 1003365 (Karolek): 24 spells
UPDATE `trainer_spell` SET `MoneyCost`=250 WHERE `TrainerId`=1003365 AND `SpellId`=2159; -- Fine Leather Cloak , was 550
UPDATE `trainer_spell` SET `MoneyCost`=250 WHERE `TrainerId`=1003365 AND `SpellId`=2165; -- Medium Armor Kit , was 650
UPDATE `trainer_spell` SET `MoneyCost`=450 WHERE `TrainerId`=1003365 AND `SpellId`=2166; -- Toughened Leather Armor , was 1400
UPDATE `trainer_spell` SET `MoneyCost`=350 WHERE `TrainerId`=1003365 AND `SpellId`=2167; -- Gray Leather Boots , was 650
UPDATE `trainer_spell` SET `MoneyCost`=350 WHERE `TrainerId`=1003365 AND `SpellId`=2168; -- Dark Leather Cloak , was 1000
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1003365 AND `SpellId`=3759; -- Embossed Leather Pants , was 500
UPDATE `trainer_spell` SET `MoneyCost`=600 WHERE `TrainerId`=1003365 AND `SpellId`=3760; -- Hillman's Cloak , was 2000
UPDATE `trainer_spell` SET `MoneyCost`=350 WHERE `TrainerId`=1003365 AND `SpellId`=3761; -- Fine Leather Tunic , was 550
UPDATE `trainer_spell` SET `MoneyCost`=300 WHERE `TrainerId`=1003365 AND `SpellId`=3763; -- Fine Leather Belt , was 500
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1003365 AND `SpellId`=3764; -- Hillman's Leather Gloves , was 1800
UPDATE `trainer_spell` SET `MoneyCost`=400 WHERE `TrainerId`=1003365 AND `SpellId`=3766; -- Dark Leather Belt , was 1500
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1003365 AND `SpellId`=3768; -- Hillman's Shoulders , was 1200
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1003365 AND `SpellId`=3770; -- Toughened Leather Gloves , was 1500
UPDATE `trainer_spell` SET `MoneyCost`=750 WHERE `TrainerId`=1003365 AND `SpellId`=3780; -- Heavy Armor Kit , was 2000
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1003365 AND `SpellId`=3817; -- Cured Medium Hide , was 650
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1003365 AND `SpellId`=3818; -- Cured Heavy Hide , was 1800
UPDATE `trainer_spell` SET `MoneyCost`=400 WHERE `TrainerId`=1003365 AND `SpellId`=7135; -- Dark Leather Pants , was 1000
UPDATE `trainer_spell` SET `MoneyCost`=150 WHERE `TrainerId`=1003365 AND `SpellId`=9065; -- Light Leather Bracers , was 450
UPDATE `trainer_spell` SET `MoneyCost`=400 WHERE `TrainerId`=1003365 AND `SpellId`=9068; -- Light Leather Pants , was 600
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1003365 AND `SpellId`=9074; -- Nimble Leather Gloves , was 1400
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1003365 AND `SpellId`=9145; -- Fletcher's Gloves , was 1500
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1003365 AND `SpellId`=9193; -- Heavy Quiver , was 2000
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1003365 AND `SpellId`=9194; -- Heavy Leather Ammo Pouch , was 2000
UPDATE `trainer_spell` SET `MoneyCost`=1000 WHERE `TrainerId`=1003365 AND `SpellId`=20649; -- Heavy Leather , was 1800

-- trainer 1003412 (Nogg): 22 spells
UPDATE `trainer_spell` SET `MoneyCost`=300 WHERE `TrainerId`=1003412 AND `SpellId`=3932; -- Target Dummy , was 270
UPDATE `trainer_spell` SET `MoneyCost`=400 WHERE `TrainerId`=1003412 AND `SpellId`=3934; -- Flying Tiger Goggles , was 360
UPDATE `trainer_spell` SET `MoneyCost`=420 WHERE `TrainerId`=1003412 AND `SpellId`=3936; -- Deadly Blunderbuss , was 378
UPDATE `trainer_spell` SET `MoneyCost`=450 WHERE `TrainerId`=1003412 AND `SpellId`=3937; -- Large Copper Bomb , was 405
UPDATE `trainer_spell` SET `MoneyCost`=450 WHERE `TrainerId`=1003412 AND `SpellId`=3938; -- Bronze Tube , was 405
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1003412 AND `SpellId`=3941; -- Small Bronze Bomb , was 450
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1003412 AND `SpellId`=3942; -- Whirring Bronze Gizmo , was 450
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1003412 AND `SpellId`=3945; -- Heavy Blasting Powder , was 450
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1003412 AND `SpellId`=3946; -- Heavy Dynamite , was 450
UPDATE `trainer_spell` SET `MoneyCost`=300 WHERE `TrainerId`=1003412 AND `SpellId`=3947; -- Crafted Solid Shot , was 270
UPDATE `trainer_spell` SET `MoneyCost`=550 WHERE `TrainerId`=1003412 AND `SpellId`=3949; -- Silver-plated Shotgun , was 495
UPDATE `trainer_spell` SET `MoneyCost`=600 WHERE `TrainerId`=1003412 AND `SpellId`=3950; -- Big Bronze Bomb , was 540
UPDATE `trainer_spell` SET `MoneyCost`=600 WHERE `TrainerId`=1003412 AND `SpellId`=3953; -- Bronze Framework , was 540
UPDATE `trainer_spell` SET `MoneyCost`=750 WHERE `TrainerId`=1003412 AND `SpellId`=3955; -- Explosive Sheep , was 675
UPDATE `trainer_spell` SET `MoneyCost`=750 WHERE `TrainerId`=1003412 AND `SpellId`=3956; -- Green Tinted Goggles , was 675
UPDATE `trainer_spell` SET `MoneyCost`=300 WHERE `TrainerId`=1003412 AND `SpellId`=3973; -- Silver Contact , was 270
UPDATE `trainer_spell` SET `MoneyCost`=475 WHERE `TrainerId`=1003412 AND `SpellId`=3978; -- Standard Scope , was 427
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1003412 AND `SpellId`=4037; -- Journeyman Engineer , was 450
UPDATE `trainer_spell` SET `MoneyCost`=400 WHERE `TrainerId`=1003412 AND `SpellId`=6458; -- Ornate Spyglass , was 360
UPDATE `trainer_spell` SET `MoneyCost`=300 WHERE `TrainerId`=1003412 AND `SpellId`=8334; -- Practice Lock , was 270
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1003412 AND `SpellId`=9271; -- Aquadynamic Fish Attractor , was 450
UPDATE `trainer_spell` SET `MoneyCost`=1000 WHERE `TrainerId`=1003412 AND `SpellId`=12584; -- Gold Power Core , was 900

-- trainer 1004598 (Brom Killian): 8 spells
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1004598 AND `SpellId`=2658; -- Silver Bar , was 180
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1004598 AND `SpellId`=3307; -- Smelt Iron , was 450
UPDATE `trainer_spell` SET `MoneyCost`=2500 WHERE `TrainerId`=1004598 AND `SpellId`=3308; -- Gold Bar , was 2250
UPDATE `trainer_spell` SET `MoneyCost`=5000 WHERE `TrainerId`=1004598 AND `SpellId`=3564; -- Expert Mining , was 4500
UPDATE `trainer_spell` SET `MoneyCost`=2500 WHERE `TrainerId`=1004598 AND `SpellId`=3569; -- Steel Bar , was 2250
UPDATE `trainer_spell` SET `MoneyCost`=5000 WHERE `TrainerId`=1004598 AND `SpellId`=10097; -- Mithril Bar , was 4500
UPDATE `trainer_spell` SET `MoneyCost`=10000 WHERE `TrainerId`=1004598 AND `SpellId`=10098; -- Truesilver Bar , was 9000
UPDATE `trainer_spell` SET `MoneyCost`=20000 WHERE `TrainerId`=1004598 AND `SpellId`=16153; -- Thorium Bar , was 18000

-- trainer 1005564 (Simon Tanner): 24 spells
UPDATE `trainer_spell` SET `MoneyCost`=250 WHERE `TrainerId`=1005564 AND `SpellId`=2159; -- Fine Leather Cloak , was 550
UPDATE `trainer_spell` SET `MoneyCost`=250 WHERE `TrainerId`=1005564 AND `SpellId`=2165; -- Medium Armor Kit , was 650
UPDATE `trainer_spell` SET `MoneyCost`=450 WHERE `TrainerId`=1005564 AND `SpellId`=2166; -- Toughened Leather Armor , was 1400
UPDATE `trainer_spell` SET `MoneyCost`=350 WHERE `TrainerId`=1005564 AND `SpellId`=2167; -- Gray Leather Boots , was 650
UPDATE `trainer_spell` SET `MoneyCost`=350 WHERE `TrainerId`=1005564 AND `SpellId`=2168; -- Dark Leather Cloak , was 1000
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1005564 AND `SpellId`=3759; -- Embossed Leather Pants , was 500
UPDATE `trainer_spell` SET `MoneyCost`=600 WHERE `TrainerId`=1005564 AND `SpellId`=3760; -- Hillman's Cloak , was 2000
UPDATE `trainer_spell` SET `MoneyCost`=350 WHERE `TrainerId`=1005564 AND `SpellId`=3761; -- Fine Leather Tunic , was 550
UPDATE `trainer_spell` SET `MoneyCost`=300 WHERE `TrainerId`=1005564 AND `SpellId`=3763; -- Fine Leather Belt , was 500
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1005564 AND `SpellId`=3764; -- Hillman's Leather Gloves , was 1800
UPDATE `trainer_spell` SET `MoneyCost`=400 WHERE `TrainerId`=1005564 AND `SpellId`=3766; -- Dark Leather Belt , was 1500
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1005564 AND `SpellId`=3768; -- Hillman's Shoulders , was 1200
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1005564 AND `SpellId`=3770; -- Toughened Leather Gloves , was 1500
UPDATE `trainer_spell` SET `MoneyCost`=750 WHERE `TrainerId`=1005564 AND `SpellId`=3780; -- Heavy Armor Kit , was 2000
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1005564 AND `SpellId`=3817; -- Cured Medium Hide , was 650
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1005564 AND `SpellId`=3818; -- Cured Heavy Hide , was 1800
UPDATE `trainer_spell` SET `MoneyCost`=400 WHERE `TrainerId`=1005564 AND `SpellId`=7135; -- Dark Leather Pants , was 1000
UPDATE `trainer_spell` SET `MoneyCost`=150 WHERE `TrainerId`=1005564 AND `SpellId`=9065; -- Light Leather Bracers , was 450
UPDATE `trainer_spell` SET `MoneyCost`=400 WHERE `TrainerId`=1005564 AND `SpellId`=9068; -- Light Leather Pants , was 600
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1005564 AND `SpellId`=9074; -- Nimble Leather Gloves , was 1400
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1005564 AND `SpellId`=9145; -- Fletcher's Gloves , was 1500
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1005564 AND `SpellId`=9193; -- Heavy Quiver , was 2000
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1005564 AND `SpellId`=9194; -- Heavy Leather Ammo Pouch , was 2000
UPDATE `trainer_spell` SET `MoneyCost`=1000 WHERE `TrainerId`=1005564 AND `SpellId`=20649; -- Heavy Leather , was 1800

-- trainer 1005811 (Kamari): 2 spells
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1005811 AND `SpellId`=3759; -- Embossed Leather Pants , was 500
UPDATE `trainer_spell` SET `MoneyCost`=150 WHERE `TrainerId`=1005811 AND `SpellId`=9065; -- Light Leather Bracers , was 450

-- trainer 1011049 (Rhiannon Davis): 43 spells
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1011049 AND `SpellId`=2385; -- Brown Linen Vest , was 45
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1011049 AND `SpellId`=2386; -- Linen Boots , was 180
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1011049 AND `SpellId`=2392; -- Red Linen Shirt , was 45
UPDATE `trainer_spell` SET `MoneyCost`=25 WHERE `TrainerId`=1011049 AND `SpellId`=2393; -- White Linen Shirt , was 22
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1011049 AND `SpellId`=2394; -- Blue Linen Shirt , was 45
UPDATE `trainer_spell` SET `MoneyCost`=300 WHERE `TrainerId`=1011049 AND `SpellId`=2395; -- Barbaric Linen Vest , was 270
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1011049 AND `SpellId`=2396; -- Green Linen Shirt , was 180
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1011049 AND `SpellId`=2397; -- Reinforced Linen Cape , was 180
UPDATE `trainer_spell` SET `MoneyCost`=300 WHERE `TrainerId`=1011049 AND `SpellId`=2399; -- Green Woolen Vest , was 270
UPDATE `trainer_spell` SET `MoneyCost`=300 WHERE `TrainerId`=1011049 AND `SpellId`=2401; -- Woolen Boots , was 270
UPDATE `trainer_spell` SET `MoneyCost`=250 WHERE `TrainerId`=1011049 AND `SpellId`=2402; -- Woolen Cape , was 225
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1011049 AND `SpellId`=2406; -- Gray Woolen Shirt , was 180
UPDATE `trainer_spell` SET `MoneyCost`=100 WHERE `TrainerId`=1011049 AND `SpellId`=2964; -- Bolt of Woolen Cloth , was 90
UPDATE `trainer_spell` SET `MoneyCost`=100 WHERE `TrainerId`=1011049 AND `SpellId`=3755; -- Linen Bag , was 90
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1011049 AND `SpellId`=3757; -- Woolen Bag , was 180
UPDATE `trainer_spell` SET `MoneyCost`=800 WHERE `TrainerId`=1011049 AND `SpellId`=3813; -- Small Silk Pack , was 720
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1011049 AND `SpellId`=3839; -- Bolt of Silk Cloth , was 450
UPDATE `trainer_spell` SET `MoneyCost`=100 WHERE `TrainerId`=1011049 AND `SpellId`=3840; -- Heavy Linen Gloves , was 90
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1011049 AND `SpellId`=3841; -- Green Linen Bracers , was 180
UPDATE `trainer_spell` SET `MoneyCost`=300 WHERE `TrainerId`=1011049 AND `SpellId`=3842; -- Handstitched Linen Britches , was 270
UPDATE `trainer_spell` SET `MoneyCost`=400 WHERE `TrainerId`=1011049 AND `SpellId`=3843; -- Heavy Woolen Gloves , was 360
UPDATE `trainer_spell` SET `MoneyCost`=300 WHERE `TrainerId`=1011049 AND `SpellId`=3845; -- Soft-soled Linen Boots , was 270
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1011049 AND `SpellId`=3848; -- Double-Stitched Woolen Shoulders , was 450
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1011049 AND `SpellId`=3850; -- Heavy Woolen Pants , was 450
UPDATE `trainer_spell` SET `MoneyCost`=750 WHERE `TrainerId`=1011049 AND `SpellId`=3852; -- Gloves of Meditation , was 675
UPDATE `trainer_spell` SET `MoneyCost`=750 WHERE `TrainerId`=1011049 AND `SpellId`=3855; -- Spidersilk Boots , was 675
UPDATE `trainer_spell` SET `MoneyCost`=750 WHERE `TrainerId`=1011049 AND `SpellId`=3859; -- Azure Silk Vest , was 675
UPDATE `trainer_spell` SET `MoneyCost`=250 WHERE `TrainerId`=1011049 AND `SpellId`=3866; -- Stylish Red Shirt , was 225
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1011049 AND `SpellId`=3909; -- Journeyman Tailoring , was 450
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1011049 AND `SpellId`=3914; -- Brown Linen Pants , was 45
UPDATE `trainer_spell` SET `MoneyCost`=400 WHERE `TrainerId`=1011049 AND `SpellId`=6521; -- Pearl-clasped Cloak , was 360
UPDATE `trainer_spell` SET `MoneyCost`=1000 WHERE `TrainerId`=1011049 AND `SpellId`=6690; -- Lesser Wizard's Robe , was 900
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1011049 AND `SpellId`=7623; -- Brown Linen Robe , was 45
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1011049 AND `SpellId`=7624; -- White Linen Robe , was 45
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1011049 AND `SpellId`=8465; -- Simple Dress , was 45
UPDATE `trainer_spell` SET `MoneyCost`=250 WHERE `TrainerId`=1011049 AND `SpellId`=8467; -- White Woolen Dress , was 225
UPDATE `trainer_spell` SET `MoneyCost`=600 WHERE `TrainerId`=1011049 AND `SpellId`=8758; -- Azure Silk Pants , was 540
UPDATE `trainer_spell` SET `MoneyCost`=600 WHERE `TrainerId`=1011049 AND `SpellId`=8760; -- Azure Silk Hood , was 540
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1011049 AND `SpellId`=8776; -- Linen Belt , was 45
UPDATE `trainer_spell` SET `MoneyCost`=50 WHERE `TrainerId`=1011049 AND `SpellId`=12045; -- Simple Linen Boots , was 45
UPDATE `trainer_spell` SET `MoneyCost`=300 WHERE `TrainerId`=1011049 AND `SpellId`=12046; -- Simple Kilt , was 270
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1011049 AND `SpellId`=1257368; -- Novice Arcanist's Sash , was 526
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1011049 AND `SpellId`=1257369; -- Novice Ardent's Sash , was 526

-- trainer 1011084 (Tarn): 24 spells
UPDATE `trainer_spell` SET `MoneyCost`=250 WHERE `TrainerId`=1011084 AND `SpellId`=2159; -- Fine Leather Cloak , was 550
UPDATE `trainer_spell` SET `MoneyCost`=250 WHERE `TrainerId`=1011084 AND `SpellId`=2165; -- Medium Armor Kit , was 650
UPDATE `trainer_spell` SET `MoneyCost`=450 WHERE `TrainerId`=1011084 AND `SpellId`=2166; -- Toughened Leather Armor , was 1400
UPDATE `trainer_spell` SET `MoneyCost`=350 WHERE `TrainerId`=1011084 AND `SpellId`=2167; -- Gray Leather Boots , was 650
UPDATE `trainer_spell` SET `MoneyCost`=350 WHERE `TrainerId`=1011084 AND `SpellId`=2168; -- Dark Leather Cloak , was 1000
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1011084 AND `SpellId`=3759; -- Embossed Leather Pants , was 500
UPDATE `trainer_spell` SET `MoneyCost`=600 WHERE `TrainerId`=1011084 AND `SpellId`=3760; -- Hillman's Cloak , was 2000
UPDATE `trainer_spell` SET `MoneyCost`=350 WHERE `TrainerId`=1011084 AND `SpellId`=3761; -- Fine Leather Tunic , was 550
UPDATE `trainer_spell` SET `MoneyCost`=300 WHERE `TrainerId`=1011084 AND `SpellId`=3763; -- Fine Leather Belt , was 500
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1011084 AND `SpellId`=3764; -- Hillman's Leather Gloves , was 1800
UPDATE `trainer_spell` SET `MoneyCost`=400 WHERE `TrainerId`=1011084 AND `SpellId`=3766; -- Dark Leather Belt , was 1500
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1011084 AND `SpellId`=3768; -- Hillman's Shoulders , was 1200
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1011084 AND `SpellId`=3770; -- Toughened Leather Gloves , was 1500
UPDATE `trainer_spell` SET `MoneyCost`=750 WHERE `TrainerId`=1011084 AND `SpellId`=3780; -- Heavy Armor Kit , was 2000
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1011084 AND `SpellId`=3817; -- Cured Medium Hide , was 650
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1011084 AND `SpellId`=3818; -- Cured Heavy Hide , was 1800
UPDATE `trainer_spell` SET `MoneyCost`=400 WHERE `TrainerId`=1011084 AND `SpellId`=7135; -- Dark Leather Pants , was 1000
UPDATE `trainer_spell` SET `MoneyCost`=150 WHERE `TrainerId`=1011084 AND `SpellId`=9065; -- Light Leather Bracers , was 450
UPDATE `trainer_spell` SET `MoneyCost`=400 WHERE `TrainerId`=1011084 AND `SpellId`=9068; -- Light Leather Pants , was 600
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1011084 AND `SpellId`=9074; -- Nimble Leather Gloves , was 1400
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1011084 AND `SpellId`=9145; -- Fletcher's Gloves , was 1500
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1011084 AND `SpellId`=9193; -- Heavy Quiver , was 2000
UPDATE `trainer_spell` SET `MoneyCost`=500 WHERE `TrainerId`=1011084 AND `SpellId`=9194; -- Heavy Leather Ammo Pouch , was 2000
UPDATE `trainer_spell` SET `MoneyCost`=1000 WHERE `TrainerId`=1011084 AND `SpellId`=20649; -- Heavy Leather , was 1800

-- trainer 1011096 (Randal Worth): 2 spells
UPDATE `trainer_spell` SET `MoneyCost`=200 WHERE `TrainerId`=1011096 AND `SpellId`=3759; -- Embossed Leather Pants , was 500
UPDATE `trainer_spell` SET `MoneyCost`=150 WHERE `TrainerId`=1011096 AND `SpellId`=9065; -- Light Leather Bracers , was 450
