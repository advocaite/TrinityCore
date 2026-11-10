-- Molten Core: two stock TrinityCore script bindings still fire on top of the Classic ports, doubling the damage.
-- Baron Geddon: classic_boss_baron_geddon deals the Inferno ticks itself (19698 per tick); the stock aura script on
-- 19695 (spell_baron_geddon_inferno, OnPeriodic) dealt the same ticks a second time. 15,000 total per Inferno after this.
DELETE FROM `spell_script_names` WHERE `spell_id` = 19695 AND `ScriptName` = 'spell_baron_geddon_inferno';
-- Ragnaros: classic_boss_ragnaros places the Lava Burst trap (178088) and uses it once; the stock object script
-- (go_ragnaros_lava_burst_trap) used it again 1 ms after spawn, so every burst hit twice.
UPDATE `gameobject_template` SET `ScriptName` = '' WHERE `entry` = 178088 AND `ScriptName` = 'go_ragnaros_lava_burst_trap';
