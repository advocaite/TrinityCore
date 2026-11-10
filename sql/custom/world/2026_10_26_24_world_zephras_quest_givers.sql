-- 92528 Among the Faithful / 92529 Falaath Village / 93036 (Zephras): the givers Sania Silverstream and Missionary Jasaan
-- used the Al'Aketh enemy faction 3578, hostile to every player, so 93036 could not be turned in and the chain stopped.
-- No disguise or force-reaction spell exists in 70532. Use the faction of the chain's previous giver (Constable Aonda, 35).
UPDATE `creature_template` SET `faction`=35 WHERE `entry` IN (251904,257065) AND `faction`=3578;
