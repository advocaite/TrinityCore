-- Account data: blob (64 KiB) -> mediumblob, matches the handler limit in WorldSession::HandleUpdateAccountData
ALTER TABLE `account_data` MODIFY COLUMN `data` mediumblob NOT NULL;
ALTER TABLE `character_account_data` MODIFY COLUMN `data` mediumblob NOT NULL;
