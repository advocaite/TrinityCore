#include "RaidEncounters.h"

// Zul'Gurub (map 309), 20-player. New for Forever: Karrz's ike3 bots had no ZG data.
// Sources: Forever's Classic ports of the VMaNGOS scripts, src/server/scripts/Custom/Classic/instances/zulgurub (abbreviated
// zg/<file>.cpp below), and the vanilla spell data (VMaNGOS spell_template: effect, radius, dispel type) for what each cast reaches and
// what removes it. Kept to the few rows that decide a fight; the rest is the tank-and-spank the bots already do.

namespace Playerbot::Raid {

    void RegisterZulGurub(RaidRules& r)
    {
        r.name = "Zul'Gurub";

        // Kick-first casts: Jeklik's Great Heal 23954 on herself (92-107k) and Lor'Khan's Great Heal 24208 on the trio.
        r.interruptFirst.push_back(23954);
        r.interruptFirst.push_back(24208);

        r.bosses = { 14517, 14507, 14510,   // High Priestess Jeklik, High Priest Venoxis, High Priestess Mar'li
                     14509, 14515,          // High Priest Thekal (with Zath and Lor'Khan), High Priestess Arlokk
                     11382, 15114, 11380,   // Bloodlord Mandokir, Gahz'ranka, Jin'do the Hexxer
                     14834 };               // Hakkar

        // ---------------------------------------------------------------------------------------------------------------
        // HIGH PRIESTESS JEKLIK (14517, zg/classic_boss_jeklik.cpp): Bloodseeker Bats 11368 come six at a time; under 50% a Frenzied Bat
        // 14965 drops Liquid Fire, a flame object 177764 on the ground. Curse of Blood 16098 is a curse on everybody in reach.
        r.priority.push_back({ 11368, 3 });
        r.priority.push_back({ 14965, 4 });
        r.avoidGameObject.push_back({ 177764, 6.0f, Who::Anyone });
        r.dispel.push_back({ 16098, DispelKind::Curse });

        // ---------------------------------------------------------------------------------------------------------------
        // HIGH PRIEST VENOXIS (14507, zg/classic_boss_venoxis.cpp): the four Razzashi Cobras 11373 come with the pull; Renew 23895 on
        // himself is purged; Holy Wrath 23979 chains between raiders standing together; at 50% he turns snake and wipes threat, then
        // drops a Poison Cloud 23861 (a persistent cloud under him, 500 a second) and puts a Parasitic Serpent 14884 on random raiders.
        // Venom Spit 23862 is a poison, Holy Fire 23860 magic.
        r.priority.push_back({ 11373, 3 });
        r.priority.push_back({ 14884, 5 });
        r.purge.push_back({ 23895 });
        r.spread.push_back({ 14507, 8.0f, Who::Ranged });
        r.spread.push_back({ 14507, 8.0f, Who::Healer });
        r.avoidGroundEffect.push_back({ 14507, 23861, 2.0f, Who::Anyone });
        r.dispel.push_back({ 23862, DispelKind::Poison });
        r.dispel.push_back({ 23860, DispelKind::Magic });
        r.retaunt.push_back(14507);

        // ---------------------------------------------------------------------------------------------------------------
        // HIGH PRIESTESS MAR'LI (14510, zg/classic_boss_marli.cpp): her Spawns of Mar'li 15041 grow (Enlarge) and die first; Poison Bolt
        // Volley 24099 hits everybody near her (poison); Corrosive Poison 24111 takes 5000 armor off the tank (poison, urgent); in spider
        // form Enveloping Webs roots the raid and she charges the top of her threat list, so a tank takes her back.
        r.priority.push_back({ 15041, 4 });
        r.avoidEnemy.push_back({ 14510, 12.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 14510, 12.0f, Who::Healer });
        r.dispel.push_back({ 24099, DispelKind::Poison });
        r.dispel.push_back({ 24111, DispelKind::Poison, true });
        r.retaunt.push_back(14510);

        // ---------------------------------------------------------------------------------------------------------------
        // HIGH PRIEST THEKAL (14509) with ZATH (11348) and LOR'KHAN (11347) (zg/classic_instance_zulgurub.cpp): each one feigns death
        // at 1 hp (Pacify Self 19951 while down) and a standing one resurrects the fallen within 10 s, so the three go down together:
        // the DPS keep them within 10% of each other and leave a downed one alone. Thekal's Bloodlust 24185 and Lor'Khan's Lightning
        // Shield 20545 are magic buffs the priests and shamans strip; in tiger form his Zulian Guardians 15068 die first and his Speed
        // Slash bleeds everybody within 8 yd, so only the tanks and melee stand there.
        r.killTogether.push_back({ 14509, 19951, 10.0f, { 11348, 11347 } });
        r.purge.push_back({ 24185 });
        r.purge.push_back({ 20545 });
        r.priority.push_back({ 15068, 5 });
        r.avoidEnemy.push_back({ 14509, 10.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 14509, 10.0f, Who::Healer });

        // ---------------------------------------------------------------------------------------------------------------
        // HIGH PRIESTESS ARLOKK (14515, zg/classic_boss_arlokk.cpp): Zulian Prowlers 15101 keep coming while she is away; Gouge 12540
        // in panther form cuts the tank's threat, so a tank taunts her back.
        r.priority.push_back({ 15101, 2 });
        r.retaunt.push_back(14515);

        // ---------------------------------------------------------------------------------------------------------------
        // BLOODLORD MANDOKIR (11382, zg/classic_boss_mandokir.cpp): the Vilebranch Speaker 11391 starts the fight and dies first; Ohgan
        // 14988 dies while the raid is fresh (Mandokir enrages for the rest of the fight when his raptor falls, so it happens early, not
        // late). Threatening Gaze 24314 watches a raider for 6 s: that raider does nothing (Decapitate 24315 otherwise). After his
        // Charge he fears everybody within 4 yd of his victim, so the ranged keep apart; a tank takes him back afterwards.
        r.priority.push_back({ 11391, 6 });
        r.priority.push_back({ 14988, 5 });
        r.holdByAura.push_back({ 24314, Who::Anyone });
        r.spread.push_back({ 11382, 5.0f, Who::Ranged });
        r.spread.push_back({ 11382, 5.0f, Who::Healer });
        r.retaunt.push_back(11382);

        // ---------------------------------------------------------------------------------------------------------------
        // GAHZ'RANKA (15114, zg/classic_boss_gahzranka.cpp): Frost Breath 16099 is a cone in front; Massive Geyser 22421 throws the melee
        // and resets threat, so a tank taunts him back.
        r.avoidFrontal.push_back({ 15114, 20.0f, 60.0f, Who::NonTank });
        r.retaunt.push_back(15114);

        // ---------------------------------------------------------------------------------------------------------------
        // JIN'DO THE HEXXER (11380, zg/classic_boss_jindo.cpp): the Powerful Healing Ward 14987 (10,000 a tick to him) and the Brain
        // Wash Totem 15112 die before anything else; a brain-washed raider (24261) is not attacked; Delusions of Jin'do 24306 is a curse
        // that drops the carrier into the spirit world with the Shades of Jin'do 14986, which only the carrier sees and kills; Hex
        // 17172 (magic) turns the tank into a frog and throws his threat away, so a tank takes him back.
        r.priority.push_back({ 14987, 7 });
        r.priority.push_back({ 15112, 7 });
        r.priority.push_back({ 14986, 4 });
        r.controlledMember.push_back({ 24261 });
        r.dispel.push_back({ 24306, DispelKind::Curse });
        r.dispel.push_back({ 17172, DispelKind::Magic });
        r.retaunt.push_back(11380);

        // ---------------------------------------------------------------------------------------------------------------
        // HAKKAR (14834, zg/classic_boss_hakkar.cpp): Corrupted Blood 24328 spreads to everybody within 13 yd of its carrier (vanilla data:
        // no dispel type), so the carrier walks away and the others keep clear; Cause Insanity 24327 charms the tank for 4 s (nobody hits
        // him, a tank takes Hakkar back); Poisonous Blood 24321 from a Son of Hakkar 11357 cloud is what turns his Blood Siphon against
        // him, so nobody removes it.
        r.carrierRun.push_back({ 24328, 14.0f, Who::NonTank });
        r.avoidCarrier.push_back({ 24328, 14.0f, Who::NonTank });
        r.controlledMember.push_back({ 24327 });
        r.noDispel.push_back(24321);
        r.retaunt.push_back(14834);
        // Not ruled: standing in a Son of Hakkar's poison cloud before each Blood Siphon is scripted work (a "stand in" move the rules do not
        // have); without it the siphon heals him and the fight is a longer burn.

        // ---------------------------------------------------------------------------------------------------------------
        // EDGE OF MADNESS (one of four, when the instance reaches them): Hazza'rah's Nightmare Illusions 15163 (457 hp, hit for 4-8k) die
        // at once; Renataki switches to a random raider every few seconds; Wushoolay's Lightning Cloud 24683 is a persistent cloud.
        r.priority.push_back({ 15163, 6 });
        r.retaunt.push_back(15084);
        r.avoidGroundEffect.push_back({ 15085, 24683, 2.0f, Who::Anyone });
    }

} // namespace Playerbot::Raid
