-- Classic 1.60: vanilla creatures that kept a retail creature_immunities row. VMaNGOS 1.12 gives none of them an immunity
-- (2026_10_11_03 only replaced the rows of creatures that have vanilla immunities), so these went untouched:
-- shadow immune: Lieutenant Fangore, Lost Whelp, Balgaras the Foul (a warlock could not hurt them);
-- fire immune: Garneg Charskull, Scorched Guardian, Immolatus (Immolatus keeps his vanilla 300 fire resistance);
-- frost immune: Frostmaul Giant, Frostmaul Preserver, Kashoch the Reaver;
-- fear/stun/polymorph immune: Marisa du'Paige; fear/horror immune: Highborne Summoner; sap immune: Brainwashed Noble;
-- charm immune: Gamon, Gurubashi Warrior, Hakkari Priest, Franklin the Friendly. Safe to run again.
UPDATE `creature_template` SET `CreatureImmunitiesId` = 0 WHERE `entry` IN
(596,599,703,1043,1364,2108,2726,6466,7137,7428,7429,10198,11355,11466,11830,14529) AND `CreatureImmunitiesId` < 0;
