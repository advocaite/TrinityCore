-- Classic 1.60: Innkeeper Shaussiy (Desolace), Innkeeper Kimlya (Astranaar) and Innkeeper Shyria (Feathermoon) have their
-- vanilla food and drink list (15 items each) and a "browse" option in their gossip menu 347, but not the vendor npcflag,
-- so the shop never opens. VMaNGOS npc_flags 135 = gossip, quest giver, vendor, innkeeper.
UPDATE `creature_template` SET `npcflag` = `npcflag` | 128 WHERE `entry` IN (6737, 6738, 7736) AND (`npcflag` & 128) = 0;
