-- Blackwing Lair: a player who uses the Orb of Command possesses Razorgore the Untamed (12435) and gets an empty action bar.
-- TrinityCore fills the possess bar from creature_template_spell (CharmInfo::InitPossessCreateSpells) and no row existed for
-- him; VMaNGOS gives him Destroy Egg 19873, Fireball Volley 22425, Calm Dragonkin 19872 and Cleave 19632 in creature_template.
-- Bots cast through the API and never noticed. Safe to run again.
DELETE FROM `creature_template_spell` WHERE `CreatureID` = 12435;
INSERT INTO `creature_template_spell` (`CreatureID`, `Index`, `Spell`, `VerifiedBuild`) VALUES
(12435, 0, 19873, 0),
(12435, 1, 22425, 0),
(12435, 2, 19872, 0),
(12435, 3, 19632, 0);
