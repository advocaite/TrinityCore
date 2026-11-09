-- Karrz1/wow-foverver: normal players may use `.summon all` / `.summon <own bot>` (PlayerbotV2 companions). The handler
-- (cs_misc.cpp -> PlayerbotV2TrySummon) gives non-GM callers the bot path only, so summoning real players stays GM-only.
-- 195 = Role: Player, 528 = Command: summon.
INSERT IGNORE INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES (195, 528);
