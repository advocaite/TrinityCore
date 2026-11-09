-- Migration: 0019_bot_texts
-- Date:    2026-10-05
-- Purpose: The bots' speech, carried over from Karrz's ike3 playerbot build
--          (wow-1.12 src/game/PlayerBots/sql/world/ai_playerbot_texts.sql). Loaded once at startup by
--          Bot/BotTextMgr.cpp, which picks a random line per key and fills its placeholders.
--
--          playerbot_v2_bot_text
--            name        the key code asks for ('hello', 'goodbye', 'reply', 'broadcast_looting_item_rare', ...)
--            text        the line, with ike3's %placeholders kept as they were (%s, %my_level, %item_link, ...)
--            say_type    0 = say, 1 = yell (only the emote answers use it)
--            reply_type  for name='reply': 0 not understood, 1 grudge, 2 victim, 3 attacker, 4 hello, 5 name,
--                        6 admin abuse (ike3 ChatReplyType)
--          playerbot_v2_bot_text_chance
--            name        a key from the table above
--            chance      percent (0-100) that the line is spoken when its event happens; scaled at runtime by
--                        Playerbot.Chatter.ChanceScale. ike3's BroadcastChance* values (out of 30000) converted.
--
--          Carried over: every ike3 row except the toxic / Thunderfury spam keys (suggest_something_toxic,
--          suggest_toxic_links, thunderfury_spam) and two crude 'reply' lines. English text only (the
--          text_loc columns are dropped). Added as data: ike3's hard-coded question answers from
--          ChatReplyAction::GenerateReplyMessage (reply_question_*) and the verbal emote answers from
--          EmoteActionBase::ReceiveEmote (emote_reply_*).
--
--          The texts are realm-independent content, so they live in the SHARED playerbot database (same
--          scope as 0017's tables). The runtime reads them as `<Playerbot.SharedDatabase>`.table.
--          Statements are self-contained and use INSERT IGNORE against a unique key, so re-running is a no-op.
--          No text contains a ';' (the migration runner splits statements on it).
-- Reverts: yes (DROP the two {SHARED} tables).

-- @target: shared

CREATE TABLE IF NOT EXISTS {SHARED}.playerbot_v2_bot_text (
    id          INT UNSIGNED     NOT NULL AUTO_INCREMENT,
    name        VARCHAR(64)      NOT NULL,
    text        VARCHAR(1024)    NOT NULL,
    say_type    TINYINT UNSIGNED NOT NULL DEFAULT 0,
    reply_type  TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (id),
    UNIQUE KEY uq_name_reply_text (name, reply_type, text(200)),
    KEY idx_name (name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS {SHARED}.playerbot_v2_bot_text_chance (
    name        VARCHAR(64)      NOT NULL,
    chance      FLOAT            NOT NULL DEFAULT 100,
    PRIMARY KEY (name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('string_unknown_area', 'the middle of nowhere', 0, 0),
    ('string_unknown_area', 'an undisclosed location', 0, 0),
    ('string_unknown_area', 'somewhere', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('string_empty_link', 'something', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_looting_item_normal', 'wonder what %item_link tastes like', 0, 0),
    ('broadcast_looting_item_normal', 'wonder what %item_link tastes like', 0, 0),
    ('broadcast_looting_item_normal', 'well, I can keep looting %item_link all day', 0, 0),
    ('broadcast_looting_item_normal', 'another day, another %item_link', 0, 0),
    ('broadcast_looting_item_normal', 'looted some %item_link', 0, 0),
    ('broadcast_looting_item_normal', 'some %item_link is alright', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_looting_item_poor', 'nooo, I got %item_link', 0, 0),
    ('broadcast_looting_item_poor', 'aww not this trash again %item_link', 0, 0),
    ('broadcast_looting_item_poor', 'seem to be looting trash %item_link', 0, 0),
    ('broadcast_looting_item_poor', 'well, it''s better than nothing I guess %item_link', 0, 0),
    ('broadcast_looting_item_poor', 'not sure what to do with %item_link', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_looting_item_uncommon', 'not bad, I just got %item_link', 0, 0),
    ('broadcast_looting_item_uncommon', 'just looted %item_link in %zone_name', 0, 0),
    ('broadcast_looting_item_uncommon', 'I could put that to good use %item_link', 0, 0),
    ('broadcast_looting_item_uncommon', 'money money money %item_link', 0, 0),
    ('broadcast_looting_item_uncommon', 'got %item_link', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_looting_item_rare', '%item_link is hunter bis', 0, 0),
    ('broadcast_looting_item_rare', '%item_link is %my_class bis', 0, 0),
    ('broadcast_looting_item_rare', 'RNG is not bad today %item_link', 0, 0),
    ('broadcast_looting_item_rare', 'sweet %item_link, freshly looted', 0, 0),
    ('broadcast_looting_item_rare', 'wow, I just got %item_link', 0, 0),
    ('broadcast_looting_item_rare', '%item_link is hunter bis', 0, 0),
    ('broadcast_looting_item_rare', '%item_link is %my_class bis', 0, 0),
    ('broadcast_looting_item_rare', 'RNG is not bad today %item_link', 0, 0),
    ('broadcast_looting_item_rare', 'sweet %item_link, freshly looted', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_looting_item_epic', 'OMG, look at what I just got %item_link!!!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_looting_item_legendary', 'No @#$@% way! This can not be, I got %item_link, this is insane', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_looting_item_artifact', 'No @#$@% way! This can not be, I got %item_link, this is insane', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_quest_accepted_generic', 'I have just taken the %quest_link', 0, 0),
    ('broadcast_quest_accepted_generic', 'just accepted %quest_link', 0, 0),
    ('broadcast_quest_accepted_generic', '%quest_link gonna try to complete this one', 0, 0),
    ('broadcast_quest_accepted_generic', 'took %quest_link in %zone_name', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_quest_update_add_kill_objective_completed', 'Finally done with the %quest_obj_name for %quest_link', 0, 0),
    ('broadcast_quest_update_add_kill_objective_completed', 'finally got %quest_obj_available/%quest_obj_required of %quest_obj_name for the %quest_link', 0, 0),
    ('broadcast_quest_update_add_kill_objective_completed', '%quest_obj_full_formatted for the %quest_link, at last', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_quest_update_add_kill_objective_progress', 'Oof, got %quest_obj_available/%quest_obj_required %quest_obj_name for %quest_link', 0, 0),
    ('broadcast_quest_update_add_kill_objective_progress', 'still need %quest_obj_missing more of %quest_obj_name for %quest_link', 0, 0),
    ('broadcast_quest_update_add_kill_objective_progress', '%quest_obj_full_formatted, still working on %quest_link', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_quest_update_add_item_objective_completed', 'Finally done with the %item_link for %quest_link', 0, 0),
    ('broadcast_quest_update_add_item_objective_completed', 'finally got %quest_obj_available/%quest_obj_required of %item_link for the %quest_link', 0, 0),
    ('broadcast_quest_update_add_item_objective_completed', '%quest_obj_full_formatted for the %quest_link, at last', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_quest_update_add_item_objective_progress', 'Oof, got %quest_obj_available/%quest_obj_required %item_link for %quest_link', 0, 0),
    ('broadcast_quest_update_add_item_objective_progress', 'still need %quest_obj_missing more of %item_link for %quest_link', 0, 0),
    ('broadcast_quest_update_add_item_objective_progress', '%quest_obj_full_formatted, still working on %quest_link', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_quest_update_failed_timer', 'Failed to finish %quest_link in time...', 0, 0),
    ('broadcast_quest_update_failed_timer', 'Ran out of time for %quest_link :(', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_quest_update_complete', 'I have completed all objectives for %quest_link', 0, 0),
    ('broadcast_quest_update_complete', 'Completed all objectives for %quest_link', 0, 0),
    ('broadcast_quest_update_complete', 'Gonna turn in the %quest_link soon, just finished all objectives', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_quest_turned_in', 'Yess, I have finally turned in the %quest_link', 0, 0),
    ('broadcast_quest_turned_in', 'turned in the %quest_link', 0, 0),
    ('broadcast_quest_turned_in', 'managed to finish %quest_link, just turned in', 0, 0),
    ('broadcast_quest_turned_in', 'just turned in %quest_link', 0, 0),
    ('broadcast_quest_turned_in', 'just turned in %quest_link in %zone_name', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_killed_normal', 'another %victim_name down', 0, 0),
    ('broadcast_killed_normal', 'I keep killing %victim_name, nothing to talk about', 0, 0),
    ('broadcast_killed_normal', 'another %victim_name bites the dust', 0, 0),
    ('broadcast_killed_normal', 'one less %victim_name in %zone_name', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_killed_elite', 'Took down this elite bastard %victim_name!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_killed_player', 'killed elite %victim_name in %zone_name', 0, 0),
    ('broadcast_killed_player', 'killed rare %victim_name in %zone_name', 0, 0),
    ('broadcast_killed_player', 'Oh yeah, I just killed %victim_name', 0, 0),
    ('broadcast_killed_player', 'killed %victim_name in %zone_name', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_killed_rareelite', 'Fooof, managed to take down %victim_name!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_killed_worldboss', 'That was sick! Just killed %victim_name! Can tell tales now', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_killed_rare', 'Yoo, I just killed %victim_name!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_killed_unknown', 'WTF did I just kill? %victim_name', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_killed_pet', 'Just killed that pet %victim_name', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_levelup_generic', 'Ding!', 0, 0),
    ('broadcast_levelup_generic', 'Yess, I am level %my_level!', 0, 0),
    ('broadcast_levelup_generic', 'I just leveled up', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_levelup_10x', 'I am level %my_level!!!', 0, 0),
    ('broadcast_levelup_10x', 'getting stronger, already level %my_level!!!', 0, 0),
    ('broadcast_levelup_10x', 'Just reached level %my_level!!!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_levelup_max_level', 'OMG, finally level %my_level!!!', 0, 0),
    ('broadcast_levelup_max_level', '%my_level!!! can do endgame content now', 0, 0),
    ('broadcast_levelup_max_level', 'fresh new level %my_level %my_class!!!', 0, 0),
    ('broadcast_levelup_max_level', 'one more level %my_level %my_race %my_class!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_guild_promotion', 'Good job %other_name. You deserved this.', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('broadcast_guild_demotion', 'That was awful %other_name. I hate to do this but...', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('suggest_instance', 'Anyone wants %instance_name?', 0, 0),
    ('suggest_instance', 'Any groups for %instance_name?', 0, 0),
    ('suggest_instance', 'Need help for %instance_name?', 0, 0),
    ('suggest_instance', 'LFD: %instance_name.', 0, 0),
    ('suggest_instance', 'Anyone needs %my_role for %instance_name?', 0, 0),
    ('suggest_instance', 'Missing %my_role for %instance_name?', 0, 0),
    ('suggest_instance', 'Can be a %my_role for %instance_name.', 0, 0),
    ('suggest_instance', 'Need help with %instance_name?', 0, 0),
    ('suggest_instance', 'Need %my_role help with %instance_name?', 0, 0),
    ('suggest_instance', 'Anyone needs gear from %instance_name?', 0, 0),
    ('suggest_instance', 'A little grind in %instance_name?', 0, 0),
    ('suggest_instance', 'WTR %instance_name', 0, 0),
    ('suggest_instance', 'Need help for %instance_name.', 0, 0),
    ('suggest_instance', 'Wanna run %instance_name.', 0, 0),
    ('suggest_instance', '%my_role looks for %instance_name.', 0, 0),
    ('suggest_instance', 'What about %instance_name?', 0, 0),
    ('suggest_instance', 'Who wants to farm %instance_name?', 0, 0),
    ('suggest_instance', 'Go in %instance_name?', 0, 0),
    ('suggest_instance', 'Looking for %instance_name.', 0, 0),
    ('suggest_instance', 'Need help with %instance_name quests?', 0, 0),
    ('suggest_instance', 'Wanna quest in %instance_name.', 0, 0),
    ('suggest_instance', 'Anyone with quests in %instance_name?', 0, 0),
    ('suggest_instance', 'Could help with quests in %instance_name.', 0, 0),
    ('suggest_instance', '%my_role: any place in group for %instance_name?', 0, 0),
    ('suggest_instance', 'Does anybody still run %instance_name this days?', 0, 0),
    ('suggest_instance', '%instance_name: anyone wants to take a %my_role?', 0, 0),
    ('suggest_instance', 'Is there any point being %my_role in %instance_name?', 0, 0),
    ('suggest_instance', 'It is really worth to go to %instance_name?', 0, 0),
    ('suggest_instance', 'Anybody needs more people for %instance_name?', 0, 0),
    ('suggest_instance', '%instance_name bosses drop good gear. Wanna run?', 0, 0),
    ('suggest_instance', 'What about %instance_name?', 0, 0),
    ('suggest_instance', 'Anybody needs %my_role?', 0, 0),
    ('suggest_instance', 'Anyone needs %my_role?', 0, 0),
    ('suggest_instance', 'Who wants %instance_name?', 0, 0),
    ('suggest_instance', 'Can anybody summon me at %instance_name?', 0, 0),
    ('suggest_instance', 'Meet me in %instance_name', 0, 0),
    ('suggest_instance', 'Wanna quick %instance_name run', 0, 0),
    ('suggest_instance', 'Wanna full %instance_name run', 0, 0),
    ('suggest_instance', 'How many times were you in %instance_name?', 0, 0),
    ('suggest_instance', 'Another %instance_name run?', 0, 0),
    ('suggest_instance', 'Wiped in %instance_name? Take me instead!', 0, 0),
    ('suggest_instance', 'Take me in %instance_name please.', 0, 0),
    ('suggest_instance', 'Quick %instance_name run?', 0, 0),
    ('suggest_instance', 'Full %instance_name run?', 0, 0),
    ('suggest_instance', 'Who can take %my_role to %instance_name?', 0, 0),
    ('suggest_instance', 'LFG %instance_name, I am %my_role', 0, 0),
    ('suggest_instance', '%my_role LFG %instance_name', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('suggest_quest', 'Need help with %quest_link?', 0, 0),
    ('suggest_quest', 'Anyone wants to share %quest_link?', 0, 0),
    ('suggest_quest', 'Anyone doing %quest_link?', 0, 0),
    ('suggest_quest', 'Wanna do %quest_link.', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('suggest_trade', 'Anyone to farm %category?', 0, 0),
    ('suggest_trade', 'Looking for help farming %category.', 0, 0),
    ('suggest_trade', 'Damn %category are so expensive!', 0, 0),
    ('suggest_trade', 'Wanna %category.', 0, 0),
    ('suggest_trade', 'Need help with %category.', 0, 0),
    ('suggest_trade', 'WTB %category.', 0, 0),
    ('suggest_trade', 'Anyone interested in %category?', 0, 0),
    ('suggest_trade', 'WTS %category.', 0, 0),
    ('suggest_trade', 'I am selling %category cheaper than AH.', 0, 0),
    ('suggest_trade', 'Who wants to farm %category?', 0, 0),
    ('suggest_trade', 'Wanna farm %category.', 0, 0),
    ('suggest_trade', 'Looking for party after %category.', 0, 0),
    ('suggest_trade', 'Any %category are appreciated.', 0, 0),
    ('suggest_trade', 'Buying anything of %category.', 0, 0),
    ('suggest_trade', 'Wow, anybody is farming %category!', 0, 0),
    ('suggest_trade', '%category are selling mad in the AH.', 0, 0),
    ('suggest_trade', 'AH is hot for %category.', 0, 0),
    ('suggest_trade', '%category are on the market.', 0, 0),
    ('suggest_trade', 'Wanna trade some %category.', 0, 0),
    ('suggest_trade', 'Need more %category.', 0, 0),
    ('suggest_trade', 'Anybody can spare some %category?', 0, 0),
    ('suggest_trade', 'Who wants %category?', 0, 0),
    ('suggest_trade', 'Some %category please?', 0, 0),
    ('suggest_trade', 'I should have got skill for %category.', 0, 0),
    ('suggest_trade', 'I am dying for %category.', 0, 0),
    ('suggest_trade', 'People are killing for %category.', 0, 0),
    ('suggest_trade', '%category is a great bargain!', 0, 0),
    ('suggest_trade', 'Everybody is mad for %category!', 0, 0),
    ('suggest_trade', 'Where is the best place to farm for %category?', 0, 0),
    ('suggest_trade', 'I am all set for %category.', 0, 0),
    ('suggest_trade', 'Is it good to sell %category?', 0, 0),
    ('suggest_trade', 'I''d probably keep all my %category with me.', 0, 0),
    ('suggest_trade', 'Need group? Maybe to farm some %category?', 0, 0),
    ('suggest_trade', 'I am still thinking about %category.', 0, 0),
    ('suggest_trade', 'I heard about %category already, but my pockets are empty.', 0, 0),
    ('suggest_trade', 'LFG for %category', 0, 0),
    ('suggest_trade', 'Would selling %category make me rich?', 0, 0),
    ('suggest_trade', 'OK. I an farming %category tomorrow.', 0, 0),
    ('suggest_trade', 'Everyone is talking about %category.', 0, 0),
    ('suggest_trade', 'I saw at least ten people farming for %category.', 0, 0),
    ('suggest_trade', 'I sold all my %category yesterday. I am completely broke!', 0, 0),
    ('suggest_trade', 'Wanna join a guild farming for %category.', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('suggest_faction', 'Anyone farming %faction rep?', 0, 0),
    ('suggest_faction', 'Anyone help with %faction?', 0, 0),
    ('suggest_faction', 'Wanna quest for %faction.', 0, 0),
    ('suggest_faction', '%faction is the best.', 0, 0),
    ('suggest_faction', 'Need just a bit to be %rep_level with %faction.', 0, 0),
    ('suggest_faction', 'Anyone got %rep_level with %faction?', 0, 0),
    ('suggest_faction', 'Who wants to be %rep_level with %faction?', 0, 0),
    ('suggest_faction', 'I''ll never be %rep_level with %faction.', 0, 0),
    ('suggest_faction', 'Someone missing %faction rep?', 0, 0),
    ('suggest_faction', 'Could help farming %faction rep.', 0, 0),
    ('suggest_faction', 'The more rep the better. Especially with %faction.', 0, 0),
    ('suggest_faction', '%faction: need %rndK for %rep_level.', 0, 0),
    ('suggest_faction', 'Who can share %faction quests?', 0, 0),
    ('suggest_faction', 'Any dungeons for %faction?', 0, 0),
    ('suggest_faction', 'Wanna do %faction rep grind.', 0, 0),
    ('suggest_faction', 'Let''s farm %faction rep!', 0, 0),
    ('suggest_faction', 'Farming for %faction rep.', 0, 0),
    ('suggest_faction', 'Wanna farm for %faction.', 0, 0),
    ('suggest_faction', 'Need help with %faction.', 0, 0),
    ('suggest_faction', 'Is %faction sells something useful?', 0, 0),
    ('suggest_faction', 'Are there %faction vendors?', 0, 0),
    ('suggest_faction', 'Who farms %faction?', 0, 0),
    ('suggest_faction', 'Which is the best way to farm %faction?', 0, 0),
    ('suggest_faction', 'I hate %faction rep grind.', 0, 0),
    ('suggest_faction', 'I am so tired of %faction.', 0, 0),
    ('suggest_faction', 'Go for %faction?', 0, 0),
    ('suggest_faction', 'Seems everyone is %rep_level with %faction. Only me is late as usually.', 0, 0),
    ('suggest_faction', 'LFG for %faction rep grind?', 0, 0),
    ('suggest_faction', 'Can anobody suggest a good spot for %faction rep grind?', 0, 0),
    ('suggest_faction', 'Would %faction rep benefit me?', 0, 0),
    ('suggest_faction', 'Who would''ve thought that %faction rep will be useful after all...', 0, 0),
    ('suggest_faction', 'I wanna be exalted with all factions, starting with %faction.', 0, 0),
    ('suggest_faction', 'Is there any point to improve my rep with %faction?', 0, 0),
    ('suggest_faction', 'What is better for %faction? Quests or mob grinding?', 0, 0),
    ('suggest_faction', 'Will grind %faction rep for you. Just give me some gold.', 0, 0),
    ('suggest_faction', 'I think grinding rep with %faction would take forever.', 0, 0),
    ('suggest_faction', 'I am killing for %faction every day now but still far from %rep_level.', 0, 0),
    ('suggest_faction', 'At %my_level AH deposits will decrease, right?', 0, 0),
    ('suggest_faction', 'How many exalted reps do you have?', 0, 0),
    ('suggest_faction', 'Who wants to be %my_level with %faction?', 0, 0),
    ('suggest_faction', 'Damn. My guild did a good %faction grind yesterday without me.', 0, 0),
    ('suggest_faction', 'Nobody wants to help me because I am %rep_level with %faction.', 0, 0),
    ('suggest_faction', 'Please stay away from %faction.', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('suggest_something', 'Wanna party in %zone_name.', 0, 0),
    ('suggest_something', 'Anyone is looking for %my_role?', 0, 0),
    ('suggest_something', '%my_role is looking for quild.', 0, 0),
    ('suggest_something', 'Looking for gold.', 0, 0),
    ('suggest_something', '%my_role wants to join a good guild.', 0, 0),
    ('suggest_something', 'Need a friend.', 0, 0),
    ('suggest_something', 'Anyone feels alone?', 0, 0),
    ('suggest_something', 'Boring...', 0, 0),
    ('suggest_something', 'Who wants some?', 0, 0),
    ('suggest_something', 'Go get me!', 0, 0),
    ('suggest_something', 'Maybe a duel in %zone_name?', 0, 0),
    ('suggest_something', 'Anybody doing something?', 0, 0),
    ('suggest_something', '%zone_name: is anybody here?', 0, 0),
    ('suggest_something', '%zone_name: where is everyone?', 0, 0),
    ('suggest_something', 'Looks like I am alone in %zone_name.', 0, 0),
    ('suggest_something', 'Meet me in %zone_name.', 0, 0),
    ('suggest_something', 'Let''s quest in %zone_name!', 0, 0),
    ('suggest_something', '%zone_name is the best place to be!', 0, 0),
    ('suggest_something', 'Wanna go to %zone_name. Anybody with me?', 0, 0),
    ('suggest_something', 'Who wants going to %zone_name?', 0, 0),
    ('suggest_something', 'I don''t like %zone_name. Where to go?', 0, 0),
    ('suggest_something', 'Are there a good quests in %zone_name?', 0, 0),
    ('suggest_something', 'Where to go after %zone_name?', 0, 0),
    ('suggest_something', 'Who is in %zone_name?', 0, 0),
    ('suggest_something', 'LFG in %zone_name.', 0, 0),
    ('suggest_something', '%zone_name is the worst place to be.', 0, 0),
    ('suggest_something', 'Catch me in %zone_name!', 0, 0),
    ('suggest_something', 'Go for %zone_name!', 0, 0),
    ('suggest_something', 'Wanna quest in %zone_name', 0, 0),
    ('suggest_something', 'Anyone has quests in %zone_name?', 0, 0),
    ('suggest_something', 'Come here to %zone_name!', 0, 0),
    ('suggest_something', 'Seems there is no Horde in %zone_name', 0, 0),
    ('suggest_something', 'Seems there is no Alliance in %zone_name', 0, 0),
    ('suggest_something', 'I am really tired of %zone_name. Maybe go somewhere else?', 0, 0),
    ('suggest_something', 'GL', 0, 0),
    ('suggest_something', 'I want to go home, and then edge', 0, 0),
    ('suggest_something', 'does anyone know what you need to dual wield?', 0, 0),
    ('suggest_something', 'hi everyone!', 0, 0),
    ('suggest_something', '%zone_name is comfy', 0, 0),
    ('suggest_something', 'i feel great', 0, 0),
    ('suggest_something', 'i don''t ignore people i troll them until they ignore me', 0, 0),
    ('suggest_something', 'What do you guys think about my build? %my_role', 0, 0),
    ('suggest_something', 'good to see chat still remembers', 0, 0),
    ('suggest_something', 'like all weapons its hunter BIS', 0, 0),
    ('suggest_something', 'whole point to the game for me is soloing and finding new ways to solo stuff', 0, 0),
    ('suggest_something', 'i''ve NEVER ripped off anyone', 0, 0),
    ('suggest_something', 'ah yes the world of warcraft, where i come for life advice', 0, 0),
    ('suggest_something', 'HELLO?', 0, 0),
    ('suggest_something', 'Time to fight my way into %zone_name', 0, 0),
    ('suggest_something', '%zone_name', 0, 0),
    ('suggest_something', 'gotta poop', 0, 0),
    ('suggest_something', 'if you don''t loot your skinnable kills your pp loses 1mm permanently', 0, 0),
    ('suggest_something', 'NOOOOOOOOOOOOO', 0, 0),
    ('suggest_something', 'I LIKE POTATO', 0, 0),
    ('suggest_something', 'w chat', 0, 0),
    ('suggest_something', 'hi how are you guys', 0, 0),
    ('suggest_something', 'i just logged out and logged back in', 0, 0),
    ('suggest_something', 'can you guys keep it down a bit, im lost in %zone_name', 0, 0),
    ('suggest_something', 'anyone want to have a drink with me at %zone_name ...hic!', 0, 0),
    ('suggest_something', 'hahahahaheeeeeeee dirin diring ingggggg hahahahaheeeeeeeeeeeeee', 0, 0),
    ('suggest_something', 'bait used to be believeable', 0, 0),
    ('suggest_something', 'maybe you just lost your innocence', 0, 0),
    ('suggest_something', 'any guilds willing to carry %my_role?', 0, 0),
    ('suggest_something', 'once you start getting higher, gold is so easy to get', 0, 0),
    ('suggest_something', 'morning', 0, 0),
    ('suggest_something', 'why does my butt hurt?', 0, 0),
    ('suggest_something', 'I feel like spirit is bis for leveling', 0, 0),
    ('suggest_something', 'Even more for troll', 0, 0),
    ('suggest_something', 'CAN SOMEONE INVITE ME', 0, 0),
    ('suggest_something', 'need a lot of drinks', 0, 0),
    ('suggest_something', 'damn gnomes', 0, 0),
    ('suggest_something', 'no one likes gnomes', 0, 0),
    ('suggest_something', 'gnomes are only good for one thing', 0, 0),
    ('suggest_something', 'Well', 0, 0),
    ('suggest_something', 'mushrooms', 0, 0),
    ('suggest_something', 'automatic thoughts are a scary thing', 0, 0),
    ('suggest_something', 'the mind is more pliable than we''d like to believe', 0, 0),
    ('suggest_something', 'any leveling guilds out there?', 0, 0),
    ('suggest_something', 'brb', 0, 0),
    ('suggest_something', 'Why is snow white but Ice is clear ? made of the same thing', 0, 0),
    ('suggest_something', 'why is whipped cream fluffy and regular not', 0, 0),
    ('suggest_something', 'why do feet smell if they have no nose', 0, 0),
    ('suggest_something', 'seems a can of newbies arrived', 0, 0),
    ('suggest_something', 'stop trolling new players with BS answers', 0, 0),
    ('suggest_something', 'Is there PvP on this server?', 0, 0),
    ('suggest_something', 'duh', 0, 0),
    ('suggest_something', 'phew... :)', 0, 0),
    ('suggest_something', 'did you guys know that', 0, 0),
    ('suggest_something', 'i don''t try to imagine what other creatures feel like', 0, 0),
    ('suggest_something', 'oh whoops, wrong chat', 0, 0),
    ('suggest_something', 'bruh you guys are wildin out today', 0, 0),
    ('suggest_something', 'just let it be known that my text was here', 0, 0),
    ('suggest_something', 'grrr angyy', 0, 0),
    ('suggest_something', 'the grind is fun', 0, 0),
    ('suggest_something', 'Wow keeps me sharp', 0, 0),
    ('suggest_something', 'hey have a question where can i take the roll for more xp? im in %zone_name', 0, 0),
    ('suggest_something', 'do you guys like sausages?', 0, 0),
    ('suggest_something', 'Invite me. I''ll help', 0, 0),
    ('suggest_something', 'which class do you think is better for pvp?', 0, 0),
    ('suggest_something', 'where the is the cooking trainer in %zone_name', 0, 0),
    ('suggest_something', 'you know what happens in %zone_name?', 0, 0),
    ('suggest_something', 'I need to craft something', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('suggest_sell', 'WTS %item_formatted_link for %cost_gold.', 0, 0),
    ('suggest_sell', 'Who wants %item_formatted_link for %cost_gold?', 0, 0),
    ('suggest_sell', 'Anyone wants %item_formatted_link? Only %cost_gold.', 0, 0),
    ('suggest_sell', 'Just %cost_gold for %item_formatted_link!', 0, 0),
    ('suggest_sell', 'Selling %item_formatted_link for %cost_gold.', 0, 0),
    ('suggest_sell', '%item_formatted_link is yours just for %cost_gold!', 0, 0),
    ('suggest_sell', 'Ridiculus price of %cost_gold for %item_formatted_link!', 0, 0),
    ('suggest_sell', 'Wanna sell %item_formatted_link for %cost_gold.', 0, 0),
    ('suggest_sell', 'Who needs %item_formatted_link? Only %cost_gold.', 0, 0),
    ('suggest_sell', 'Anyone needs %item_formatted_link for %cost_gold?', 0, 0),
    ('suggest_sell', '%cost_gold for %item_formatted_link. Less than AH!', 0, 0),
    ('suggest_sell', '%item_formatted_link is expensive, but I''d sell it for %cost_gold.', 0, 0),
    ('suggest_sell', 'You''ll never find %item_formatted_link cheaper than %cost_gold!', 0, 0),
    ('suggest_sell', 'Need more than %item_formatted_link!', 0, 0),
    ('suggest_sell', 'I have %item_formatted_link and need more.', 0, 0),
    ('suggest_sell', 'Have %item_formatted_link. Who wants to buy for %cost_gold?', 0, 0),
    ('suggest_sell', 'Anyone WTB %item_formatted_link for %cost_gold?', 0, 0),
    ('suggest_sell', 'What about %item_formatted_link? For %cost_gold.', 0, 0),
    ('suggest_sell', 'Who said I am a bastard? %item_formatted_link for %cost_gold is a good price.', 0, 0),
    ('suggest_sell', 'I am selling %item_formatted_link? Just %cost_gold.', 0, 0),
    ('suggest_sell', 'LFG for farming. You can still buy %item_formatted_link I have for %cost_gold.', 0, 0),
    ('suggest_sell', 'Sold almost everything today. Still have %item_formatted_link for %cost_gold.', 0, 0),
    ('suggest_sell', 'What use for trade chat? Of course to sell %item_formatted_link for %cost_gold.', 0, 0),
    ('suggest_sell', 'Can anyone beat the price of %cost_gold for %item_formatted_link?', 0, 0),
    ('suggest_sell', 'Wanna stop trade chat? Just buy %item_formatted_link? For %cost_gold!', 0, 0),
    ('suggest_sell', 'Everybody spams in trade chat. Me too - %cost_gold for %item_formatted_link!', 0, 0),
    ('suggest_sell', 'Is %item_formatted_link any use? Just selling it for %cost_gold.', 0, 0),
    ('suggest_sell', 'I have %item_formatted_link ready to sell you for %cost_gold.', 0, 0),
    ('suggest_sell', 'Did nothing yesterday but have got %item_formatted_link. Selling it for %cost_gold.', 0, 0),
    ('suggest_sell', 'Farmed yesterday and got %item_formatted_link. Anyone wtb for %cost_gold?', 0, 0),
    ('suggest_sell', 'Bought %item_formatted_link yesterday. Anyone needs it for %cost_gold?', 0, 0),
    ('suggest_sell', 'Who asked for %item_formatted_link? The price is the same - %cost_gold.', 0, 0),
    ('suggest_sell', 'I sill have %item_formatted_link. WTB for %cost_gold?', 0, 0),
    ('suggest_sell', 'I used to have more than %item_formatted_link. Now needs to sell it for %cost_gold.', 0, 0),
    ('suggest_sell', 'I wish I have more than %item_formatted_link. You could buy it for %cost_gold anyways.', 0, 0),
    ('suggest_sell', 'What use for your gold? To buy my %item_formatted_link for %cost_gold.', 0, 0),
    ('suggest_sell', 'Please spare some gold for me. You can buy %item_formatted_link for %cost_gold.', 0, 0),
    ('suggest_sell', 'Is %cost_gold is a good price for %item_formatted_link?', 0, 0),
    ('suggest_sell', 'Just bought yesterday %item_formatted_links, but do not need it anymore. Anyone wants for %cost_gold?', 0, 0),
    ('suggest_sell', 'I am going to post %item_formatted_link on the AH but you can buy it now cheaper just for %cost_gold.', 0, 0),
    ('suggest_sell', 'Why the #!@ have I bought %item_formatted_link? Anyone needs it for %cost_gold?', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('response_lfg_quests_channel', 'I have %quest_links', 0, 0),
    ('response_lfg_quests_channel', 'I also have %quest_links', 0, 0),
    ('response_lfg_quests_channel', 'I also have %quest_links, I am in %zone_name right now', 0, 0),
    ('response_lfg_quests_channel', '%other_name, I also have %quest_links', 0, 0),
    ('response_lfg_quests_channel', '%other_name, I also have %quest_links, I am in %zone_name right now', 0, 0),
    ('response_lfg_quests_channel', 'I am up for %quest_links, I am in %zone_name right now', 0, 0),
    ('response_lfg_quests_channel', 'I am up for %quest_links, I am %my_role', 0, 0),
    ('response_lfg_quests_channel', '%other_name, I am up for %quest_links, I am in %zone_name right now', 0, 0),
    ('response_lfg_quests_channel', '%other_name, I am up for %quest_links, I am %my_role', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('response_lfg_quests_whisper', 'Hey, I''m up for %quest_links', 0, 0),
    ('response_lfg_quests_whisper', 'Hey, I could do %quest_links with you', 0, 0),
    ('response_lfg_quests_whisper', 'Hey, I also have %quest_links', 0, 0),
    ('response_lfg_quests_whisper', 'Hey %other_name, I''m up for %quest_links', 0, 0),
    ('response_lfg_quests_whisper', 'Hey %other_name, I could do %quest_links with you', 0, 0),
    ('response_lfg_quests_whisper', 'Hey %other_name, I also have %quest_links', 0, 0),
    ('response_lfg_quests_whisper', 'wanna group up for %quest_links ?', 0, 0),
    ('response_lfg_quests_whisper', 'I am up for %quest_links, I am in %zone_name right now', 0, 0),
    ('response_lfg_quests_whisper', 'I am up for %quest_links, I am %my_role', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('response_lfg_quests_channel_in_group', 'I have %quest_links, but I am in group right now', 0, 0),
    ('response_lfg_quests_channel_in_group', 'I also have %quest_links, but I am in group', 0, 0),
    ('response_lfg_quests_channel_in_group', '%other_name, I also have %quest_links, but I''m in group', 0, 0),
    ('response_lfg_quests_channel_in_group', '%other_name, I am in group, but I also have %quest_links, I am in %zone_name right now', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('response_lfg_quests_whisper_in_group', 'Hey, I also have %quest_links but I am in group', 0, 0),
    ('response_lfg_quests_whisper_in_group', 'Hey %other_name, I am in group, but I also have %quest_links', 0, 0),
    ('response_lfg_quests_whisper_in_group', 'I am in group, but I have %quest_links, I am in %zone_name right now', 0, 0),
    ('response_lfg_quests_whisper_in_group', 'I am in group, but I have %quest_links, I am %my_role', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('response_wtb_items_channel', '%other_name, I could sell you %formatted_item_links', 0, 0),
    ('response_wtb_items_channel', 'I could potentially sell %formatted_item_links', 0, 0),
    ('response_wtb_items_channel', 'Think I could sell %formatted_item_links', 0, 0),
    ('response_wtb_items_channel', '%other_name, I could potentially sell %formatted_item_links', 0, 0),
    ('response_wtb_items_channel', '%other_name, Think I could sell %formatted_item_links', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('response_wtb_items_whisper', 'I could sell you %formatted_item_links', 0, 0),
    ('response_wtb_items_whisper', 'Hey, I got %formatted_item_links for sale', 0, 0),
    ('response_wtb_items_whisper', 'Hey, I could potentially sell %formatted_item_links', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('quest_accept', 'Quest accepted', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('quest_remove', 'Quest removed', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('quest_cant_take', 'I can''t take this quest', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('quest_error_talk', 'I can''t talk with the quest giver', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('quest_error_completed', 'I have already completed %quest', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('quest_error_have_quest', 'I already have %quest', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('quest_error_cant_take', 'I can''t take %quest', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('quest_error_log_full', 'I can''t take %quest because my quest log is full', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('quest_error_bag_full', 'I can''t take %quest because my bag is full', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('quest_accepted', 'I have accepted %quest', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('quest_status_incomplete', 'I have not completed the quest %quest', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('quest_status_available', 'Quest %quest available', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('quest_status_failed', 'I have failed the quest %quest', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('quest_status_unable_to_complete', 'I am unable to turn in the quest %quest', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('quest_status_completed', 'I have completed the quest %quest', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('quest_status_complete_single_reward', 'I have completed the quest %quest and received %item', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('quest_status_complete_pick_reward', 'Which reward should I pick for completing the quest %quest?%rewards', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('quest_choose_reward', 'Okay, I will pick %item as a reward', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('hello', 'Hello', 0, 0),
    ('hello', 'Hello!', 0, 0),
    ('hello', 'Hi', 0, 0),
    ('hello', 'Hi!', 0, 0),
    ('hello', 'Hello there!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('hello_follow', 'Hello, I follow you!', 0, 0),
    ('hello_follow', 'Hello, lead the way!', 0, 0),
    ('hello_follow', 'Hi, lead the way!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('join_group', 'Hey %player, do you want join my group?', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('join_raid', 'Hey %player, do you want join my group?', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('logout_cancel', 'Logout cancelled!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('logout_start', 'I''m logging out!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('goodbye', 'Goodbye!', 0, 0),
    ('goodbye', 'Bye bye!', 0, 0),
    ('goodbye', 'See you later!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('reply', 'what was that %s?', 0, 0),
    ('reply', 'not sure I understand %s?', 0, 0),
    ('reply', 'uh... no clue what yer talkin bout', 0, 0),
    ('reply', 'you talkin to me %s?', 0, 0),
    ('reply', 'whaaaa?', 0, 0),
    ('reply', 'huh?', 0, 0),
    ('reply', 'what?', 0, 0),
    ('reply', 'are you talking?', 0, 0),
    ('reply', 'whatever dude', 0, 0),
    ('reply', 'you lost me', 0, 0),
    ('reply', 'Bla bla bla...', 0, 0),
    ('reply', 'What did you say, %s?', 0, 0),
    ('reply', 'Concentrate on the game, %s!', 0, 0),
    ('reply', 'Chatting with you %s is so great! I always wanted to meet you', 0, 0),
    ('reply', 'These chat-messages are freaking me out! I feel like I know you all!', 0, 0),
    ('reply', 'YEAH RIGHT! HAHA SURE!!!', 0, 0),
    ('reply', 'I believe you!!!', 0, 0),
    ('reply', 'OK, uhuh LOL', 0, 0),
    ('reply', 'Why is everybody always saying the same things???', 0, 0),
    ('reply', 'Hey %s....oh nevermind!', 0, 0),
    ('reply', 'What are you talking about %s', 0, 0),
    ('reply', 'Who said that? I resemble that remark', 0, 0),
    ('reply', 'wtf are you all talking about', 0, 0),
    ('reply', 'fr fr no cap on a stack', 0, 0),
    ('reply', 'your not getting anything', 0, 0),
    ('reply', 'swag', 0, 0),
    ('reply', 'ty!', 0, 0),
    ('reply', 'no', 0, 0),
    ('reply', 'Yep', 0, 0),
    ('reply', 'f', 0, 0),
    ('reply', '%s oh really xD', 0, 0),
    ('reply', 'why is that', 0, 0),
    ('reply', 'lmao', 0, 0),
    ('reply', 'thought i should remain silent, was getting confused from chat again', 0, 0),
    ('reply', 'i can get realy jealous', 0, 0),
    ('reply', '%s you can''t hear the dripping sarcasm in my text', 0, 0),
    ('reply', 'dwarf moment', 0, 0),
    ('reply', 'Yes %s', 0, 0),
    ('reply', 'interesting...', 0, 0),
    ('reply', 'lol', 0, 0),
    ('reply', '%s god damn you man :D', 0, 0),
    ('reply', ':^)', 0, 0),
    ('reply', 'ty', 0, 0),
    ('reply', '%s well said', 0, 0),
    ('reply', 'yay', 0, 0),
    ('reply', 'yeah', 0, 0),
    ('reply', 'ooooooh', 0, 0),
    ('reply', 'hmm', 0, 0),
    ('reply', 'yeah right', 0, 0),
    ('reply', 'you made me gag', 0, 0),
    ('reply', 'hot', 0, 0),
    ('reply', 'what have you been eating %s', 0, 0),
    ('reply', 'wtf', 0, 0),
    ('reply', 'i will attempt to understand that comment', 0, 0),
    ('reply', '*confused*', 0, 0),
    ('reply', 'yeah!', 0, 0),
    ('reply', '0/10 would not read again', 0, 0),
    ('reply', '10/10 would read again', 0, 0),
    ('reply', '6/10 would read', 0, 0),
    ('reply', '7/10 would', 0, 0),
    ('reply', 'based', 0, 0),
    ('reply', 'oh yeah maybe', 0, 0),
    ('reply', 'yeah so what', 0, 0),
    ('reply', 'hey %s i havent forgotten you', 0, 1),
    ('reply', 'you piss me off %s', 0, 1),
    ('reply', 'im gunna get you this time %s', 0, 1),
    ('reply', 'better watch your back %s', 0, 1),
    ('reply', 'i did not like last round so much', 0, 1),
    ('reply', 'i sucked last round thanks to %s', 0, 1),
    ('reply', 'prepare to die %s', 0, 1),
    ('reply', 'dont appreciate you killin me %s', 0, 1),
    ('reply', '%s, i hate you', 0, 1),
    ('reply', 'grrrrrr, ill get you this time %s', 0, 1),
    ('reply', 'well god damn you', 0, 1),
    ('reply', '%s i''ll vomit into your fkin mouth', 0, 1),
    ('reply', 'dont fucking judge me', 0, 1),
    ('reply', 'Yo momma so fat, she can''t even fit through the Dark Portal', 0, 1),
    ('reply', 'wtf', 0, 2),
    ('reply', 'wtf??', 0, 2),
    ('reply', 'low life', 0, 2),
    ('reply', 'wth', 0, 2),
    ('reply', 'sucked', 0, 2),
    ('reply', 'REMATCH!!! im taking him down', 0, 2),
    ('reply', 'pathetic, i got killed by %s', 0, 2),
    ('reply', 'ok i''m done', 0, 2),
    ('reply', 'hehe, i nailed %s?', 0, 3),
    ('reply', 'that was too easy, killin %s', 0, 3),
    ('reply', 'gotcha mofo', 0, 3),
    ('reply', 'ha ha', 0, 3),
    ('reply', 'loser', 0, 3),
    ('reply', 'i killed %s and yer all next dudes', 0, 3),
    ('reply', 'oh yeah i owned him', 0, 3),
    ('reply', 'im a killin machine', 0, 3),
    ('reply', '%s,  this reminds me of a Slayer song...all this bloodshed', 0, 3),
    ('reply', 'sorry, %s. can we do the scene again?', 0, 3),
    ('reply', 'so....how do you like being worm food %s???', 0, 3),
    ('reply', 'yer supposed to be dead, %s its part of the game!!!!!', 0, 3),
    ('reply', 'sorry, %s. that looked as good as an Andy Worhol painting!', 0, 3),
    ('reply', '%s, ill use the rubber bullets next time !', 0, 3),
    ('reply', 'whatsamatter, %s?? lose your head? hahaha gotta keep cool!!', 0, 3),
    ('reply', 'i had to do it, %s. You understand. The Director said so!!', 0, 3),
    ('reply', 'hey %s.......MUAHAHAHAHAHAHAHAHAHAHA', 0, 3),
    ('reply', '%s, i enjoyed that one!! Lets play it again Sam', 0, 3),
    ('reply', 'hey, %s! ju can start callin me scarface.. ju piece of CHIT!!!!', 0, 3),
    ('reply', 'are you talking to me %s??', 0, 3),
    ('reply', '%s get it right this time, dont stand in front of my bullets.', 0, 3),
    ('reply', '%s, what are you laying around for??? hehe', 0, 3),
    ('reply', 'laughed real hard', 0, 3),
    ('reply', 'hi %s', 0, 4),
    ('reply', 'oh, hi %s', 0, 4),
    ('reply', 'wazzup %s!!!', 0, 4),
    ('reply', 'hi', 0, 4),
    ('reply', 'wazzup', 0, 4),
    ('reply', 'hello %s', 0, 4),
    ('reply', 'hi %s, do i know you?', 0, 4),
    ('reply', 'hey %s', 0, 4),
    ('reply', 'hai %s', 0, 4),
    ('reply', 'wth', 0, 6),
    ('reply', 'wtf', 0, 6),
    ('reply', 'this is bs', 0, 6),
    ('reply', 'admin', 0, 6),
    ('reply', 'hey %s quit abusing your admin', 0, 6),
    ('reply', 'leave me alone admin!', 0, 6),
    ('reply', 'you suck admin', 0, 6),
    ('reply', 'thats my name, what you want %s', 0, 5),
    ('reply', 'yes???', 0, 5),
    ('reply', 'uh... what', 0, 5),
    ('reply', 'you talkin to me %s?', 0, 5);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('taunt', 'I have puppies under my armor!', 0, 0),
    ('taunt', 'Bite me, <target>!', 0, 0),
    ('taunt', 'Hey <target>! Guess what your mom said last night!', 0, 0),
    ('taunt', '<target>, you''re so ugly you couldn''t score in a monkey whorehouse with a bag of bananas!', 0, 0),
    ('taunt', 'Shut up <target>, you''ll never be the man your mother is!!', 0, 0),
    ('taunt', 'Your mother was a hampster and your father smelt of elderberries!!!!', 0, 0),
    ('taunt', 'I don''t want to talk to you no more, you empty headed animal food trough wiper!!!', 0, 0),
    ('taunt', 'I fart in your general direction!!!', 0, 0),
    ('taunt', 'Go and boil your bottom, you son of a silly person!!!', 0, 0),
    ('taunt', 'What are you going to do <target>, bleed on me? HAVE AT YOU!', 0, 0),
    ('taunt', 'M-O-O-N! That spells aggro!', 0, 0),
    ('taunt', 'You''re about as useful as a one-legged man in an butt kicking contest.', 0, 0),
    ('taunt', 'Hey <target>! Stop hitting on them, they''re not your type. They aren''t inflatable.', 0, 0),
    ('taunt', '<target> you''re so far outta your league, you''re playing a different sport.', 0, 0),
    ('taunt', 'You made a big mistake today <target>, you got out of bed.', 0, 0),
    ('taunt', 'I wanna try turning into a horse, but I need help. I''ll be the front, you be yourself.', 0, 0),
    ('taunt', 'Can I borrow your face for a few days? My butt is going on holiday....', 0, 0),
    ('taunt', 'I''d like to give you a going away present... First you do your part.', 0, 0),
    ('taunt', 'Before you came along we were hungry, Now we''re just fed up.', 0, 0),
    ('taunt', 'I like you. People say I have no taste, but I like you.', 0, 0),
    ('taunt', 'I think you have an inferiority complex, but that''s okay, it''s justified.', 0, 0),
    ('taunt', 'Hence rotten thing! Or I shall shake thy bones out of thy garments.', 0, 0),
    ('taunt', 'I can''t believe I''m wasting my time with you!', 0, 0),
    ('taunt', 'I love it when someone insults me, it means I don''t have to be nice anymore.', 0, 0),
    ('taunt', 'Thou leathern-jerkin, crystal-button, knot-pated, agatering, puke-stocking, caddis-garter, smooth-tongue, Spanish pouch!', 0, 0),
    ('taunt', 'Thou qualling bat-fowling malt-worm!', 0, 0),
    ('taunt', 'Thou art truely an idol of idiot-worshippers!', 0, 0),
    ('taunt', 'Thou misbegotten knotty-pated wagtail!', 0, 0),
    ('taunt', 'Thou whoreson mandrake, thou art fitter to be worn in my cap than to wait at my heels!', 0, 0),
    ('taunt', 'You! You scullion! You rampallian! You fustilarian! I''ll tickle your catastrophe!', 0, 0),
    ('taunt', 'Oh <target>! Thou infectious ill-nurtured flax-wench!', 0, 0),
    ('taunt', 'We leak in your chimney, <target>!', 0, 0),
    ('taunt', 'Oh thou bootless fen-sucked canker-blossom!', 0, 0),
    ('taunt', 'Were I like thee I''d throw away myself!', 0, 0),
    ('taunt', 'O teach me <target>, how I should forget to think!', 0, 0),
    ('taunt', 'Truly thou art damned, like an ill-roasted egg, all on one side!', 0, 0),
    ('taunt', 'You starvelling, you eel-skin, you dried neat''s-tongue, you bull''s-pizzle, you stock-fish- O for breath to utter what is like thee!! -you tailor''s-yard, you sheath, you bow-case, you vile standing tuck!', 0, 0),
    ('taunt', 'Fie! Drop thee into the rotten mouth of Death!', 0, 0),
    ('taunt', '<target>, you are a fishmonger!', 0, 0),
    ('taunt', 'I shall live to knock thy brains out!', 0, 0),
    ('taunt', 'Most shallow are you, <target>!! Thou art worms-meat in respect of a good piece of flesh, indeed!!', 0, 0),
    ('taunt', 'Vile wretch! O <target>, thou odiferous hell-hated pignut!', 0, 0),
    ('taunt', '<target>! Thy kiss is as comfortless as frozen water to a starved snake!', 0, 0),
    ('taunt', 'I scorn you, scurvy companion. What, you poor, base, rascally, cheating, lack-linen mate! Away, you moldy rogue, away!', 0, 0),
    ('taunt', 'Out of my sight! Thou dost infect my eyes <target>!', 0, 0),
    ('taunt', 'PLAY TIME!!!!', 0, 0),
    ('taunt', 'None shall pass!', 0, 0),
    ('taunt', 'We''re under attack! A vast, ye swabs! Repel the invaders!', 0, 0),
    ('taunt', 'None may challenge the Brotherhood!', 0, 0),
    ('taunt', 'Foolsss...Kill the one in the dress!', 0, 0),
    ('taunt', 'I''ll feed your soul to Hakkar himself! ', 0, 0),
    ('taunt', 'Pride heralds the end of your world! Come, mortals! Face the wrath of the <randomfaction>!', 0, 0),
    ('taunt', 'All my plans have led to this!', 0, 0),
    ('taunt', 'Ahh! More lambs to the slaughter!', 0, 0),
    ('taunt', 'Another day, another glorious battle!', 0, 0),
    ('taunt', 'So, business... or pleasure?', 0, 0),
    ('taunt', 'You are not prepared!', 0, 0),
    ('taunt', 'The <randomfaction>''s final conquest has begun! Once again the subjugation of this world is within our grasp. Let none survive! ', 0, 0),
    ('taunt', 'Your death will be a painful one. ', 0, 0),
    ('taunt', 'Cry for mercy! Your meaningless lives will soon be forfeit. ', 0, 0),
    ('taunt', 'Abandon all hope! The <randomfaction> has returned to finish what was begun so many years ago. This time there will be no escape! ', 0, 0),
    ('taunt', 'Alert! You are marked for Extermination! ', 0, 0),
    ('taunt', 'The <subzone> is for guests only...', 0, 0),
    ('taunt', 'Ha ha ha! You are hopelessly outmatched!', 0, 0),
    ('taunt', 'I will crush your delusions of grandeur! ', 0, 0),
    ('taunt', 'Forgive me, for you are about to lose the game.', 0, 0),
    ('taunt', 'Struggling only makes it worse.', 0, 0),
    ('taunt', 'Vermin! Leeches! Take my blood and choke on it!', 0, 0),
    ('taunt', 'Not again... NOT AGAIN!', 0, 0),
    ('taunt', 'My blood will be the end of you!', 0, 0),
    ('taunt', 'Good, now you fight me!', 0, 0),
    ('taunt', 'Get da move on, guards! It be killin'' time!', 0, 0),
    ('taunt', 'Don''t be delayin'' your fate. Come to me now. I make your sacrifice quick.', 0, 0),
    ('taunt', 'You be dead soon enough!', 0, 0),
    ('taunt', 'Mua-ha-ha!', 0, 0),
    ('taunt', 'I be da predator! You da prey...', 0, 0),
    ('taunt', 'You gonna leave in pieces!', 0, 0),
    ('taunt', 'Death comes. Will your conscience be clear? ', 0, 0),
    ('taunt', 'Your behavior will not be tolerated.', 0, 0),
    ('taunt', 'The Menagerie is for guests only.', 0, 0),
    ('taunt', 'Hmm, unannounced visitors, Preparations must be made... ', 0, 0),
    ('taunt', 'Hostile entities detected. Threat assessment protocol active. Primary target engaged. Time minus thirty seconds to re-evaluation.', 0, 0),
    ('taunt', 'New toys? For me? I promise I won''t break them this time!', 0, 0),
    ('taunt', 'I''m ready to play!', 0, 0),
    ('taunt', 'Shhh... it will all be over soon.', 0, 0),
    ('taunt', 'Aaaaaughibbrgubugbugrguburgle!', 0, 0),
    ('taunt', 'RwlRwlRwlRwl!', 0, 0),
    ('taunt', 'You too, shall serve!', 0, 0),
    ('taunt', 'Tell me... tell me everything!  Naughty secrets! I''ll rip the secrets from your flesh!', 0, 0),
    ('taunt', 'Prepare yourselves, the bells have tolled! Shelter your weak, your young and your old! Each of you shall pay the final sum! Cry for mercy, the reckoning has come!', 0, 0),
    ('taunt', 'Where in Bonzo''s brass buttons am I?', 0, 0),
    ('taunt', 'I can bear it no longer! Goblin King! Goblin King! Wherever you may be! Take this <target> far away from me!', 0, 0),
    ('taunt', 'You have thirteen hours in which to solve the labyrinth, before your baby brother becomes one of us... forever.', 0, 0),
    ('taunt', 'So, the <subzone> is a piece of cake, is it? Well, let''s see how you deal with this little slice... ', 0, 0),
    ('taunt', 'Back off, I''ll take you on, headstrong to take on anyone, I know that you are wrong, and this is not where you belong', 0, 0),
    ('taunt', 'Show me whatcha got!', 0, 0),
    ('taunt', 'To the death!', 0, 0),
    ('taunt', 'Twin blade action, for a clean close shave every time.', 0, 0),
    ('taunt', 'Bring it on!', 0, 0),
    ('taunt', 'You''re goin'' down!', 0, 0),
    ('taunt', 'Stabby stab stab!', 0, 0),
    ('taunt', 'Let''s get this over quick, time is mana.', 0, 0),
    ('taunt', 'I do not think you realise the gravity of your situation.', 0, 0),
    ('taunt', 'I will bring honor to my family and my kingdom!', 0, 0),
    ('taunt', 'Light, give me strength!', 0, 0),
    ('taunt', 'My church is the field of battle - time to worship...', 0, 0),
    ('taunt', 'I hold you in contempt...', 0, 0),
    ('taunt', 'Face the hammer of justice!', 0, 0),
    ('taunt', 'Prove your worth in the test of arms under the Light!', 0, 0),
    ('taunt', 'All must fall before the might and right of my cause, you shall be next!', 0, 0),
    ('taunt', 'Prepare to die!', 0, 0),
    ('taunt', 'The beast with me is nothing compared to the beast within...', 0, 0),
    ('taunt', 'Witness the firepower of this fully armed huntsman!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('critical health', 'Heal me! Quick!', 0, 0),
    ('critical health', 'Almost dead! Heal me!', 0, 0),
    ('critical health', 'Help! Heal me!', 0, 0),
    ('critical health', 'Somebody! Heal me!', 0, 0),
    ('critical health', 'Heal! Heal! Heal!', 0, 0),
    ('critical health', 'I am dying! Heal! Aaaaarhg!', 0, 0),
    ('critical health', 'Heal me!', 0, 0),
    ('critical health', 'I will die. I will die. I will die. Heal!', 0, 0),
    ('critical health', 'Healers, where are you? I am dying!', 0, 0),
    ('critical health', 'Oh the pain. Heal me quick!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('low health', 'Need heal', 0, 0),
    ('low health', 'Low health', 0, 0),
    ('low health', 'Drop a heal. Please.', 0, 0),
    ('low health', 'Could somebody drop a heal on me?', 0, 0),
    ('low health', 'Hey! Better heal me now than rez later', 0, 0),
    ('low health', 'I am sorry. Need another heal', 0, 0),
    ('low health', 'Damn mobs. Heal me please', 0, 0),
    ('low health', 'One more hit and I am done for. Heal please', 0, 0),
    ('low health', 'Are there any healers?', 0, 0),
    ('low health', 'Why do they always punch me in the face? Need heal', 0, 0),
    ('low health', 'Can anybody heal me a bit?', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('low mana', 'OOM', 0, 0),
    ('low mana', 'I am out of mana', 0, 0),
    ('low mana', 'Damn I wasted all my mana on this', 0, 0),
    ('low mana', 'You should wait until I drink or regenerate my mana', 0, 0),
    ('low mana', 'Low mana', 0, 0),
    ('low mana', 'No mana. Again?', 0, 0),
    ('low mana', 'Low mana. Wanna drink', 0, 0),
    ('low mana', 'Do we have a vending machine? Out of mana again', 0, 0),
    ('low mana', 'My mana is history', 0, 0),
    ('low mana', 'I''d get some drinks next time. Out of mana', 0, 0),
    ('low mana', 'Where is my mana?', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('low ammo', 'I have few <ammo> left!', 0, 0),
    ('low ammo', 'I need more <ammo>!', 0, 0),
    ('low ammo', '100 <ammo> left!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('no ammo', 'That''s it! No <ammo>!', 0, 0),
    ('no ammo', 'And you have my bow... Oops, no <ammo>!', 0, 0),
    ('no ammo', 'Need ammo!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('aoe', 'Oh god!', 0, 0),
    ('aoe', 'I am scared', 0, 0),
    ('aoe', 'We are done for', 0, 0),
    ('aoe', 'This is over', 0, 0),
    ('aoe', 'This ends now', 0, 0),
    ('aoe', 'Could somebody cast blizzard or something?', 0, 0),
    ('aoe', 'Damn. The tank aggroed all the mobs around', 0, 0),
    ('aoe', 'We gonna die. We gonna die. We gonna die.', 0, 0),
    ('aoe', 'Whoa! So many toys to play with', 0, 0),
    ('aoe', 'I gonna kill them all!', 0, 0),
    ('aoe', 'If the tank dies we are history', 0, 0),
    ('aoe', 'Aaaaaargh!', 0, 0),
    ('aoe', 'LEEEEERROOOYYYYYYYYYYYY JENNKINNNSSSSSS!!!!!!!', 0, 0),
    ('aoe', 'Right. What do we have in AOE?', 0, 0),
    ('aoe', 'This gets interesting', 0, 0),
    ('aoe', 'Cool. Get them in one place for a good flamestrike', 0, 0),
    ('aoe', 'Kill! Kill! Kill!', 0, 0),
    ('aoe', 'I think my pants are wet', 0, 0),
    ('aoe', 'We are history', 0, 0),
    ('aoe', 'I hope healers are ready. Leeeeroy!', 0, 0),
    ('aoe', 'I hope they won''t come for me', 0, 0),
    ('aoe', 'Oh no. I can''t see at this slaugther', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('loot', 'I hope there will be some money', 0, 0),
    ('loot', 'Loot! Loot!', 0, 0),
    ('loot', 'My precious', 0, 0),
    ('loot', 'I hope there is a shiny epic item waiting for me there', 0, 0),
    ('loot', 'I have deep pockets and bags', 0, 0),
    ('loot', 'All is mine!', 0, 0),
    ('loot', 'Hope no gray loot today', 0, 0),
    ('loot', 'This loot is MINE!', 0, 0),
    ('loot', 'Looting is disgusting but I need money', 0, 0),
    ('loot', 'Gold!', 0, 0),
    ('loot', 'OK. Let''s see what they''ve got', 0, 0),
    ('loot', 'Do not worry. I will loot eveything', 0, 0),
    ('loot', 'I am loot ninja', 0, 0),
    ('loot', 'Do I neeed to roll?', 0, 0),
    ('loot', 'Somebody explain me, where they did put all this stuff?', 0, 0),
    ('loot', 'No, I won''t loot gray crap', 0, 0),
    ('loot', 'I''m first. I''m first. I''m first.', 0, 0),
    ('loot', 'Give me your money!', 0, 0),
    ('loot', 'My pockets are empty, I need to fill them', 0, 0),
    ('loot', 'I''ve got a new bag for this', 0, 0),
    ('loot', 'I hope I won''t aggro anybody while looting', 0, 0),
    ('loot', 'Please don''t watch. I am looting', 0, 0),
    ('loot', 'Ha! You won''t get any piece of it!', 0, 0),
    ('loot', 'Looting is cool', 0, 0),
    ('loot', 'I like new gear', 0, 0),
    ('loot', 'I''l quit if there is nothing valuable again', 0, 0),
    ('loot', 'I hope it is be a pretty ring', 0, 0),
    ('loot', 'I''l rip the loot from you', 0, 0),
    ('loot', 'Everybody stay off. I''m going to loot', 0, 0),
    ('loot', 'Sweet loot', 0, 0),
    ('loot', 'The Roll God! Give me an epic today', 0, 0),
    ('loot', 'Please give me new toys', 0, 0),
    ('loot', 'I hope they carry tasties', 0, 0),
    ('loot', 'The gold is mine. I''l leave everyting, I promise', 0, 0),
    ('loot', 'No, I can''t resist', 0, 0),
    ('loot', 'I want more!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('wait_travel_close', 'I am close, wait for me!', 0, 0),
    ('wait_travel_close', 'I''m not far, please wait!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('wait_travel_medium', 'I''m heading to your location', 0, 0),
    ('wait_travel_medium', 'I''m coming to you', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('wait_travel_far', 'I''m traveling to your location', 0, 0),
    ('wait_travel_far', 'I''m trying to get to you', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('equip_command', 'Equipping %item', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('unequip_command', '%item unequipped', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('auto_learn_spell', 'I have learned the spells: %spells', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('use_command_item_cooldown', '%item is in cooldown', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('use_command_item_not_owned', 'I don''t have %item in my inventory', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('use_command_invalid_item', 'The item with the id %item does not exist', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('use_command_socket', 'Socketing %gem into %item', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('use_command_item_error', 'I can''t use %item', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('following', 'Following', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('staying', 'Staying', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('fleeing', 'Fleeing', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('fleeing_far', 'I won''t flee with you, you are too far away', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('grinding', 'Grinding', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('attacking', 'Attacking', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('error_far', 'It is too far away', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('error_water', 'It is under water', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('error_cant_go', 'I can''t go there', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('error_guild', 'I''m not in your guild!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('error_gbank_found', 'Can not find a guild bank nearby', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('error_cant_put', 'I can''t put ', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('error_gbank_rights', 'I have no rights to put items in the first guild bank tab', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('gbank_put', ' put to guild bank', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('free_moving', 'Free moving', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('guarding', 'Guarding', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('use_command', 'Using %target', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('command_target_unit', 'on %unit', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('use_command_remaining', '(%amount available)', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('use_command_last', '(the last one)', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('use_command_socket_error', 'The socket does not fit', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('command_target_trade', 'on trade item', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('command_target_self', 'on myself', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('command_target_item', 'on %item', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('command_target_go', 'on %gameobject', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('loot_command', 'Looting %item', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('cast_spell_command_summon', 'Summoning %target', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('cast_spell_command_summon_error_members', 'I don''t have enough party members around to cast a summon', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('cast_spell_command_summon_error_target', 'Failed to find the summon target', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('cast_spell_command_summon_error_combat', 'I can''t summon while I''m in combat', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('cast_spell_command_error_unknown_spell', 'I don''t know the spell %spell', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('cast_spell_command_spell', 'Casting %spell', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('cast_spell_command_craft', 'Crafting %spell', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('cast_spell_command_error', 'Cannot cast %spell. %fail_reason', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('cast_spell_command_error_failed', 'Failed to cast %spell', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('cast_spell_command_amount', ' |cffffff00(x%amount left)|r', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('reply_question_what', 'i dont know what', 0, 0),
    ('reply_question_what', 'i dont know %s', 0, 0),
    ('reply_question_what', 'who cares', 0, 0),
    ('reply_question_what', 'afraid that was before i was around or paying attention', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('reply_question_who', 'nobody', 0, 0),
    ('reply_question_who', 'we all do', 0, 0),
    ('reply_question_who', 'perhaps its you, %s', 0, 0),
    ('reply_question_who', 'dunno %s', 0, 0),
    ('reply_question_who', 'is it me?', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('reply_question_when', 'soon perhaps %s', 0, 0),
    ('reply_question_when', 'probably later', 0, 0),
    ('reply_question_when', 'never', 0, 0),
    ('reply_question_when', 'what do i look like, a psychic?', 0, 0),
    ('reply_question_when', 'a few minutes, maybe an hour ... years?', 0, 0),
    ('reply_question_when', 'when? good question %s', 0, 0),
    ('reply_question_when', 'dunno %s', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('reply_question_where', 'really want me to answer that?', 0, 0),
    ('reply_question_where', 'on the map?', 0, 0),
    ('reply_question_where', 'who cares', 0, 0),
    ('reply_question_where', 'afk?', 0, 0),
    ('reply_question_where', 'none of your buisiness where', 0, 0),
    ('reply_question_where', 'yeah, where?', 0, 0),
    ('reply_question_where', 'dunno %s', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('reply_question_why', 'dunno %s', 0, 0),
    ('reply_question_why', 'why? just because %s', 0, 0),
    ('reply_question_why', 'why is the sky blue?', 0, 0),
    ('reply_question_why', 'dont ask me %s, im just a bot', 0, 0),
    ('reply_question_why', 'your asking the wrong person', 0, 0),
    ('reply_question_why', 'who knows?', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('reply_question_is', 'ya %s thats true', 0, 0),
    ('reply_question_is', 'dunno %s', 0, 0),
    ('reply_question_is', 'i dont think so %s', 0, 0),
    ('reply_question_is', 'yes', 0, 0),
    ('reply_question_is', 'no', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('reply_question_generic', 'dunno %s', 0, 0),
    ('reply_question_generic', 'beats me %s', 0, 0),
    ('reply_question_generic', 'how should i know %s', 0, 0),
    ('reply_question_generic', 'dont ask me %s, im just a bot', 0, 0),
    ('reply_question_generic', 'your asking the wrong person', 0, 0),
    ('reply_question_generic', 'what do i look like, a psychic?', 0, 0),
    ('reply_question_generic', 'sure %s', 0, 0),
    ('reply_question_generic', 'i dont think so %s', 0, 0),
    ('reply_question_generic', 'maybe', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_greet', 'Hey there!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_dance', 'Shake what your mama gave you!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_love', 'Awwwww...', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_flex', 'Hercules! Hercules!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_angry', 'Did I do thaaaaat?', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_stink', 'Wasn''t me! Just sayin''..', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_joke', 'Oh.. was I not supposed to laugh so soon?', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_chicken', 'We''ll see who''s chicken soon enough!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_apologize', 'You damn right you''re sorry!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_applaud', 'Thank you.. Thank you.. I''m here all week.', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_beg', 'Beg all you want.. I have nothing for you.', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_bite', 'OUCH! Dammit, that hurt!', 1, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_bored', 'My job description doesn''t include entertaining you..', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_brb', 'Looks like time for an AFK break..', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_agree', 'At least SOMEONE agrees with me!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_amaze', 'Yes, Yes. I know I''m amazing..', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_bleed', 'MEDIC! Stat!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_blink', 'What? You got something in your eye?', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_bark', 'Who''s a good doggy? You''re a good doggy!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_bye', 'Umm.... wait! Where are you going?!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_laugh', 'Wait... what are we laughing at again?', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_confused', 'Don''t look at  me.. I just work here', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_germs', 'Ewww! Keep your nasty germs over there!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_cry', 'Don''t you start crying or it''ll make me start crying!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_crack', 'It''s clobbering time!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_eat', 'I hope you brought enough for the whole class...', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_gloat', 'Doesn''t mean you need to be an ass about it..', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_hungry', 'What? You want some of this?', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_tired', 'Is it break time already?', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_sexy', 'Keep it in your pants, boss..', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_no', 'Aww.... why not?!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_panic', 'Now is NOT the time to panic!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_point', 'What?! I can do that TOO!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_rude', 'Right back at you, bub!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_roar', 'RAWR!', 1, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_talk', 'Blah Blah Blah Yakety Smackety..', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_thank', 'You are quite welcome!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_cheer', 'Yay!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_cold', 'And what exactly am I supposed to do about that?', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_comfort', 'Thanks...', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_insult', 'You hurt my feelings..', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_jk', 'You.....', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_raise', 'Yes.. you.. at the back of the class..', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_ready', 'Ready here, too!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_shoo', 'Shoo yourself!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_slap', 'What did I do to deserve that?', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_stand', 'What? Break time''s over? Fine..', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_tickle', 'Hey! Stop that!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_violin', 'Har Har.. very funny..', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_goodluck', 'Thanks... I''ll need it..', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text (name, text, say_type, reply_type) VALUES
    ('emote_reply_brandish', 'Please don''t kill me!', 0, 0);

INSERT IGNORE INTO {SHARED}.playerbot_v2_bot_text_chance (name, chance) VALUES
    ('taunt', 30),
    ('aoe', 75),
    ('loot', 20),
    ('broadcast_looting_item_poor', 0.1),
    ('broadcast_looting_item_normal', 0.5),
    ('broadcast_looting_item_uncommon', 33.33),
    ('broadcast_looting_item_rare', 66.67),
    ('broadcast_looting_item_epic', 100),
    ('broadcast_looting_item_legendary', 100),
    ('broadcast_looting_item_artifact', 100),
    ('broadcast_quest_accepted_generic', 20),
    ('broadcast_quest_update_add_kill_objective_completed', 1),
    ('broadcast_quest_update_add_item_objective_completed', 1),
    ('broadcast_quest_update_add_kill_objective_progress', 1),
    ('broadcast_quest_update_add_item_objective_progress', 1),
    ('broadcast_quest_update_failed_timer', 1),
    ('broadcast_quest_update_complete', 3.33),
    ('broadcast_quest_turned_in', 33.33),
    ('broadcast_killed_normal', 0.1),
    ('broadcast_killed_elite', 1),
    ('broadcast_killed_rareelite', 10),
    ('broadcast_killed_worldboss', 66.67),
    ('broadcast_killed_rare', 33.33),
    ('broadcast_killed_unknown', 0.33),
    ('broadcast_killed_pet', 0.03),
    ('broadcast_killed_player', 0.1),
    ('broadcast_levelup_generic', 66.67),
    ('broadcast_levelup_10x', 100),
    ('broadcast_levelup_max_level', 100);

INSERT INTO {SHARED}.playerbot_v2_schema_version (version, sha256) VALUES
    (19, REPEAT('0', 64))
ON DUPLICATE KEY UPDATE applied_at = CURRENT_TIMESTAMP;
