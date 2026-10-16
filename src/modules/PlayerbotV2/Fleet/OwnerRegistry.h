// OwnerRegistry — bot-to-owner binding. The "owner" of a bot is a
// specific player account (and optionally a specific character on that
// account). Authority for whisper / squad commands derives from this
// binding: only the owner can issue control commands, and the binding
// survives logout, group disband, and server restart.
//
// Storage: persisted in playerbot_v2_character.owner_account_id /
// owner_player_guid (migration 0002). In-memory cache is a hash map
// guarded by shared_mutex. Writes are immediate (not debounced) — owner
// changes are infrequent and command authority depends on durability.
//
// World-thread mutations (mark / adopt / disown). Reads from any thread
// (whisper handler, bot workers, snapshot builder).

#pragma once

#include "Bot/BotTypes.h"
#include <cstdint>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Field;

namespace Playerbot {

struct OwnerBinding
{
    // Account that owns the bot. 0 = unowned (legacy / pre-migration).
    uint32 account_id = 0;
    // Specific character GUID-low on that account that owns the bot. 0
    // means "any character on the account is the owner". Used to support
    // alts: by default any of the player's characters can command, but
    // an owner can bind a bot to a specific main if they want.
    uint64 player_guid = 0;
    // On the owner's saved companion roster (`.bot save`): `.bot call all` brings back only these. Migration 0018.
    bool saved = false;
};

class OwnerRegistry
{
public:
    // Loads bindings from the DB. Returns the count.
    size_t LoadFromDb();

    // Returns the bot's binding. {0, 0} when unowned.
    OwnerBinding GetOwner(BotId bot) const;

    // Sets the bot's owner. Writes immediately to the DB; the in-memory
    // cache is updated on success. account_id == 0 + player_guid == 0
    // = clear ownership.
    void SetOwner(BotId bot, uint32 account_id, uint64 player_guid);

    // Convenience: clears any binding.
    void ClearOwner(BotId bot) { SetOwner(bot, 0, 0); }

    // Authority predicate. Returns true when (sender_account, sender_guid)
    // is allowed to command `bot` per the binding rules:
    //   - If bot is unowned: any sender is allowed (legacy behaviour;
    //     command parser then layers a group-membership check for those).
    //   - If bot is owned and player_guid != 0: only that exact char.
    //   - If bot is owned and player_guid == 0: any char on the account.
    bool IsOwner(BotId bot, uint32 sender_account, uint64 sender_guid) const;

    // Puts the bot on (or takes it off) its owner's saved roster. Writes immediately to the DB. No-op for an unowned bot.
    void SetSaved(BotId bot, bool saved);

    // A hunter companion's starter pet (the Taming the Beast pet it never quests for) is granted once per bot, ever:
    // after that a pet the owner abandons stays abandoned and the owner's own Tame Beast is free. Migration 0020.
    bool StarterPetGranted(BotId bot) const;
    void MarkStarterPetGranted(BotId bot);

    // The companion's equipment has had its one full re-roll with the per-bot varied generator. Companions geared
    // before gear variety existed all wear the same top pick per slot; they are re-rolled once (in or out of combat
    // permitting) and then marked. Migration 0023.
    // 2 in the same column: a level-cap companion has also had the raid-gear variety pass (RaidRegearPass `vary`), which swaps worn
    // pieces for the bot's own near-equal pick. The 1 of the first re-roll leaves it at 1, so bots varied before are found again.
    bool GearVariedRaid(BotId bot) const;
    void MarkGearVariedRaid(BotId bot);
    bool GearVaried(BotId bot) const;
    void MarkGearVaried(BotId bot);

    // The role the owner gave the companion (`.bot add <class> healer`, `/setrole`), kept across relog, `.bot call` and
    // restart (the BotAI override and the Classic virtual spec are memory only, and a re-inference from the bought talents
    // flips hybrids). Role::Unknown = none declared. Writes immediately. Migration 0024.
    Role DeclaredRole(BotId bot) const;
    void SetDeclaredRole(BotId bot, Role role);

