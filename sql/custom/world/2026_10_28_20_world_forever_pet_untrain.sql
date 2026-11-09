-- Classic 1.60 pet trainers: "I wish to untrain my pet." did nothing (sniffed option opens menu 6520, which has no text or options).
-- The option now asks for confirmation at the vanilla price of 10 silver and removes every spell pet trainers taught the hunter's
-- current pet (Player::UntrainClassicPet). GossipOptionID 96000000 + MenuID is recognised by Pet::IsPetUntrainGossipOption.
-- Safe to run again.
UPDATE `gossip_menu_option` SET
    `GossipOptionID` = 96000000 + `MenuID`,
    `OptionNpc` = 0,
    `ActionMenuID` = 0,
    `ActionPoiID` = 0,
    `BoxCoded` = 0,
    `BoxMoney` = 1000,
    `BoxText` = 'Are you sure you want your pet to forget everything a pet trainer taught it?',
    `BoxBroadcastTextID` = 0
WHERE `MenuID` < 1000000
  AND (`OptionBroadcastTextID` = 10621 OR `OptionText` LIKE 'I wish to untrain my pet%');

INSERT INTO `gossip_menu_option_locale` (`MenuID`, `OptionID`, `Locale`, `OptionText`, `BoxText`)
SELECT `MenuID`, `OptionID`, 'ruRU', NULL, 'Вы уверены, что хотите, чтобы питомец забыл всё, чему его научил учитель питомцев?'
FROM `gossip_menu_option` WHERE `GossipOptionID` BETWEEN 96000000 AND 96999999
ON DUPLICATE KEY UPDATE `BoxText` = 'Вы уверены, что хотите, чтобы питомец забыл всё, чему его научил учитель питомцев?';
