-- Loot: the _50 drops at Wowhead's rate were diluted (kills from before Forever added the item count too), so they get the rate
-- of comparable vanilla drops in vMaNGOS. Chance INFERRED:
--  * dungeon bosses: the vMaNGOS per-item chance of that boss's own blues (Sneed 35, Grubbis 10, Thalnos 50, Loksey 33.3, Doan 25, Agathelos 50);
--  * Crowd Pummeler's schematic: 3.8, the median of recipes that vanilla dungeon bosses drop;
--  * named rares: 45, the median of the vMaNGOS drops unique to one rare or rare elite (14 items, 2.2 to 100).
-- Drops on the Forever-only maps 2959 and 2991 (Dark Caster, Fallen Mage, Kirin Tor Necromancer, Den'dralass) keep Wowhead's rate, which has no old kills in it.
UPDATE `creature_loot_template` SET `Chance`=35, `Comment`='Sneed: Bandsaw Wristbands (chance INFERRED, boss blues)' WHERE `Entry`=643 AND `ItemType`=0 AND `Item`=273293;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Lost One Chieftain: Lost Chieftain''s Greatsword (chance INFERRED, named rare)' WHERE `Entry`=763 AND `ItemType`=0 AND `Item`=282003;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Shadowclaw: Serrated Shadowclaw (chance INFERRED, named rare)' WHERE `Entry`=2175 AND `ItemType`=0 AND `Item`=284174;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Lady Moongazer: Moongazer''s Wand (chance INFERRED, named rare)' WHERE `Entry`=2184 AND `ItemType`=0 AND `Item`=284193;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Licillin: Impcaster''s Staff (chance INFERRED, named rare)' WHERE `Entry`=2191 AND `ItemType`=0 AND `Item`=284173;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Sewer Beast: Baby Crocolisk (chance INFERRED, named rare)' WHERE `Entry`=3581 AND `ItemType`=0 AND `Item`=282047;
UPDATE `creature_loot_template` SET `Chance`=33.3, `Comment`='Houndmaster Loksey: Houndmaster Boomerang (chance INFERRED, boss blues)' WHERE `Entry`=3974 AND `ItemType`=0 AND `Item`=274292;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Vengeful Ancient: Budding Leaf Belt (chance INFERRED, named rare)' WHERE `Entry`=4030 AND `ItemType`=0 AND `Item`=284382;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Nal''taszar: Faerie Dragon''s Skin (chance INFERRED, named rare)' WHERE `Entry`=4066 AND `ItemType`=0 AND `Item`=284383;
UPDATE `creature_loot_template` SET `Chance`=50, `Comment`='Agathelos the Raging: Quilrager Throwing Axe (chance INFERRED, boss blues)' WHERE `Entry`=4422 AND `ItemType`=0 AND `Item`=274160;
UPDATE `creature_loot_template` SET `Chance`=50, `Comment`='Bloodmage Thalnos: Polished Skullcap (chance INFERRED, boss blues)' WHERE `Entry`=4543 AND `ItemType`=0 AND `Item`=274291;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Humar the Pridelord: Mark of the Pack Leader (chance INFERRED, named rare)' WHERE `Entry`=5828 AND `ItemType`=0 AND `Item`=285331;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Sister Rathtalon: Jagged Knives (chance INFERRED, named rare)' WHERE `Entry`=5830 AND `ItemType`=0 AND `Item`=285282;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Engineer Whirleygig: Jagged Knives (chance INFERRED, named rare)' WHERE `Entry`=5836 AND `ItemType`=0 AND `Item`=285282;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Stonearm: Jagged Knives (chance INFERRED, named rare)' WHERE `Entry`=5837 AND `ItemType`=0 AND `Item`=285282;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Rocklance: Jagged Knives (chance INFERRED, named rare)' WHERE `Entry`=5841 AND `ItemType`=0 AND `Item`=285282;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Swinegart Spearhide: Puncturing Spear (chance INFERRED, named rare)' WHERE `Entry`=5864 AND `ItemType`=0 AND `Item`=285332;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Foreman Rigger: Foreman''s Helm (chance INFERRED, named rare)' WHERE `Entry`=5931 AND `ItemType`=0 AND `Item`=284400;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Taskmaster Whipfang: Whipfang''s Skinsearer (chance INFERRED, named rare)' WHERE `Entry`=5932 AND `ItemType`=0 AND `Item`=284386;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Heartrazor: Wyvern Heart Band (chance INFERRED, named rare)' WHERE `Entry`=5934 AND `ItemType`=0 AND `Item`=285190;
UPDATE `creature_loot_template` SET `Chance`=3.8, `Comment`='Crowd Pummeler 9-60: Schematic: 9-60 Battery Pack (chance INFERRED, boss recipes)' WHERE `Entry`=6229 AND `ItemType`=0 AND `Item`=274049;
UPDATE `creature_loot_template` SET `Chance`=25, `Comment`='Arcanist Doan: Spellsever Crossbow (chance INFERRED, boss blues)' WHERE `Entry`=6487 AND `ItemType`=0 AND `Item`=274293;
UPDATE `creature_loot_template` SET `Chance`=25, `Comment`='Arcanist Doan: Library Walkers (chance INFERRED, boss blues)' WHERE `Entry`=6487 AND `ItemType`=0 AND `Item`=274294;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Flagglemurk the Cruel: Greymist Gauntlets (chance INFERRED, named rare)' WHERE `Entry`=7015 AND `ItemType`=0 AND `Item`=284185;
UPDATE `creature_loot_template` SET `Chance`=10, `Comment`='Grubbis: Skullduggery Belt (chance INFERRED, boss blues)' WHERE `Entry`=7361 AND `ItemType`=0 AND `Item`=274042;
UPDATE `creature_loot_template` SET `Chance`=10, `Comment`='Grubbis: Irradiated Shield (chance INFERRED, boss blues)' WHERE `Entry`=7361 AND `ItemType`=0 AND `Item`=274043;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Ambassador Bloodrage: Ambassador''s Bloodrobes (chance INFERRED, named rare)' WHERE `Entry`=7895 AND `ItemType`=0 AND `Item`=286639;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Mist Howler: Wolf Whistle (chance INFERRED, named rare)' WHERE `Entry`=10644 AND `ItemType`=0 AND `Item`=284666;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Araga: Denmother''s Hide (chance INFERRED, named rare)' WHERE `Entry`=14222 AND `ItemType`=0 AND `Item`=283253;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Shanda the Spinner: Silkspun Gloves (chance INFERRED, named rare)' WHERE `Entry`=14266 AND `ItemType`=0 AND `Item`=282636;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Lord Condar: Shaking Egg (chance INFERRED, named rare)' WHERE `Entry`=14268 AND `ItemType`=0 AND `Item`=282637;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Snarlflare: Whelpfire Wand (chance INFERRED, named rare)' WHERE `Entry`=14272 AND `ItemType`=0 AND `Item`=281925;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Tamra Stormpike: Dun Garok Rifle (chance INFERRED, named rare)' WHERE `Entry`=14275 AND `ItemType`=0 AND `Item`=282710;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Scargil: Imbued Scaled Cuffs (chance INFERRED, named rare)' WHERE `Entry`=14276 AND `ItemType`=0 AND `Item`=282716;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Ro''Bark: Gnoll Shaman''s Staff (chance INFERRED, named rare)' WHERE `Entry`=14278 AND `ItemType`=0 AND `Item`=282721;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Gnawbone: Gnawed Bone (chance INFERRED, named rare)' WHERE `Entry`=14425 AND `ItemType`=0 AND `Item`=282654;
UPDATE `creature_loot_template` SET `Chance`=45, `Comment`='Harb Foulmountain: Foulmountain Family Hammer (chance INFERRED, named rare)' WHERE `Entry`=14426 AND `ItemType`=0 AND `Item`=285212;
