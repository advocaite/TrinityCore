-- "A Sealed Crate" (91899, 91900, 91904, 91905): Marcy Baker <Inventory Intake> (256390, Lakeshire) offers these, but they
-- have no objectives, so anyone could take and hand them in for 5/10/10/20 Merchant's Favor without a Sealed crate.
-- Which Sealed crate each quest takes is not in the client data (needs live data), so Marcy stops offering them until the
-- quests get their real required crate. Quest templates and hand-in rows stay. Safe to run again.
DELETE FROM `creature_queststarter` WHERE `id` = 256390 AND `quest` IN (91899, 91900, 91904, 91905);
