-- Guard directions: the vanilla points of interest (2026_10_11_20, VMaNGOS) had height 0 and icon 6, while the points the
-- official Forever server sends carry the real height and icon 7 (2026_10_04_05 sniff). Height = the nearest VMaNGOS creature or
-- gameobject spawn on the same map (within 40 yards; Undercity points take the nearest spawn below ground).
-- Points left at height 0 get the ground height when sent (PlayerMenu::SendPointOfInterest).
UPDATE `points_of_interest` SET `Icon` = 7 WHERE `Icon` = 6 AND `ID` IN (47,68,69,70,71,72,73,74,75,87,90,91,92,93,94,95,107,108,148,149,150,151,152,153,154,155,157,158,160,161,162,167,168,187,267,268,269,270,271,272,273,274,275,276,277,278,279,280,281,282,283,284,285,287,288,289,307,347,367,368,407,408,409,410,411,412,413,415,416,417,418,419,420,422,423,424,425,426,427,447,448,449,450,451,452,453,454,455,456,457,458,459,460,461,462,463,464,465,467,468,470,471,472,473,474,475,476,477,478,479,480,481,483,485,486,487,488,489,490,507,527,528,529,532,535,547,567,587,588,589,590,591,592,593,594,595,596,597,598,601,602,604,605,606,607,608,609,610,611,612,613,615,616,617,618,619,620,621,622,623,624,625,626,627,628,647,649,650,651,652,653,654,655,656,657,658,659,660,661,662,663,664,665,666,667,668,669,670,671,672,674,676,677,687,688,689,690,691,692,693,694,695,696,697,698,699,729,747,750,801,808,809,810,827,847,848,849,850,851,852,867,868,869,870,871,872,928,987,1427,1467,1468,1474,1647,1667,1668,1669,1670,1671,1672,1673,1674,1675,1676,1677,1678,1679,1680,1691,1692,1693);
UPDATE `points_of_interest` SET `PositionZ` = 99.61 WHERE `ID` = 47 AND `PositionZ` = 0; -- Stormwind Bank
UPDATE `points_of_interest` SET `PositionZ` = 102.92 WHERE `ID` = 68 AND `PositionZ` = 0; -- Pig and Whistle Tavern
UPDATE `points_of_interest` SET `PositionZ` = 103.17 WHERE `ID` = 69 AND `PositionZ` = 0; -- Stormwind - Rogue House
UPDATE `points_of_interest` SET `PositionZ` = 111.69 WHERE `ID` = 70 AND `PositionZ` = 0; -- Stormwind Barracks
UPDATE `points_of_interest` SET `PositionZ` = 105.56 WHERE `ID` = 71 AND `PositionZ` = 0; -- Wizard's Sanctum
UPDATE `points_of_interest` SET `PositionZ` = 90.62 WHERE `ID` = 72 AND `PositionZ` = 0; -- The Park
UPDATE `points_of_interest` SET `PositionZ` = 97.4 WHERE `ID` = 73 AND `PositionZ` = 0; -- Cathedral Of Light
UPDATE `points_of_interest` SET `PositionZ` = 95.35 WHERE `ID` = 74 AND `PositionZ` = 0; -- Hunter Lodge
UPDATE `points_of_interest` SET `PositionZ` = 122.21 WHERE `ID` = 75 AND `PositionZ` = 0; -- The Slaughtered Lamb
UPDATE `points_of_interest` SET `PositionZ` = 95.34 WHERE `ID` = 87 AND `PositionZ` = 0; -- Stormwind Vistor's Center
UPDATE `points_of_interest` SET `PositionZ` = 98.7 WHERE `ID` = 90 AND `PositionZ` = 0; -- The Protective Hide
UPDATE `points_of_interest` SET `PositionZ` = 95.97 WHERE `ID` = 91 AND `PositionZ` = 0; -- Alchemy Needs
UPDATE `points_of_interest` SET `PositionZ` = 97.4 WHERE `ID` = 92 AND `PositionZ` = 0; -- Duncan's Textiles
UPDATE `points_of_interest` SET `PositionZ` = 95.75 WHERE `ID` = 93 AND `PositionZ` = 0; -- Lilliam Sparkspindle
UPDATE `points_of_interest` SET `PositionZ` = 95.3 WHERE `ID` = 94 AND `PositionZ` = 0; -- Therum Deepforge
UPDATE `points_of_interest` SET `PositionZ` = 96.4 WHERE `ID` = 95 AND `PositionZ` = 0; -- Gelman Stonehand
UPDATE `points_of_interest` SET `PositionZ` = 96.42 WHERE `ID` = 107 AND `PositionZ` = 0; -- Arnold Leland
UPDATE `points_of_interest` SET `PositionZ` = 96.42 WHERE `ID` = 108 AND `PositionZ` = 0; -- Lucan Cordell
UPDATE `points_of_interest` SET `PositionZ` = 111.99 WHERE `ID` = 148 AND `PositionZ` = 0; -- Hall of Spirits
UPDATE `points_of_interest` SET `PositionZ` = 159.02 WHERE `ID` = 149 AND `PositionZ` = 0; -- Hunter's Hall
UPDATE `points_of_interest` SET `PositionZ` = 159.11 WHERE `ID` = 150 AND `PositionZ` = 0; -- Hall of Elders
UPDATE `points_of_interest` SET `PositionZ` = 143.64 WHERE `ID` = 151 AND `PositionZ` = 0; -- Holistic Herbalism
UPDATE `points_of_interest` SET `PositionZ` = 140.91 WHERE `ID` = 152 AND `PositionZ` = 0; -- Bena's Alchemy
UPDATE `points_of_interest` SET `PositionZ` = 140.52 WHERE `ID` = 153 AND `PositionZ` = 0; -- Dawnstrider Enchanters
UPDATE `points_of_interest` SET `PositionZ` = 145.88 WHERE `ID` = 154 AND `PositionZ` = 0; -- Thunder Bluff Armorers
UPDATE `points_of_interest` SET `PositionZ` = 129.09 WHERE `ID` = 155 AND `PositionZ` = 0; -- Karn's Smithy
UPDATE `points_of_interest` SET `PositionZ` = 133.63 WHERE `ID` = 157 AND `PositionZ` = 0; -- Stonehoof Geology
UPDATE `points_of_interest` SET `PositionZ` = 131.5 WHERE `ID` = 158 AND `PositionZ` = 0; -- Thunder Bluff Civic Information
UPDATE `points_of_interest` SET `PositionZ` = 162.28 WHERE `ID` = 160 AND `PositionZ` = 0; -- Mountaintop Bait & Tackle
UPDATE `points_of_interest` SET `PositionZ` = 107.66 WHERE `ID` = 161 AND `PositionZ` = 0; -- Pools of Vision
UPDATE `points_of_interest` SET `PositionZ` = 136.22 WHERE `ID` = 162 AND `PositionZ` = 0; -- Spiritual Healing
UPDATE `points_of_interest` SET `PositionZ` = 177.03 WHERE `ID` = 167 AND `PositionZ` = 0; -- Wind Rider Roost
UPDATE `points_of_interest` SET `PositionZ` = 166.05 WHERE `ID` = 187 AND `PositionZ` = 0; -- Aska's Kitchen
UPDATE `points_of_interest` SET `PositionZ` = 11.81 WHERE `ID` = 267 AND `PositionZ` = 0; -- Bank of Orgrimmar
UPDATE `points_of_interest` SET `PositionZ` = 61.33 WHERE `ID` = 268 AND `PositionZ` = 0; -- The Sky Tower
UPDATE `points_of_interest` SET `PositionZ` = 26.04 WHERE `ID` = 269 AND `PositionZ` = 0; -- Horde Embassy
UPDATE `points_of_interest` SET `PositionZ` = 15.49 WHERE `ID` = 270 AND `PositionZ` = 0; -- Orgrimmar Inn
UPDATE `points_of_interest` SET `PositionZ` = 10.38 WHERE `ID` = 271 AND `PositionZ` = 0; -- Orgrimmar Mailbox
UPDATE `points_of_interest` SET `PositionZ` = 58.74 WHERE `ID` = 272 AND `PositionZ` = 0; -- Orgrimmar Hunter's Hall
UPDATE `points_of_interest` SET `PositionZ` = 43.65 WHERE `ID` = 273 AND `PositionZ` = 0; -- Darkbriar Lodge
UPDATE `points_of_interest` SET `PositionZ` = 44.13 WHERE `ID` = 274 AND `PositionZ` = 0; -- Spirit Lodge
UPDATE `points_of_interest` SET `PositionZ` = 55.9 WHERE `ID` = 275 AND `PositionZ` = 0; -- Hall of the Brave
UPDATE `points_of_interest` SET `PositionZ` = 41.01 WHERE `ID` = 276 AND `PositionZ` = 0; -- Thrall's Fortress
UPDATE `points_of_interest` SET `PositionZ` = 7.95 WHERE `ID` = 277 AND `PositionZ` = 0; -- Shadowswift Brotherhood
UPDATE `points_of_interest` SET `PositionZ` = -14.8 WHERE `ID` = 278 AND `PositionZ` = 0; -- Darkfire Enclave
UPDATE `points_of_interest` SET `PositionZ` = 25.79 WHERE `ID` = 279 AND `PositionZ` = 0; -- Yelmak's Alchemy and Potions
UPDATE `points_of_interest` SET `PositionZ` = 24.43 WHERE `ID` = 280 AND `PositionZ` = 0; -- The Burning Anvil
UPDATE `points_of_interest` SET `PositionZ` = 45.5 WHERE `ID` = 281 AND `PositionZ` = 0; -- Borstan's Firepit
UPDATE `points_of_interest` SET `PositionZ` = 24.63 WHERE `ID` = 282 AND `PositionZ` = 0; -- Godan's Runeworks
UPDATE `points_of_interest` SET `PositionZ` = 43.88 WHERE `ID` = 283 AND `PositionZ` = 0; -- Survival of the Fittest
UPDATE `points_of_interest` SET `PositionZ` = 27.14 WHERE `ID` = 284 AND `PositionZ` = 0; -- Lumak's Fishing
UPDATE `points_of_interest` SET `PositionZ` = 53.28 WHERE `ID` = 285 AND `PositionZ` = 0; -- Jandi's Arboretum
UPDATE `points_of_interest` SET `PositionZ` = 24.84 WHERE `ID` = 287 AND `PositionZ` = 0; -- Kodohide Leatherworkers
UPDATE `points_of_interest` SET `PositionZ` = 26.89 WHERE `ID` = 288 AND `PositionZ` = 0; -- Red Canyon Mining
UPDATE `points_of_interest` SET `PositionZ` = 22.86 WHERE `ID` = 289 AND `PositionZ` = 0; -- Magar's Cloth Goods
UPDATE `points_of_interest` SET `PositionZ` = 29.24 WHERE `ID` = 307 AND `PositionZ` = 0; -- Nogg's Machine Shop
UPDATE `points_of_interest` SET `PositionZ` = 20.06 WHERE `ID` = 347 AND `PositionZ` = 0; -- Orgrimmar Auction House
UPDATE `points_of_interest` SET `PositionZ` = 129.29 WHERE `ID` = 367 AND `PositionZ` = 0; -- Thunder Bluff Inn
UPDATE `points_of_interest` SET `PositionZ` = 128.58 WHERE `ID` = 368 AND `PositionZ` = 0; -- Thunder Bluff Mailbox
UPDATE `points_of_interest` SET `PositionZ` = 502.3 WHERE `ID` = 407 AND `PositionZ` = 0; -- Ironforge Gryphon Master
UPDATE `points_of_interest` SET `PositionZ` = 505.85 WHERE `ID` = 408 AND `PositionZ` = 0; -- Ironforge Physician
UPDATE `points_of_interest` SET `PositionZ` = 505.9 WHERE `ID` = 409 AND `PositionZ` = 0; -- Thistlefuzz Arcanery
UPDATE `points_of_interest` SET `PositionZ` = 502.81 WHERE `ID` = 410 AND `PositionZ` = 0; -- The Bronze Kettle
UPDATE `points_of_interest` SET `PositionZ` = 504.53 WHERE `ID` = 411 AND `PositionZ` = 0; -- Deepmountain Mining Guild
UPDATE `points_of_interest` SET `PositionZ` = 524.78 WHERE `ID` = 412 AND `PositionZ` = 0; -- Hall of Mysteries
UPDATE `points_of_interest` SET `PositionZ` = 505.38 WHERE `ID` = 413 AND `PositionZ` = 0; -- Hall of Arms
UPDATE `points_of_interest` SET `PositionZ` = 504.02 WHERE `ID` = 415 AND `PositionZ` = 0; -- The Vault
UPDATE `points_of_interest` SET `PositionZ` = 502.0 WHERE `ID` = 416 AND `PositionZ` = 0; -- Ironforge Mailbox
UPDATE `points_of_interest` SET `PositionZ` = 502.0 WHERE `ID` = 417 AND `PositionZ` = 0; -- Stonefire Tavern
UPDATE `points_of_interest` SET `PositionZ` = 505.17 WHERE `ID` = 418 AND `PositionZ` = 0; -- Ironforge Auction House
UPDATE `points_of_interest` SET `PositionZ` = 501.81 WHERE `ID` = 419 AND `PositionZ` = 0; -- Berryfizz's Potions and Mixed Drinks
UPDATE `points_of_interest` SET `PositionZ` = 502.09 WHERE `ID` = 420 AND `PositionZ` = 0; -- Springspindle's Gadgets
UPDATE `points_of_interest` SET `PositionZ` = 504.28 WHERE `ID` = 422 AND `PositionZ` = 0; -- Stonebrow's Clothier
UPDATE `points_of_interest` SET `PositionZ` = 498.89 WHERE `ID` = 423 AND `PositionZ` = 0; -- The Great Forge
UPDATE `points_of_interest` SET `PositionZ` = 503.96 WHERE `ID` = 424 AND `PositionZ` = 0; -- Ironforge Visitor's Center
UPDATE `points_of_interest` SET `PositionZ` = 505.06 WHERE `ID` = 425 AND `PositionZ` = 0; -- Traveling Fisherman
UPDATE `points_of_interest` SET `PositionZ` = 511.69 WHERE `ID` = 426 AND `PositionZ` = 0; -- Ironforge Warlock Trainer
UPDATE `points_of_interest` SET `PositionZ` = 501.97 WHERE `ID` = 427 AND `PositionZ` = 0; -- Ironforge Rogue Trainer
UPDATE `points_of_interest` SET `PositionZ` = -43.02 WHERE `ID` = 447 AND `PositionZ` = 0; -- Undercity Bat Handler
UPDATE `points_of_interest` SET `PositionZ` = -55.26 WHERE `ID` = 448 AND `PositionZ` = 0; -- Undercity Guild Master
UPDATE `points_of_interest` SET `PositionZ` = -43.02 WHERE `ID` = 449 AND `PositionZ` = 0; -- Undercity Inn
UPDATE `points_of_interest` SET `PositionZ` = -43.02 WHERE `ID` = 450 AND `PositionZ` = 0; -- Undercity Mailbox
UPDATE `points_of_interest` SET `PositionZ` = 59.41 WHERE `ID` = 451 AND `PositionZ` = 0; -- Undercity Zeppelin
UPDATE `points_of_interest` SET `PositionZ` = -61.41 WHERE `ID` = 452 AND `PositionZ` = 0; -- Undercity Mage Trainers
UPDATE `points_of_interest` SET `PositionZ` = -61.41 WHERE `ID` = 453 AND `PositionZ` = 0; -- Undercity Warlock Trainers
UPDATE `points_of_interest` SET `PositionZ` = -57.13 WHERE `ID` = 454 AND `PositionZ` = 0; -- Undercity Priest Trainers
UPDATE `points_of_interest` SET `PositionZ` = -57.11 WHERE `ID` = 455 AND `PositionZ` = 0; -- Undercity Warrior Trainers
UPDATE `points_of_interest` SET `PositionZ` = -62.2 WHERE `ID` = 456 AND `PositionZ` = 0; -- Undercity Rogue Trainers
UPDATE `points_of_interest` SET `PositionZ` = -55.11 WHERE `ID` = 457 AND `PositionZ` = 0; -- The Apothecarium
UPDATE `points_of_interest` SET `PositionZ` = -62.06 WHERE `ID` = 458 AND `PositionZ` = 0; -- Undercity Blacksmithing Trainer
UPDATE `points_of_interest` SET `PositionZ` = -62.1 WHERE `ID` = 459 AND `PositionZ` = 0; -- Undercity Mining Trainer
UPDATE `points_of_interest` SET `PositionZ` = -55.26 WHERE `ID` = 460 AND `PositionZ` = 0; -- Undercity Cooking Trainer
UPDATE `points_of_interest` SET `PositionZ` = 1341.47 WHERE `ID` = 461 AND `PositionZ` = 0; -- Warrior's Terrace
UPDATE `points_of_interest` SET `PositionZ` = -62.09 WHERE `ID` = 462 AND `PositionZ` = 0; -- Undercity Tailoring Trainer
UPDATE `points_of_interest` SET `PositionZ` = 1334.01 WHERE `ID` = 463 AND `PositionZ` = 0; -- Darnassus Herbalism Trainer
UPDATE `points_of_interest` SET `PositionZ` = 1360.08 WHERE `ID` = 464 AND `PositionZ` = 0; -- Temple of the Moon
UPDATE `points_of_interest` SET `PositionZ` = 1318.01 WHERE `ID` = 465 AND `PositionZ` = 0; -- Darnassus Bank
UPDATE `points_of_interest` SET `PositionZ` = 1318.16 WHERE `ID` = 467 AND `PositionZ` = 0; -- Rut'theran Village
UPDATE `points_of_interest` SET `PositionZ` = 1329.99 WHERE `ID` = 468 AND `PositionZ` = 0; -- Darnassus Inn
UPDATE `points_of_interest` SET `PositionZ` = 1333.08 WHERE `ID` = 470 AND `PositionZ` = 0; -- Darnassus Tailor
UPDATE `points_of_interest` SET `PositionZ` = 1343.39 WHERE `ID` = 471 AND `PositionZ` = 0; -- Darnassus Leatherworking Trainer
UPDATE `points_of_interest` SET `PositionZ` = 1343.39 WHERE `ID` = 472 AND `PositionZ` = 0; -- Darnassus Skinning Trainer
UPDATE `points_of_interest` SET `PositionZ` = 1333.08 WHERE `ID` = 473 AND `PositionZ` = 0; -- Darnassus Enchanting Trainer
UPDATE `points_of_interest` SET `PositionZ` = 1323.56 WHERE `ID` = 474 AND `PositionZ` = 0; -- Darnassus First Aid Trainer
UPDATE `points_of_interest` SET `PositionZ` = -62.07 WHERE `ID` = 475 AND `PositionZ` = 0; -- Undercity Enchanting Trainer
UPDATE `points_of_interest` SET `PositionZ` = 1321.67 WHERE `ID` = 476 AND `PositionZ` = 0; -- Darnassus Alchemy Trainer
UPDATE `points_of_interest` SET `PositionZ` = -62.08 WHERE `ID` = 477 AND `PositionZ` = 0; -- Undercity Herbalism Trainer
UPDATE `points_of_interest` SET `PositionZ` = 1321.74 WHERE `ID` = 478 AND `PositionZ` = 0; -- Darnassus Cooking Trainer
UPDATE `points_of_interest` SET `PositionZ` = 1343.24 WHERE `ID` = 479 AND `PositionZ` = 0; -- Darnassus Hunter Trainer
UPDATE `points_of_interest` SET `PositionZ` = -62.06 WHERE `ID` = 480 AND `PositionZ` = 0; -- Undercity Leatherworking Trainer
UPDATE `points_of_interest` SET `PositionZ` = -62.06 WHERE `ID` = 481 AND `PositionZ` = 0; -- Undercity Skinning Trainer
UPDATE `points_of_interest` SET `PositionZ` = -62.07 WHERE `ID` = 483 AND `PositionZ` = 0; -- Undercity First Aid Trainer
UPDATE `points_of_interest` SET `PositionZ` = -62.22 WHERE `ID` = 485 AND `PositionZ` = 0; -- Undercity Engineering Trainer
UPDATE `points_of_interest` SET `PositionZ` = 1326.05 WHERE `ID` = 486 AND `PositionZ` = 0; -- Darnassus Druid Trainer
UPDATE `points_of_interest` SET `PositionZ` = -62.09 WHERE `ID` = 487 AND `PositionZ` = 0; -- Undercity Fishing Trainer
UPDATE `points_of_interest` SET `PositionZ` = -52.06 WHERE `ID` = 488 AND `PositionZ` = 0; -- Undercity Bank
UPDATE `points_of_interest` SET `PositionZ` = 1315.81 WHERE `ID` = 489 AND `PositionZ` = 0; -- Darnassus Fishing Trainer
UPDATE `points_of_interest` SET `PositionZ` = 1346.7 WHERE `ID` = 490 AND `PositionZ` = 0; -- Darnassus Guild Master
UPDATE `points_of_interest` SET `PositionZ` = 1325.97 WHERE `ID` = 507 AND `PositionZ` = 0; -- Darnassus Rogue Trainer
UPDATE `points_of_interest` SET `PositionZ` = 95.42 WHERE `ID` = 527 AND `PositionZ` = 0; -- Stormwind Auction House
UPDATE `points_of_interest` SET `PositionZ` = 96.11 WHERE `ID` = 528 AND `PositionZ` = 0; -- Stormwind Mailbox
UPDATE `points_of_interest` SET `PositionZ` = 97.99 WHERE `ID` = 529 AND `PositionZ` = 0; -- The Gilded Rose
UPDATE `points_of_interest` SET `PositionZ` = 92.06 WHERE `ID` = 532 AND `PositionZ` = 0; -- The Deeprun Tram
UPDATE `points_of_interest` SET `PositionZ` = 109.7 WHERE `ID` = 535 AND `PositionZ` = 0; -- Stormwind Gryphon Master
UPDATE `points_of_interest` SET `PositionZ` = 501.95 WHERE `ID` = 547 AND `PositionZ` = 0; -- Deeprun Tram
UPDATE `points_of_interest` SET `PositionZ` = 144.92 WHERE `ID` = 567 AND `PositionZ` = 0; -- Mooranta
UPDATE `points_of_interest` SET `PositionZ` = 16.54 WHERE `ID` = 587 AND `PositionZ` = 0; -- Razor Hill Inn
UPDATE `points_of_interest` SET `PositionZ` = 17.57 WHERE `ID` = 588 AND `PositionZ` = 0; -- Thotar
UPDATE `points_of_interest` SET `PositionZ` = 21.09 WHERE `ID` = 589 AND `PositionZ` = 0; -- Un'Thuwa
UPDATE `points_of_interest` SET `PositionZ` = 10.61 WHERE `ID` = 590 AND `PositionZ` = 0; -- Tai'jin
UPDATE `points_of_interest` SET `PositionZ` = 17.57 WHERE `ID` = 591 AND `PositionZ` = 0; -- Kaplak
UPDATE `points_of_interest` SET `PositionZ` = 10.61 WHERE `ID` = 592 AND `PositionZ` = 0; -- Swart
UPDATE `points_of_interest` SET `PositionZ` = 9.66 WHERE `ID` = 593 AND `PositionZ` = 0; -- Tarshaw Jaggedscar
UPDATE `points_of_interest` SET `PositionZ` = 11.17 WHERE `ID` = 594 AND `PositionZ` = 0; -- Dhugru Gorelust
UPDATE `points_of_interest` SET `PositionZ` = 22.98 WHERE `ID` = 595 AND `PositionZ` = 0; -- Miao'zan
UPDATE `points_of_interest` SET `PositionZ` = 23.6 WHERE `ID` = 596 AND `PositionZ` = 0; -- Mishiki
UPDATE `points_of_interest` SET `PositionZ` = 15.89 WHERE `ID` = 597 AND `PositionZ` = 0; -- Dwukk
UPDATE `points_of_interest` SET `PositionZ` = 10.61 WHERE `ID` = 598 AND `PositionZ` = 0; -- Rawrk
UPDATE `points_of_interest` SET `PositionZ` = 16.39 WHERE `ID` = 601 AND `PositionZ` = 0; -- Krunn
UPDATE `points_of_interest` SET `PositionZ` = 13.36 WHERE `ID` = 602 AND `PositionZ` = 0; -- Mukdrak
UPDATE `points_of_interest` SET `PositionZ` = -8.87 WHERE `ID` = 604 AND `PositionZ` = 0; -- Bloodhoof Village Inn
UPDATE `points_of_interest` SET `PositionZ` = -8.87 WHERE `ID` = 605 AND `PositionZ` = 0; -- Vira Younghoof
UPDATE `points_of_interest` SET `PositionZ` = -5.36 WHERE `ID` = 606 AND `PositionZ` = 0; -- Gennia Runetotem
UPDATE `points_of_interest` SET `PositionZ` = -5.36 WHERE `ID` = 607 AND `PositionZ` = 0; -- Narm Skychaser
UPDATE `points_of_interest` SET `PositionZ` = -4.52 WHERE `ID` = 608 AND `PositionZ` = 0; -- Yaw Sharpmane
UPDATE `points_of_interest` SET `PositionZ` = -8.96 WHERE `ID` = 609 AND `PositionZ` = 0; -- Krang Stonehoof
UPDATE `points_of_interest` SET `PositionZ` = -9.29 WHERE `ID` = 610 AND `PositionZ` = 0; -- Pyall Silentstride
UPDATE `points_of_interest` SET `PositionZ` = -9.34 WHERE `ID` = 611 AND `PositionZ` = 0; -- Chaw Stronghide
UPDATE `points_of_interest` SET `PositionZ` = -9.34 WHERE `ID` = 612 AND `PositionZ` = 0; -- Yonn Deepcut
UPDATE `points_of_interest` SET `PositionZ` = -8.31 WHERE `ID` = 613 AND `PositionZ` = 0; -- Uthan Stillwater
UPDATE `points_of_interest` SET `PositionZ` = 34.34 WHERE `ID` = 615 AND `PositionZ` = 0; -- Gallows' End Tavern
UPDATE `points_of_interest` SET `PositionZ` = 41.2 WHERE `ID` = 616 AND `PositionZ` = 0; -- Cain Firesong
UPDATE `points_of_interest` SET `PositionZ` = 41.2 WHERE `ID` = 617 AND `PositionZ` = 0; -- Dark Cleric Beryl
UPDATE `points_of_interest` SET `PositionZ` = 34.34 WHERE `ID` = 618 AND `PositionZ` = 0; -- Marion Call
UPDATE `points_of_interest` SET `PositionZ` = 41.2 WHERE `ID` = 619 AND `PositionZ` = 0; -- Rupert Boch
UPDATE `points_of_interest` SET `PositionZ` = 33.72 WHERE `ID` = 620 AND `PositionZ` = 0; -- Austil de Mon
UPDATE `points_of_interest` SET `PositionZ` = 36.1 WHERE `ID` = 621 AND `PositionZ` = 0; -- Carolai Anise
UPDATE `points_of_interest` SET `PositionZ` = 41.2 WHERE `ID` = 622 AND `PositionZ` = 0; -- Vance Undergloom
UPDATE `points_of_interest` SET `PositionZ` = 34.34 WHERE `ID` = 623 AND `PositionZ` = 0; -- Nurse Neela
UPDATE `points_of_interest` SET `PositionZ` = 23.26 WHERE `ID` = 624 AND `PositionZ` = 0; -- Clyde Kellen
UPDATE `points_of_interest` SET `PositionZ` = 36.76 WHERE `ID` = 625 AND `PositionZ` = 0; -- Faruza
UPDATE `points_of_interest` SET `PositionZ` = 35.49 WHERE `ID` = 626 AND `PositionZ` = 0; -- Shelene Rhobart
UPDATE `points_of_interest` SET `PositionZ` = 34.46 WHERE `ID` = 627 AND `PositionZ` = 0; -- Rand Rhobart
UPDATE `points_of_interest` SET `PositionZ` = 34.63 WHERE `ID` = 628 AND `PositionZ` = 0; -- Bowen Brisboise
UPDATE `points_of_interest` SET `PositionZ` = 48.23 WHERE `ID` = 647 AND `PositionZ` = 0; -- Eldrin
UPDATE `points_of_interest` SET `PositionZ` = 58.87 WHERE `ID` = 649 AND `PositionZ` = 0; -- Lee Brown
UPDATE `points_of_interest` SET `PositionZ` = 64.52 WHERE `ID` = 650 AND `PositionZ` = 0; -- Adele Fielder
UPDATE `points_of_interest` SET `PositionZ` = 64.52 WHERE `ID` = 651 AND `PositionZ` = 0; -- Helene Peltskinner
UPDATE `points_of_interest` SET `PositionZ` = 63.9 WHERE `ID` = 652 AND `PositionZ` = 0; -- Zaldimar Wefhellt
UPDATE `points_of_interest` SET `PositionZ` = 63.9 WHERE `ID` = 653 AND `PositionZ` = 0; -- Priestess Josetta
UPDATE `points_of_interest` SET `PositionZ` = 63.9 WHERE `ID` = 654 AND `PositionZ` = 0; -- Michelle Belle
UPDATE `points_of_interest` SET `PositionZ` = 57.03 WHERE `ID` = 655 AND `PositionZ` = 0; -- Tomas
UPDATE `points_of_interest` SET `PositionZ` = 49.88 WHERE `ID` = 656 AND `PositionZ` = 0; -- Maximillian Crowe
UPDATE `points_of_interest` SET `PositionZ` = 63.9 WHERE `ID` = 657 AND `PositionZ` = 0; -- Keryn Sylvius
UPDATE `points_of_interest` SET `PositionZ` = 56.97 WHERE `ID` = 658 AND `PositionZ` = 0; -- Lion's Pride Inn
UPDATE `points_of_interest` SET `PositionZ` = 58.43 WHERE `ID` = 659 AND `PositionZ` = 0; -- Smith Argus
UPDATE `points_of_interest` SET `PositionZ` = 57.66 WHERE `ID` = 660 AND `PositionZ` = 0; -- Brother Wilhelm
UPDATE `points_of_interest` SET `PositionZ` = 57.88 WHERE `ID` = 661 AND `PositionZ` = 0; -- Lyria Du Lac
UPDATE `points_of_interest` SET `PositionZ` = 115.22 WHERE `ID` = 662 AND `PositionZ` = 0; -- Herbalist Pomeroy
UPDATE `points_of_interest` SET `PositionZ` = 115.12 WHERE `ID` = 663 AND `PositionZ` = 0; -- Alchemist Mallory
UPDATE `points_of_interest` SET `PositionZ` = 403.62 WHERE `ID` = 664 AND `PositionZ` = 0; -- Magis Sparkmantle
UPDATE `points_of_interest` SET `PositionZ` = 403.62 WHERE `ID` = 665 AND `PositionZ` = 0; -- Azar Stronghammer
UPDATE `points_of_interest` SET `PositionZ` = 399.74 WHERE `ID` = 666 AND `PositionZ` = 0; -- Maxan Anvol
UPDATE `points_of_interest` SET `PositionZ` = 399.74 WHERE `ID` = 667 AND `PositionZ` = 0; -- Granis Swiftaxe
UPDATE `points_of_interest` SET `PositionZ` = 399.74 WHERE `ID` = 668 AND `PositionZ` = 0; -- Thamner Pol
UPDATE `points_of_interest` SET `PositionZ` = 399.18 WHERE `ID` = 669 AND `PositionZ` = 0; -- Hogral Bakkan
UPDATE `points_of_interest` SET `PositionZ` = 398.85 WHERE `ID` = 670 AND `PositionZ` = 0; -- Gremlock Pilsnor
UPDATE `points_of_interest` SET `PositionZ` = 400.85 WHERE `ID` = 671 AND `PositionZ` = 0; -- Thunderbrew Distillery
UPDATE `points_of_interest` SET `PositionZ` = 397.33 WHERE `ID` = 672 AND `PositionZ` = 0; -- Tognus Flintfire
UPDATE `points_of_interest` SET `PositionZ` = 407.84 WHERE `ID` = 674 AND `PositionZ` = 0; -- Grif Wildheart
UPDATE `points_of_interest` SET `PositionZ` = 393.41 WHERE `ID` = 676 AND `PositionZ` = 0; -- Yarr Hammerstone
UPDATE `points_of_interest` SET `PositionZ` = 404.38 WHERE `ID` = 677 AND `PositionZ` = 0; -- Gimrizz Shadowcog
UPDATE `points_of_interest` SET `PositionZ` = 1325.15 WHERE `ID` = 687 AND `PositionZ` = 0; -- Radnaal Maneweaver
UPDATE `points_of_interest` SET `PositionZ` = 1326.09 WHERE `ID` = 688 AND `PositionZ` = 0; -- Nadyia Maneweaver
UPDATE `points_of_interest` SET `PositionZ` = 1313.92 WHERE `ID` = 689 AND `PositionZ` = 0; -- Byancie
UPDATE `points_of_interest` SET `PositionZ` = 1313.92 WHERE `ID` = 690 AND `PositionZ` = 0; -- Laurna Morninglight
UPDATE `points_of_interest` SET `PositionZ` = 1308.86 WHERE `ID` = 691 AND `PositionZ` = 0; -- Kyra Windblade
UPDATE `points_of_interest` SET `PositionZ` = 1308.86 WHERE `ID` = 692 AND `PositionZ` = 0; -- Jannok Breezesong
UPDATE `points_of_interest` SET `PositionZ` = 1308.19 WHERE `ID` = 693 AND `PositionZ` = 0; -- Dazalar
UPDATE `points_of_interest` SET `PositionZ` = 1295.6 WHERE `ID` = 694 AND `PositionZ` = 0; -- Zarrin
UPDATE `points_of_interest` SET `PositionZ` = 1297.78 WHERE `ID` = 695 AND `PositionZ` = 0; -- Malorne Bladeleaf
UPDATE `points_of_interest` SET `PositionZ` = 1297.78 WHERE `ID` = 696 AND `PositionZ` = 0; -- Cyndra Kindwhisper
UPDATE `points_of_interest` SET `PositionZ` = 1293.78 WHERE `ID` = 697 AND `PositionZ` = 0; -- Kal
UPDATE `points_of_interest` SET `PositionZ` = 1308.88 WHERE `ID` = 698 AND `PositionZ` = 0; -- Dolanaar Inn
UPDATE `points_of_interest` SET `PositionZ` = 1335.3 WHERE `ID` = 699 AND `PositionZ` = 0; -- Alanna Raveneye
UPDATE `points_of_interest` SET `PositionZ` = -62.07 WHERE `ID` = 729 AND `PositionZ` = 0; -- Archibald
UPDATE `points_of_interest` SET `PositionZ` = 1330.87 WHERE `ID` = 750 AND `PositionZ` = 0; -- Ilyenia Moonfire
UPDATE `points_of_interest` SET `PositionZ` = 17.44 WHERE `ID` = 801 AND `PositionZ` = 0; -- Lau'Tiki
UPDATE `points_of_interest` SET `PositionZ` = 96.86 WHERE `ID` = 808 AND `PositionZ` = 0; -- Woo Ping
UPDATE `points_of_interest` SET `PositionZ` = 24.21 WHERE `ID` = 809 AND `PositionZ` = 0; -- Sayoc & Hanashi
UPDATE `points_of_interest` SET `PositionZ` = 129.28 WHERE `ID` = 810 AND `PositionZ` = 0; -- Ansekhwa
UPDATE `points_of_interest` SET `PositionZ` = 24.12 WHERE `ID` = 827 AND `PositionZ` = 0; -- Orgrimmar Zeppelin Tower
UPDATE `points_of_interest` SET `PositionZ` = 14.61 WHERE `ID` = 847 AND `PositionZ` = 0; -- Shoja'my
UPDATE `points_of_interest` SET `PositionZ` = 46.51 WHERE `ID` = 848 AND `PositionZ` = 0; -- Xon'cha
UPDATE `points_of_interest` SET `PositionZ` = -8.78 WHERE `ID` = 849 AND `PositionZ` = 0; -- Seikwa
UPDATE `points_of_interest` SET `PositionZ` = 128.26 WHERE `ID` = 850 AND `PositionZ` = 0; -- Bulrug
UPDATE `points_of_interest` SET `PositionZ` = 35.23 WHERE `ID` = 851 AND `PositionZ` = 0; -- Morganus
UPDATE `points_of_interest` SET `PositionZ` = -43.02 WHERE `ID` = 852 AND `PositionZ` = 0; -- Anya Maulray
UPDATE `points_of_interest` SET `PositionZ` = 507.84 WHERE `ID` = 867 AND `PositionZ` = 0; -- Ulbrek Firehand
UPDATE `points_of_interest` SET `PositionZ` = 402.29 WHERE `ID` = 868 AND `PositionZ` = 0; -- Shelby Stoneflint
UPDATE `points_of_interest` SET `PositionZ` = 56.91 WHERE `ID` = 869 AND `PositionZ` = 0; -- Erma
UPDATE `points_of_interest` SET `PositionZ` = 95.35 WHERE `ID` = 870 AND `PositionZ` = 0; -- Jenova Stoneshield
UPDATE `points_of_interest` SET `PositionZ` = 1307.67 WHERE `ID` = 871 AND `PositionZ` = 0; -- Seriadne
UPDATE `points_of_interest` SET `PositionZ` = 1342.57 WHERE `ID` = 872 AND `PositionZ` = 0; -- Alassin
UPDATE `points_of_interest` SET `PositionZ` = 502.32 WHERE `ID` = 928 AND `PositionZ` = 0; -- Bixi and Buliwyf
UPDATE `points_of_interest` SET `PositionZ` = 504.51 WHERE `ID` = 987 AND `PositionZ` = 0; -- Finespindle's Leather Goods
UPDATE `points_of_interest` SET `PositionZ` = 106.6 WHERE `ID` = 1427 AND `PositionZ` = 0; -- Shaina Fuller
UPDATE `points_of_interest` SET `PositionZ` = 101.14 WHERE `ID` = 1467 AND `PositionZ` = 0; -- Champions' Hall
UPDATE `points_of_interest` SET `PositionZ` = 49.03 WHERE `ID` = 1468 AND `PositionZ` = 0; -- Hall of Legends
UPDATE `points_of_interest` SET `PositionZ` = 1317.7 WHERE `ID` = 1474 AND `PositionZ` = 0; -- Darnassus Mailbox
UPDATE `points_of_interest` SET `PositionZ` = 1330.87 WHERE `ID` = 1647 AND `PositionZ` = 0; -- Aethalas
UPDATE `points_of_interest` SET `PositionZ` = 1330.87 WHERE `ID` = 1667 AND `PositionZ` = 0; -- Brogun Stoneshield
UPDATE `points_of_interest` SET `PositionZ` = 159.02 WHERE `ID` = 1668 AND `PositionZ` = 0; -- Taim Ragetotem
UPDATE `points_of_interest` SET `PositionZ` = 160.6 WHERE `ID` = 1669 AND `PositionZ` = 0; -- Kergul Bloodaxe
UPDATE `points_of_interest` SET `PositionZ` = -63.62 WHERE `ID` = 1670 AND `PositionZ` = 0; -- Kurden Bloodclaw
UPDATE `points_of_interest` SET `PositionZ` = -63.62 WHERE `ID` = 1671 AND `PositionZ` = 0; -- Grizzle Halfmane
UPDATE `points_of_interest` SET `PositionZ` = 510.41 WHERE `ID` = 1672 AND `PositionZ` = 0; -- Glordrum Steelbeard
UPDATE `points_of_interest` SET `PositionZ` = 510.41 WHERE `ID` = 1673 AND `PositionZ` = 0; -- Lylandris
UPDATE `points_of_interest` SET `PositionZ` = 120.89 WHERE `ID` = 1674 AND `PositionZ` = 0; -- Thelman Slatefist
UPDATE `points_of_interest` SET `PositionZ` = 120.97 WHERE `ID` = 1675 AND `PositionZ` = 0; -- Elfarran
UPDATE `points_of_interest` SET `PositionZ` = -63.63 WHERE `ID` = 1676 AND `PositionZ` = 0; -- Sir Malory Wheeler
UPDATE `points_of_interest` SET `PositionZ` = 104.73 WHERE `ID` = 1677 AND `PositionZ` = 0; -- Martin Lindsey
UPDATE `points_of_interest` SET `PositionZ` = 510.41 WHERE `ID` = 1678 AND `PositionZ` = 0; -- Donal Osgood
UPDATE `points_of_interest` SET `PositionZ` = 120.89 WHERE `ID` = 1679 AND `PositionZ` = 0; -- Lady Hoteshem
UPDATE `points_of_interest` SET `PositionZ` = 1330.87 WHERE `ID` = 1680 AND `PositionZ` = 0; -- Keras Wolfheart
UPDATE `points_of_interest` SET `PositionZ` = -43.02 WHERE `ID` = 1691 AND `PositionZ` = 0; -- Undercity Auction House
UPDATE `points_of_interest` SET `PositionZ` = 134.95 WHERE `ID` = 1692 AND `PositionZ` = 0; -- Thunder Bluff Auction house
UPDATE `points_of_interest` SET `PositionZ` = 1321.67 WHERE `ID` = 1693 AND `PositionZ` = 0; -- Darnassus Auction House
-- Thunder Bluff Bank: back to the point the Forever server sends (2026_10_11_20 replaced it with the vanilla one)
UPDATE `points_of_interest` SET `PositionX` = -1257.800, `PositionY` = 24.143, `PositionZ` = 127.718, `Icon` = 7, `VerifiedBuild` = 70205 WHERE `ID` = 168;
