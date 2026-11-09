-- Among the Faithful (92528), second fix: still 0/1 after 2026_11_01_00 (tested 11-01).
-- Real cause: Eavesdrop 1261177 puts the player in control of the Wardrobe (vehicle 9200). A creature controlled as a vehicle
-- gets PossessedAI in place of its SmartAI (Unit::UpdateCharmAI), so the Wardrobe's listen timer is never updated, whatever
-- its event flags, and the SmartAI that comes back afterwards has lost the timer.
-- Give the credit without any creature AI: when Eavesdrop finishes casting, the player casts Eavesdropped 1254871 (client
-- spell: kill credit 252078 "Learn about the cultists' plans" to players around the caster), the same credit spell the
-- client data has for this objective.
DELETE FROM `spell_linked_spell` WHERE `spell_trigger` = 1261177 AND `spell_effect` = 1254871 AND `type` = 0;
INSERT INTO `spell_linked_spell` (`spell_trigger`, `spell_effect`, `type`, `comment`) VALUES
(1261177, 1254871, 0, 'Eavesdrop (Wardrobe, Among the Faithful) - cast Eavesdropped (credit 252078)');