    // The raid sub-group (0-based) the owner had the companion in, kept across relog and restart so `.bot call all` and a relog put it
    // back (RaidSubgroupPlan.h). 0xFF = none stored. Writes immediately. Migration 0026.
    uint8 StoredSubgroup(BotId bot) const;
    void  SetStoredSubgroup(BotId bot, uint8 subgroup);

    // The Classic spec id the owner had the companion in when it was saved (`.bot save`), 0 = none stored. Pinned again when the bot is
    // set up on a call or login (BotPopulationManager::ApplyCompanionRole) if it still fits the stored role. Writes immediately.
    // Migration 0027.
    uint32 StoredSpec(BotId bot) const;
    void   SetStoredSpec(BotId bot, uint32 spec);

    // The companion's index in the owner's group member list when it was saved (`.bot save`), 0xFFFF = none. The raid frame lists a
    // sub-group in member-list order; RestoreRaidSubgroups puts the owned members back in this order. Migration 0028.
    uint16 StoredOrder(BotId bot) const;
    void   SetStoredOrder(BotId bot, uint16 order);

    // The gear mode an owner account chose for its companions (Gear::Owned::GearMode value; 0 = match when never set). Writes
    // immediately. Migration 0025.
    uint8 OwnerGearMode(uint32 account_id) const;
    void  SetOwnerGearMode(uint32 account_id, uint8 mode);

    // The owner's crowd-control switch (the ForeverBots panel's CC icon, group order "crowdcontrol on|off"): whether the bots of the
    // groups this account leads mark and hold crowd-control targets (Playerbot.Behavior.MarkCc must be on as well). On when never
    // set; memory only, so a restart is back to on. Returns true when the value changed.
    bool CcEnabled(uint32 account_id) const;
    bool SetCcEnabled(uint32 account_id, bool enabled);

    // Returns the bot ids currently owned by `account_id`. Empty if none.
    // O(N) in the registry; only used by GM diagnostic commands and the
    // /squad addressing — not in any per-tick path.
    std::vector<BotId> BotsOwnedBy(uint32 account_id) const;

    // Per-bot squad state persisted alongside the owner binding.
    // Migration 0002 adds these columns. Loaded on bot register;
    // saved when /formation, /follow_distance, /slot, /verbose mutate.
    struct SquadState
    {
        uint8 formation_type = 0;     // FormationType (Free=0)
        uint8 formation_slot = 0;
        float follow_distance = 5.0f;
        bool  owner_verbose  = false;
    };

    // Pull the persisted squad state for a bot. Used at bot register
    // time so the in-memory BotAI starts with the owner's saved
    // preferences instead of defaulting every relog.
    SquadState LoadSquadState(BotId bot) const;

    // Column list LoadSquadState selects, in the order SquadStateFromFields
    // reads them. A caller that already reads the bot's playerbot_v2_character
    // row (the bot-login path) selects these alongside its own columns and
    // parses them with SquadStateFromFields instead of a second round trip.
    static constexpr char const* kSquadStateColumns =
        "formation_type, formation_slot, follow_distance_yd, owner_verbose";
    // Parse the four kSquadStateColumns fields starting at `f`.
    static SquadState SquadStateFromFields(Field const* f);

    // Persist the in-memory squad state back to the DB. Called from
    // mutation commands (/formation, /follow_distance, etc). Single
    // UPDATE statement; safe to call frequently — no debouncing
    // necessary for a fleet under ~500 bots.
    void SaveSquadState(BotId bot, SquadState const& s) const;

