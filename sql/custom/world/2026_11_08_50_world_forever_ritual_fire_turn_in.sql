-- Ritual Fire (660739), Mulgore, ends quest 95805 Grace of An'she and Mu'sha. A player reported (2026-11-08) that the quest
-- shows Ready for turn-in with the Pouch of Smoldering Incense in the bags, but the fire at -3371.9 -1004.1 cannot be clicked.
-- Its gameobject_template_addon row (from the 2026-11-05 quest data restore) had faction 12 and flags 4.
-- Faction template 12 is Stormwind (faction 72, Alliance group, enemy group Horde, FactionTemplate.db2), so for the Horde
-- races this quest is for, the fire is a hostile object. Flags 4 is GO_FLAG_INTERACT_COND (clickable only when the server
-- sends the activate flag). Faction 0 makes it neutral to everyone, and clearing flag 4 lets any player click it; it only
-- offers the turn-in to players who have the quest done. Safe to run again.
UPDATE `gameobject_template_addon` SET `faction`=0, `flags`=`flags` & ~4 WHERE `entry`=660739;
