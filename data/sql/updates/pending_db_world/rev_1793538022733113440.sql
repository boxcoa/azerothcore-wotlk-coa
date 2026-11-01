-- Mithril Bound Trunk (21228): guaranteed uncommon item level 41-50 world-drop gear, common drop rates raised
UPDATE `item_loot_template` SET `Chance` = 43 WHERE `Entry` = 21228 AND `Item` IN (4304, 4339);
UPDATE `item_loot_template` SET `Chance` = 33 WHERE `Entry` = 21228 AND `Item` = 8170;
UPDATE `item_loot_template` SET `Chance` = 32 WHERE `Entry` = 21228 AND `Item` = 14048;
UPDATE `item_loot_template` SET `Chance` = 22 WHERE `Entry` = 21228 AND `Item` IN (3928, 6149);

DELETE FROM `item_loot_template` WHERE `Entry` = 21228 AND `Reference` IN
(1023842, 1023943, 1024044, 1024145, 1024246, 1024347, 1024448, 1024549, 1024650, 1024751);
INSERT INTO `item_loot_template` (`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(21228, 1023842, 1023842, 0, 0, 1, 3, 1, 1, 'Mithril Bound Trunk - (ReferenceTable) World Drop Uncommon ilvl 41'),
(21228, 1023943, 1023943, 0, 0, 1, 3, 1, 1, 'Mithril Bound Trunk - (ReferenceTable) World Drop Uncommon ilvl 42'),
(21228, 1024044, 1024044, 0, 0, 1, 3, 1, 1, 'Mithril Bound Trunk - (ReferenceTable) World Drop Uncommon ilvl 43'),
(21228, 1024145, 1024145, 0, 0, 1, 3, 1, 1, 'Mithril Bound Trunk - (ReferenceTable) World Drop Uncommon ilvl 44'),
(21228, 1024246, 1024246, 0, 0, 1, 3, 1, 1, 'Mithril Bound Trunk - (ReferenceTable) World Drop Uncommon ilvl 45'),
(21228, 1024347, 1024347, 0, 0, 1, 3, 1, 1, 'Mithril Bound Trunk - (ReferenceTable) World Drop Uncommon ilvl 46'),
(21228, 1024448, 1024448, 0, 0, 1, 3, 1, 1, 'Mithril Bound Trunk - (ReferenceTable) World Drop Uncommon ilvl 47'),
(21228, 1024549, 1024549, 0, 0, 1, 3, 1, 1, 'Mithril Bound Trunk - (ReferenceTable) World Drop Uncommon ilvl 48'),
(21228, 1024650, 1024650, 0, 0, 1, 3, 1, 1, 'Mithril Bound Trunk - (ReferenceTable) World Drop Uncommon ilvl 49'),
(21228, 1024751, 1024751, 0, 0, 1, 3, 1, 1, 'Mithril Bound Trunk - (ReferenceTable) World Drop Uncommon ilvl 50');
