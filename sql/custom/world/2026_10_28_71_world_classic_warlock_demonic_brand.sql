-- Classic 1.60 (WoW Forever) warlock talent Demonic Brand (1293695): Searing Pain brands the target (1293696), and the warlock's demon
-- deals extra damage with its next attacks on it (src/server/scripts/Custom/Classic/world/talents/classic_talents_warlock_druid.cpp)

DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('classic_talent_warl_demonic_brand_searing_pain', 'classic_talent_warl_demonic_brand');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(5676, 'classic_talent_warl_demonic_brand_searing_pain'),
(17919, 'classic_talent_warl_demonic_brand_searing_pain'),
(17920, 'classic_talent_warl_demonic_brand_searing_pain'),
(17921, 'classic_talent_warl_demonic_brand_searing_pain'),
(17922, 'classic_talent_warl_demonic_brand_searing_pain'),
(17923, 'classic_talent_warl_demonic_brand_searing_pain'),
(1293696, 'classic_talent_warl_demonic_brand');
