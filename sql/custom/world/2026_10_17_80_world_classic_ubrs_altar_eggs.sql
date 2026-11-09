-- UBRS: the Blackrock Altar (175706) becomes a three-player channel (script classic_go_blackrock_altar) and the Rookery Egg (175124)
-- hatches through script classic_go_rookery_egg. The egg's core trap radius (Data2, was 3) is zeroed so the trap spell chain 16474 -> 15745,
-- which needs spells the client data may not carry, does not fire a second whelp.
UPDATE gameobject_template SET ScriptName = 'classic_go_blackrock_altar' WHERE entry = 175706;
UPDATE gameobject_template SET ScriptName = 'classic_go_rookery_egg', Data2 = 0 WHERE entry = 175124;
