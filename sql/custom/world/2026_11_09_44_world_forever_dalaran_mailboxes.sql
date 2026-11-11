-- Dalaran (Skyborne city and the town below it): the 13 sniffed mailboxes carry player condition 161772 (Data0), a modifier tree check
-- (461442) that our players never meet, so the client would not open them (Discord 2026-11-09, "Dalaran mailboxes not usable").
-- Every other mailbox has no condition or only the side check (923 / 924). Cleared, so anyone can use them. A client that cached the
-- old object data may need its Cache folder deleted once. Safe to run again.
UPDATE `gameobject_template` SET `Data0` = 0 WHERE `type` = 19 AND `Data0` = 161772
  AND `entry` IN (560880,560881,560883,560884,560885,560886,560887,560888,560901,561073,561074,561075,651604);
