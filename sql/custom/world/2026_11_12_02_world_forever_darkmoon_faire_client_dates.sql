-- Darkmoon Faire on the client's own dates (Discord 2026-11-09, zeroyka: "the Faire started today on Forever, it is nowhere here").
--
-- 2026_10_25_36 gave the Faire a chosen rhythm (Elwynn from 2026-11-02, Mulgore from 2026-12-07, each every 60 days) because the
-- old rows pointed at the retail holidays 374 / 375, which this client does not have. The Classic 1.60 client does carry the
-- Faire: Holidays 263 (description "this time in idyllic Elwynn Forest") and 264 ("at the foot of Thunder Bluff"), each a
-- 168 hour run on a list of defined dates, always a Monday 04:00. Elwynn: 2026-09-07, 2026-11-09, 2027-01-04, 2027-03-08 ...;
-- Mulgore: 2026-10-05, 2026-12-07, 2027-02-08 ... So the Faire of this month opened on 2026-11-09, a week after ours.
--
-- With holiday and holidayStage set the server reads start and length from those dates (GameEventMgr::SetHolidayEventTime) and,
-- since 2026_11_12, moves on to the next date when a run ends. The buildings (23 / 24) follow the same dates: the client has no
-- earlier setup stage for them, so they stand for the week of the Faire (the vanilla Friday setup is not in this client's data).
UPDATE `game_event` SET `holiday` = 263, `holidayStage` = 1 WHERE `eventEntry` IN (4, 23);
UPDATE `game_event` SET `holiday` = 264, `holidayStage` = 1 WHERE `eventEntry` IN (5, 24);