    // Squad-preset save/load. A "squad" is the owner's currently-owned
    // set of bots; a preset is a snapshot of each bot's
    // (formation_type, formation_slot, follow_distance) under a
    // user-named label. Stored in playerbot_v2_squad_preset, keyed by
    // (owner_account_id, preset_name).
    //
    // Save walks the owner's bots, gathers each bot's current squad
    // state, serialises as a compact string (`bot:type:slot:fd|...`),
    // upserts the preset row.
    //
    // Load reads the row, parses, applies SquadState to each named bot
    // (if still owned + registered). Bots no longer owned by the
    // account are skipped silently.
    //
    // Both return the count affected. 0 from Save = the owner has no
    // bots; 0 from Load = preset name not found or all bots gone.
    size_t SaveSquadPreset(uint32 owner_account, std::string const& preset_name) const;
    size_t LoadSquadPreset(uint32 owner_account, std::string const& preset_name) const;

    // The character a real player is currently playing on each account, so a bot bound to the whole account
    // (player_guid 0, e.g. `.playerbot adopt`) knows which character to follow and treat as its owner. Set on a
    // real player's login and cleared on logout (world thread); read from any thread.
    void NoteOwnerCharacterOnline(uint32 account_id, uint64 player_guid);
    void NoteOwnerCharacterOffline(uint32 account_id, uint64 player_guid);

    // The bot's owning character: the bound one, or for an account-wide binding the character currently online on
    // that account. 0 when unowned or nobody on the owning account is online.
    uint64 OwnerCharacter(BotId bot) const;

    // Owner-assigned tank markers per group (Playerbot.Behavior.TankMarkers): tank guid-low -> raid marker index
    // (0..7, WoW order Star=0 .. Skull=7). A tank holds the mob carrying its marker; these override the automatic
    // marker split (PveGroupCoordinator::PlanRaidTanks). Memory only (a group lasts one session). Keyed by the
    // group's GUID counter. A marker belongs to one tank and a tank has one marker: setting either replaces the old.
    // Written on the world thread (chat order), read from the coordinator on the world thread.
    void SetTankMark(uint64 group_low, uint64 tank_low, uint8 marker);
    void ClearTankMark(uint64 group_low, uint64 tank_low);
    void ClearTankMarks(uint64 group_low);
    std::vector<std::pair<uint64, uint8>> TankMarks(uint64 group_low) const;   // (tank guid-low, marker)

    // Per-bot raid-symbol orders ("rti <symbols>" / "rti cc <symbols>", ike3 semantics): the symbols (0..7, WoW order
    // star 0 .. skull 7) whose marked mob this bot kills / tanks first, and the ones whose mob it crowd-controls, in
    // order. They extend the symbol the owner puts on the bot itself (PveGroupCoordinator::PlanMarkOrders). Memory
    // only; world thread writers and readers. An empty list clears.
    struct RtiOrder
    {
        std::vector<uint8> kill;
        std::vector<uint8> cc;
    };
    void     SetRtiKill(uint64 bot_low, std::vector<uint8> symbols);
    void     SetRtiCc(uint64 bot_low, std::vector<uint8> symbols);
    RtiOrder RtiOrderOf(uint64 bot_low) const;

private:
    mutable std::shared_mutex                mtx_;
    std::unordered_map<uint64, std::unordered_map<uint64, uint8>> tank_marks_;
    std::unordered_map<uint64, RtiOrder>     rti_orders_;
    std::unordered_map<BotId, OwnerBinding>  owners_;
    std::unordered_map<uint32, uint64>       online_owner_character_;
    std::unordered_set<BotId>                starter_pet_granted_;
    std::unordered_set<BotId>                gear_varied_;
    std::unordered_set<BotId>                gear_varied_raid_;
    std::unordered_map<BotId, Role>          declared_role_;
    std::unordered_map<BotId, uint32>        spec_;
    std::unordered_map<BotId, uint16>        order_;
    std::unordered_map<BotId, uint8>         subgroup_;   // 0-based raid sub-group
    std::unordered_map<uint32, uint8>        owner_gear_mode_;
    std::unordered_set<uint32>               cc_disabled_;
};

} // namespace Playerbot
