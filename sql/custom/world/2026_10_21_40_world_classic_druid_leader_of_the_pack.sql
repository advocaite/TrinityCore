-- Classic 1.60: Leader of the Pack (17007). The client talent is a dummy aura; the party area aura 24932 (+3% critical strike chance, Cat / Bear /
-- Dire Bear Form) was never cast. C++: Custom/Classic/world/talents/classic_talents_warlock_druid.cpp (classic_talent_dru_leader_of_the_pack on the
-- talent, UpdateLeaderOfThePack from classic_talent_dru_form_talents on the form spells). Safe to run again.
DELETE FROM `spell_script_names` WHERE `spell_id` = 17007 AND `ScriptName` = 'classic_talent_dru_leader_of_the_pack';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (17007, 'classic_talent_dru_leader_of_the_pack');
