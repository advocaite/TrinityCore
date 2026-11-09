-- Main Gauche (1289849) and Improved Expose Armor (14168) were dummy / unrestricted proc auras nothing acted on; Lone Wolf (415370) applied its
-- +20 % damage with a pet out. Scripts in classic_talents_warrior_rogue_hunter.cpp; descriptions read from the client Spell.db2.

DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('classic_spell_rog_main_gauche','classic_spell_rog_improved_expose_armor','classic_spell_hun_lone_wolf');
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(1289849,'classic_spell_rog_main_gauche'),
(14168,'classic_spell_rog_improved_expose_armor'),
(415370,'classic_spell_hun_lone_wolf');

-- Improved Expose Armor: only when an Expose Armor (rogue family flag 0x80000) finishes (phase finish = 0x4), melee ability, any landed outcome
DELETE FROM `spell_proc` WHERE `SpellId` = 14168;
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(14168,0,8,0x80000,0,0,0,0x10,0,0,0x4,0,0,0,0,100,0,0);
