#include "RaidEncounters.h"

// Ported from Karrz's raid10-revamp ike3 bots (Karrz1/wow-1.12). Script references (mc/..., bwl/..., ony/...) point at the
// VMaNGOS scripts that Forever's scripts/Custom/Classic instances were ported from; the ids are the same.

namespace Playerbot::Raid {

    // Movement and positioning for Blackwing Lair (map 469). Maintained by the lead engineer.
    void RegisterBlackwingMovement(RaidRules& r)
    {
        // The three drakes (Firemaw 11983, Ebonroc 14601, Flamegor 11981): Wing Buffet and Shadow Flame hit the cone in front of them.
        // The tank holds the front; everyone else stays out of it.
        for (uint32_t drake : { 11983u, 14601u, 11981u })
            r.avoidFrontal.push_back({ drake, 24.0f, 55.0f, Who::NonTank });
        // Chromaggus: the breath comes out of the front.
        r.avoidFrontal.push_back({ 14020, 35.0f, 50.0f, Who::NonTank });
        // Vaelastrasz: his flame breath cleave chains through anyone near the front.
        r.avoidFrontal.push_back({ 13020, 20.0f, 60.0f, Who::NonTank });
        // Broodlord Lashlayer (12017): Blast Wave 23331 is cast on himself (boss_broodlord_lashlayer.cpp:170-173) every 20-35s; it is a
        // point-blank AoE with knockback, so ranged and healers stay out of 10 yards (radius UNVERIFIED: spell_template is not in the extracts).
        r.avoidEnemy.push_back({ 12017, 10.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 12017, 10.0f, Who::Healer });
        // Nefarian (11583): Shadow Flame 22539 and Cleave 20691 to the front (classic_boss_nefarian.cpp:314,341). Tail Lash 23364 behind
        // him: melee stand at his sides.
        r.avoidFrontal.push_back({ 11583, 25.0f, 55.0f, Who::NonTank });
        r.avoidFrontal.push_back({ 11583, 20.0f, 45.0f, Who::NonTank, 180.0f });
        // Corrupted Infernal (14668, warlock call): Immolation 20153 burns everything close; casters and healers stay off it (ike3).
        r.avoidEnemy.push_back({ 14668, 10.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 14668, 10.0f, Who::Healer });
        // Victor Nefarius' Drakonid packs are handled by the data file (priorities, dispels).
    }
} // namespace Playerbot::Raid
