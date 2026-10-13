-- Classic 1.60.1.70009 sends 360 action buttons (SMSG_UPDATE_ACTION_BUTTONS asserts 8 * 360 + 1). The controller UI
-- binds above button 180, so the server now stores all 360 (MAX_ACTION_BUTTONS): the button column must hold 0..359.
-- Re-runnable.
ALTER TABLE `character_action` MODIFY COLUMN `button` SMALLINT UNSIGNED NOT NULL DEFAULT 0;
