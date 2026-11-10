-- Converted VMaNGOS EventAI creatures set phase 1 on every reset. SmartAI resets right before Reached Home, so an event that
-- should run when a beaten creature gets home in a later phase never ran: Overlord Mok'Morokk (1173) skipped his post-duel lines,
-- and Ogron (1273) never started his talk and walk after the Reethe fight. Their own home scripts put them back in phase 1, so
-- the reset only needs to apply while they are idle. Same change as Calvin Montague (_73) and Private Hendel (_75).
UPDATE `smart_scripts` SET `event_phase_mask`=1 WHERE `entryorguid` IN (4500,4983) AND `source_type`=0 AND `id`=0 AND `event_type`=25 AND `action_type`=22;
