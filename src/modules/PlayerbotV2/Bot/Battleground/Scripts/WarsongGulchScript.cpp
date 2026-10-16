// WarsongGulchScript — Warsong Gulch (BattlemasterList id 2 / BATTLEGROUND_WS).
// Classic CTF on the modern map (TC registers WSG on map 2106; legacy
// map 489 still referenced in some DBC paths).
//
// Authoritative TC sources (12.0.1 branch):
//   src/server/scripts/Battlegrounds/WarsongGulch/battleground_warsong_gulch.cpp
//     :123-124 — flag GO entries 227740 (Horde) / 227741 (Alliance)
//     :43-50   — pickup spells (23333..23336)
//   src/server/game/Miscellaneous/SharedDefines.h
//     :3195    — GAMEOBJECT_TYPE_NEW_FLAG = 36
//     :3196    — GAMEOBJECT_TYPE_NEW_FLAG_DROP = 37
// V1 coords:
//   src/modules/Playerbot/bot logic/Coordination/Battleground/Scripts/CTF/
//     WarsongGulchData.h:38-47 (flag spawns), :123-149 (defender posts)

#include "../BattlegroundScript.h"
#include "../../BotSnapshotView.h"

namespace Playerbot {

namespace {

class WarsongGulchScript final : public BattlegroundScript
{
public:
    uint16_t bg_type_id() const override { return 2; }  // BATTLEGROUND_WS
    char const* name() const override { return "warsong_gulch"; }

    BattlegroundAdvice get_advice(BotSnapshotView const& s) const override
    {
        BattlegroundAdvice a;
        // WSG/TP score = flag CAPS (0-3), not points: a 2-cap swing is
        // decisive (BG audit N55/N65 -- the 200-point default could
        // never trigger here).
        a.score_bias_threshold = 2;
        // 10v10. Slot 0 is FC (stealth-class bias is a deferred schema
        // add — see fc_class_preference TODO). FlagCarrier and FCEscort
        // are dynamic-handoff capable upstream (idle:acts_as_fc backup
        // path lets Roamer/Attacker step in when the primary dies).
        a.role_by_slot = {
            BgRole::FlagCarrier,
            BgRole::FCEscort,
            BgRole::Defender,     // flag room guards: two, each at its own post beside the stand (not on it: the owner, two bots camped on
            BgRole::Defender,     // the stand). Ported from the ike3 bots' two defender posts (Karrz1/wow-1.12 raid10-revamp bcc8b9f)
            BgRole::Roamer,
            BgRole::Roamer,
            BgRole::Attacker,
            BgRole::Attacker,
            BgRole::Healer,
            BgRole::Healer,
        };
        a.escort_friendly_carrier = true;
        a.chase_enemy_carrier     = true;
        // Clothie casters (Priest/Mage/Warlock/Evoker) shouldn't sprint
        // into a moving FC at 1v1 range — they feed honor and get peeled
        // off their objectives. Melee + hybrid + Hunter can chase.
        a.chase_melee_only        = true;
        // Stealth-FC meta: Rogue (4) and Druid (11 — feral Travel Form)
        // are the canonical WSG carriers. Stealth opens a defended
        // flagroom; Druid Travel keeps the run from being kited dead.
        // The dispatcher promotes preferred-class bots to FlagCarrier
        // regardless of slot hash; non-preferred bots that would have
        // hashed into the FC slot fall back to Roamer.
        a.fc_class_preference     = { 4u, 11u };
        // Modern WSG (map 2106): flag pedestals are NEW_FLAG (36), a
        // dropped flag is NEW_FLAG_DROP (37). Classic WSG (map 489,
        // battleground_warsong_gulch.cpp) has the 1.12 objects:
        // FLAGSTAND (24) on the stands (179830/179831) and FLAGDROP (26)
        // on the ground (179785/179786). Each map spawns only its own
        // pair; the own-stand skip in BgTryUseObjectiveGo covers 24 the
        // same way as 36.
        a.auto_use_go_types = { 36, 37, 24, 26 };
        // Flag spawn coords from V1 WarsongGulchData.h:38-47
        // (Alliance 1540.423,1481.325,351.818; Horde 916.023,1433.805,346.037).
        constexpr float ALLY_FX  = 1540.423f, ALLY_FY  = 1481.325f, ALLY_FZ  = 351.818f;
        constexpr float HORDE_FX =  916.023f, HORDE_FY = 1433.805f, HORDE_FZ =  346.037f;
        if (s.is_horde())
        {
            a.own_flag_x   = HORDE_FX; a.own_flag_y   = HORDE_FY; a.own_flag_z   = HORDE_FZ;
            a.enemy_flag_x = ALLY_FX;  a.enemy_flag_y = ALLY_FY;  a.enemy_flag_z = ALLY_FZ;
            // Defender anchor = the Horde flag room (stand). The ramp top (948.68,1458.65) left defenders 40 y from the stand, so an
            // Alliance carrier walked out past them. The hold rule gives each defender its own hold radius around the anchor, so
            // they spread through the room instead of piling on the pedestal.
            a.home_base_x = HORDE_FX; a.home_base_y = HORDE_FY; a.home_base_z = HORDE_FZ;
            a.defend_posts = { {921.0f, 1429.5f, 346.2f}, {921.0f, 1438.0f, 346.2f} };   // ike3 WS_DEFEND_POS_HORDE_1 / _2
            // ike3 WS_FLAG_HIDE_HORDE_1 / _2: the ground-level hiding spots (the roof ones need a jump route)
            a.carrier_hide_spots = { {928.178f, 1458.432f, 346.889f}, {914.954f, 1439.540f, 346.415f} };
        }
        else
        {
            a.own_flag_x   = ALLY_FX;  a.own_flag_y   = ALLY_FY;  a.own_flag_z   = ALLY_FZ;
            a.enemy_flag_x = HORDE_FX; a.enemy_flag_y = HORDE_FY; a.enemy_flag_z = HORDE_FZ;
            // Alliance flag room (stand); the ramp top (1507.04,1456.85) was 40 y from the stand.
            a.home_base_x = ALLY_FX; a.home_base_y = ALLY_FY; a.home_base_z = ALLY_FZ;
            a.defend_posts = { {1534.5f, 1477.5f, 352.4f}, {1534.5f, 1486.0f, 352.4f} };   // ike3 WS_DEFEND_POS_ALLIANCE_1 / _2
            // ike3 WS_FLAG_HIDE_ALLIANCE_1 / _2 / _3: the ground-level hiding spots (the roof ones need a jump route)
            a.carrier_hide_spots = { {1529.249f, 1456.470f, 353.040f}, {1540.286f, 1476.026f, 352.692f}, {1495.807f, 1466.774f, 352.350f} };
        }
        return a;
    }
};

} // anonymous

std::unique_ptr<BattlegroundScript> MakeWarsongGulchScript()
{
    return std::make_unique<WarsongGulchScript>();
}

} // namespace Playerbot
