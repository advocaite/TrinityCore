-- WoW Forever camping: each camp feature's buff is "mutually exclusive" with one class buff (70707 client texts of the placement
-- spells: Camp Chair 1307229 / Moonkin Aura, Lodestone 1307254 / Blessing of Might, Enchanted Lute 1307234 / Mark of the Wild,
-- Faction Banner 1307239-1307240 / Divine Spirit, First Aid Kit 1307244 / Power Word: Fortitude, Fish Bowl 1307245 / Blessing of
-- Kings, Incense Candle 1307251 / Arcane Intellect). They stacked (Azeldof, 2026-11-02). Each pair is a spell group where only the
-- highest effect counts (stack rule 4), so the camp buff, renewed every minute while sitting, never pushes out a stronger class buff.
-- Class buff ranks as in the existing groups 1400164/1400176/1400182/1400204-1400206/1400223. Safe to run again.
DELETE FROM `spell_group` WHERE `id` BETWEEN 1400900 AND 1400906;
INSERT INTO `spell_group` (`id`,`spell_id`) VALUES
-- Lodestone: Boosted Attack Power 1230164 / Blessing of Might, Greater Blessing of Might
(1400900,1230164),(1400900,19740),(1400900,19834),(1400900,19835),(1400900,19836),(1400900,19837),(1400900,19838),(1400900,25291),
(1400900,25782),(1400900,25916),
-- Fish Bowl: Boosted Stats 1230098 / Blessing of Kings, Greater Blessing of Kings
(1400901,1230098),(1400901,20217),(1400901,25898),
-- First Aid Kit: Boosted Stamina 1230124 / Power Word: Fortitude, Prayer of Fortitude
(1400902,1230124),(1400902,1243),(1400902,1244),(1400902,1245),(1400902,2791),(1400902,10937),(1400902,10938),(1400902,21562),
(1400902,21564),
-- Faction Banner: Boosted Spirit 1229718 / Divine Spirit, Prayer of Spirit
(1400903,1229718),(1400903,14752),(1400903,14818),(1400903,14819),(1400903,27681),(1400903,27841),
-- Incense Candle: Boosted Intellect 1229513 / Arcane Intellect, Arcane Brilliance
(1400904,1229513),(1400904,1459),(1400904,1460),(1400904,1461),(1400904,10156),(1400904,10157),(1400904,23028),
-- Enchanted Lute: 1230653 / Mark of the Wild, Gift of the Wild
(1400905,1230653),(1400905,1126),(1400905,5232),(1400905,5234),(1400905,6756),(1400905,8907),(1400905,9884),(1400905,9885),
(1400905,21849),(1400905,21850),
-- Camp Chair: Boosted Critical Chance 1229519 / Moonkin Aura
(1400906,1229519),(1400906,24907);

DELETE FROM `spell_group_stack_rules` WHERE `group_id` BETWEEN 1400900 AND 1400906;
INSERT INTO `spell_group_stack_rules` (`group_id`,`stack_rule`) VALUES
(1400900,4),(1400901,4),(1400902,4),(1400903,4),(1400904,4),(1400905,4),(1400906,4);

-- Camp feature limit per campfire tier and "placed over ... to replace it" (classic_spell_camp_feature_placement).
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'classic_spell_camp_feature_placement';
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(1307229,'classic_spell_camp_feature_placement'),(1307230,'classic_spell_camp_feature_placement'),
(1307234,'classic_spell_camp_feature_placement'),(1307239,'classic_spell_camp_feature_placement'),
(1307240,'classic_spell_camp_feature_placement'),(1307244,'classic_spell_camp_feature_placement'),
(1307245,'classic_spell_camp_feature_placement'),(1307251,'classic_spell_camp_feature_placement'),
(1307254,'classic_spell_camp_feature_placement'),(1307259,'classic_spell_camp_feature_placement'),
(1307266,'classic_spell_camp_feature_placement'),(1307223,'classic_spell_camp_feature_placement'),
(1307242,'classic_spell_camp_feature_placement'),(1307243,'classic_spell_camp_feature_placement'),
(1307246,'classic_spell_camp_feature_placement'),(1307247,'classic_spell_camp_feature_placement'),
(1307248,'classic_spell_camp_feature_placement'),(1307255,'classic_spell_camp_feature_placement'),
(1307261,'classic_spell_camp_feature_placement'),(1307264,'classic_spell_camp_feature_placement'),
(1307265,'classic_spell_camp_feature_placement');
