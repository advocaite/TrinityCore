#include "OwnerRegistry.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include <cstdio>
#include <stdexcept>
#include <unordered_set>

namespace Playerbot {

namespace {
auto& Db() { return CharacterDatabase; }
}

size_t OwnerRegistry::LoadFromDb()
{
    auto result = Db().Query(
        "SELECT character_guid_low, owner_account_id, owner_player_guid, companion_saved, starter_pet_granted, gear_varied, companion_role, companion_subgroup, companion_spec, companion_order "
        "FROM playerbot_v2_character "
        "WHERE owner_account_id != 0 OR companion_role != 0");
    auto gear_modes = Db().Query("SELECT owner_account_id, gear_mode FROM playerbot_v2_owner_gear");
    std::unique_lock lk(mtx_);
    owner_gear_mode_.clear();
    if (gear_modes)
        do { Field* g = gear_modes->Fetch(); owner_gear_mode_[g[0].GetUInt32()] = g[1].GetUInt8(); } while (gear_modes->NextRow());
    owners_.clear();
    starter_pet_granted_.clear();
    gear_varied_.clear();
    gear_varied_raid_.clear();
    declared_role_.clear();
    subgroup_.clear();
    spec_.clear();
    order_.clear();
    if (!result)
    {
        TC_LOG_INFO("playerbot.v2", "[PlayerbotV2 Owners] No owner bindings yet.");
        return 0;
    }
    do {
        Field* f = result->Fetch();
        OwnerBinding b{};
        const BotId bot = f[0].GetUInt64();
        b.account_id    = f[1].GetUInt32();
        b.player_guid   = f[2].GetUInt64();
        b.saved         = f[3].GetUInt8() != 0;
        if (b.account_id != 0)
            owners_[bot] = b;
        if (f[4].GetUInt8() != 0)
            starter_pet_granted_.insert(bot);
        if (f[5].GetUInt8() != 0)
            gear_varied_.insert(bot);
        if (f[5].GetUInt8() >= 2)
            gear_varied_raid_.insert(bot);
        if (uint8 const r = f[6].GetUInt8(); r >= 1 && r <= 3)
            declared_role_[bot] = Role(r - 1);
        if (uint8 const sg = f[7].GetUInt8(); sg >= 1 && sg <= 8)
            subgroup_[bot] = uint8(sg - 1);
        if (uint32 const sp = f[8].GetUInt16(); sp != 0)
            spec_[bot] = sp;
        if (uint32 const ord = f[9].GetUInt8(); ord != 0)
            order_[bot] = uint16(ord - 1);
    } while (result->NextRow());
    uint32 tanks = 0, healers = 0, dps = 0;
    for (auto const& [bot, role] : declared_role_)
        (role == Role::Tank ? tanks : role == Role::Healer ? healers : dps) += 1;
    TC_LOG_INFO("playerbot.v2",
        "[PlayerbotV2 Owners] Loaded {} owner bindings; stored companion roles: {} tank, {} healer, {} dps.",
        owners_.size(), tanks, healers, dps);
    return owners_.size();
}

OwnerBinding OwnerRegistry::GetOwner(BotId bot) const
{
    std::shared_lock lk(mtx_);
    auto it = owners_.find(bot);
    return it == owners_.end() ? OwnerBinding{} : it->second;
}

void OwnerRegistry::NoteOwnerCharacterOnline(uint32 account_id, uint64 player_guid)
{
    std::unique_lock lk(mtx_);
    online_owner_character_[account_id] = player_guid;
}

void OwnerRegistry::NoteOwnerCharacterOffline(uint32 account_id, uint64 player_guid)
{
    std::unique_lock lk(mtx_);
    auto it = online_owner_character_.find(account_id);
    if (it != online_owner_character_.end() && it->second == player_guid)
        online_owner_character_.erase(it);
}

void OwnerRegistry::SetTankMark(uint64 group_low, uint64 tank_low, uint8 marker)
{
    if (marker > 7) return;
    std::unique_lock lk(mtx_);
    auto& m = tank_marks_[group_low];
    for (auto it = m.begin(); it != m.end();)
        it = (it->second == marker || it->first == tank_low) ? m.erase(it) : std::next(it);
    m[tank_low] = marker;
}

void OwnerRegistry::ClearTankMark(uint64 group_low, uint64 tank_low)
{
    std::unique_lock lk(mtx_);
    if (auto it = tank_marks_.find(group_low); it != tank_marks_.end())
    {
        it->second.erase(tank_low);
        if (it->second.empty()) tank_marks_.erase(it);
    }
}

void OwnerRegistry::ClearTankMarks(uint64 group_low)
{
    std::unique_lock lk(mtx_);
    tank_marks_.erase(group_low);
}

std::vector<std::pair<uint64, uint8>> OwnerRegistry::TankMarks(uint64 group_low) const
{
    std::shared_lock lk(mtx_);
    std::vector<std::pair<uint64, uint8>> out;
    if (auto it = tank_marks_.find(group_low); it != tank_marks_.end())
        out.assign(it->second.begin(), it->second.end());
    return out;
}

void OwnerRegistry::SetRtiKill(uint64 bot_low, std::vector<uint8> symbols)
{
    std::unique_lock lk(mtx_);
    rti_orders_[bot_low].kill = std::move(symbols);
    if (rti_orders_[bot_low].kill.empty() && rti_orders_[bot_low].cc.empty()) rti_orders_.erase(bot_low);
}

void OwnerRegistry::SetRtiCc(uint64 bot_low, std::vector<uint8> symbols)
{
    std::unique_lock lk(mtx_);
    rti_orders_[bot_low].cc = std::move(symbols);
    if (rti_orders_[bot_low].kill.empty() && rti_orders_[bot_low].cc.empty()) rti_orders_.erase(bot_low);
}

OwnerRegistry::RtiOrder OwnerRegistry::RtiOrderOf(uint64 bot_low) const
{
    std::shared_lock lk(mtx_);
    auto it = rti_orders_.find(bot_low);
    return it == rti_orders_.end() ? RtiOrder{} : it->second;
}

uint64 OwnerRegistry::OwnerCharacter(BotId bot) const
{
    std::shared_lock lk(mtx_);
    auto it = owners_.find(bot);
    if (it == owners_.end() || it->second.account_id == 0)
        return 0;
    if (it->second.player_guid != 0)
        return it->second.player_guid;
    auto online = online_owner_character_.find(it->second.account_id);
    return online == online_owner_character_.end() ? 0 : online->second;
}

void OwnerRegistry::SetOwner(BotId bot, uint32 account_id, uint64 player_guid)
{
    bool saved = false;   // a bot stays on the roster only while the same account owns it
    {
        std::unique_lock lk(mtx_);
        if (account_id == 0 && player_guid == 0)
        {
            owners_.erase(bot);
            // The declared role stays with the character through a dismissal: re-added, it plays what it played (the owner
            // 2026-10-08: dismissed paladins and druids came back as tanks from a talent inference).
        }
        else
        {
            auto it = owners_.find(bot);
            saved = it != owners_.end() && it->second.account_id == account_id && it->second.saved;
            owners_[bot] = OwnerBinding{account_id, player_guid, saved};
        }
    }
    // Persist. UPDATE only — the bot row must already exist (it's
    // created by mark_as_bot via BotIdentityRegistry). If the row is
    // missing the UPDATE is a no-op and the in-memory cache is the
    // only state, which is fine for the rare race of a just-spawned
    // bot — next mutation will succeed.
    Db().DirectPExecute(
        "UPDATE playerbot_v2_character "
        "SET owner_account_id = {}, owner_player_guid = {}, companion_saved = {} "
        "WHERE character_guid_low = {}",
        account_id, player_guid, saved ? 1 : 0, bot);
}

void OwnerRegistry::SetSaved(BotId bot, bool saved)
{
    {
        std::unique_lock lk(mtx_);
        auto it = owners_.find(bot);
        if (it == owners_.end())
            return;
        it->second.saved = saved;
    }
    Db().DirectPExecute("UPDATE playerbot_v2_character SET companion_saved = {} WHERE character_guid_low = {}",
        saved ? 1 : 0, bot);
}

bool OwnerRegistry::StarterPetGranted(BotId bot) const
{
    std::shared_lock lk(mtx_);
    return starter_pet_granted_.contains(bot);
}

void OwnerRegistry::MarkStarterPetGranted(BotId bot)
{
    {
        std::unique_lock lk(mtx_);
        if (!starter_pet_granted_.insert(bot).second)
            return;
    }
    Db().DirectPExecute("UPDATE playerbot_v2_character SET starter_pet_granted = 1 WHERE character_guid_low = {}", bot);
}

bool OwnerRegistry::GearVariedRaid(BotId bot) const
{
    std::shared_lock lk(mtx_);
    return gear_varied_raid_.contains(bot);
}

void OwnerRegistry::MarkGearVariedRaid(BotId bot)
{
    {
        std::unique_lock lk(mtx_);
        gear_varied_.insert(bot);
        if (!gear_varied_raid_.insert(bot).second)
            return;
    }
    Db().DirectPExecute("UPDATE playerbot_v2_character SET gear_varied = 2 WHERE character_guid_low = {}", bot);
}

bool OwnerRegistry::GearVaried(BotId bot) const
{
    std::shared_lock lk(mtx_);
    return gear_varied_.contains(bot);
}

void OwnerRegistry::MarkGearVaried(BotId bot)
{
    {
        std::unique_lock lk(mtx_);
        if (!gear_varied_.insert(bot).second)
            return;
    }
    Db().DirectPExecute("UPDATE playerbot_v2_character SET gear_varied = 1 WHERE character_guid_low = {}", bot);
}

uint8 OwnerRegistry::OwnerGearMode(uint32 account_id) const
{
    std::shared_lock lk(mtx_);
    auto it = owner_gear_mode_.find(account_id);
    return it == owner_gear_mode_.end() ? 0 : it->second;
}

void OwnerRegistry::SetOwnerGearMode(uint32 account_id, uint8 mode)
{
    {
        std::unique_lock lk(mtx_);
        auto const it = owner_gear_mode_.find(account_id);
        if ((it == owner_gear_mode_.end() ? uint8(0) : it->second) == mode && it != owner_gear_mode_.end())
            return;   // unchanged: a group order reaches every bot of the owner
        owner_gear_mode_[account_id] = mode;
    }
    Db().DirectPExecute("REPLACE INTO playerbot_v2_owner_gear (owner_account_id, gear_mode) VALUES ({}, {})", account_id, uint32(mode));
}

bool OwnerRegistry::CcEnabled(uint32 account_id) const
{
    std::shared_lock lk(mtx_);
    return !cc_disabled_.contains(account_id);
}

bool OwnerRegistry::SetCcEnabled(uint32 account_id, bool enabled)
{
    std::unique_lock lk(mtx_);
    return enabled ? cc_disabled_.erase(account_id) > 0 : cc_disabled_.insert(account_id).second;
}

Role OwnerRegistry::DeclaredRole(BotId bot) const
{
    std::shared_lock lk(mtx_);
    auto it = declared_role_.find(bot);
    return it == declared_role_.end() ? Role::Unknown : it->second;
}

void OwnerRegistry::SetDeclaredRole(BotId bot, Role role)
{
    {
        std::unique_lock lk(mtx_);
        auto it = declared_role_.find(bot);
        if (role == Role::Unknown)
        {
            if (it == declared_role_.end())
                return;
            declared_role_.erase(it);
        }
        else
        {
            if (it != declared_role_.end() && it->second == role)
                return;
            declared_role_[bot] = role;
        }
    }
    Db().DirectPExecute("UPDATE playerbot_v2_character SET companion_role = {} WHERE character_guid_low = {}",
        role == Role::Unknown ? 0u : uint32(role) + 1u, bot);
}

uint8 OwnerRegistry::StoredSubgroup(BotId bot) const
{
    std::shared_lock lk(mtx_);
    auto const it = subgroup_.find(bot);
    return it == subgroup_.end() ? uint8(0xFF) : it->second;
}

void OwnerRegistry::SetStoredSubgroup(BotId bot, uint8 subgroup)
{
    {
        std::unique_lock lk(mtx_);
        auto const it = subgroup_.find(bot);
        if (subgroup > 7)
        {
            if (it == subgroup_.end())
                return;
            subgroup_.erase(it);
        }
        else
        {
            if (it != subgroup_.end() && it->second == subgroup)
                return;
            subgroup_[bot] = subgroup;
        }
    }
    Db().DirectPExecute("UPDATE playerbot_v2_character SET companion_subgroup = {} WHERE character_guid_low = {}",
        subgroup > 7 ? 0u : uint32(subgroup) + 1u, bot);
}

uint16 OwnerRegistry::StoredOrder(BotId bot) const
{
    std::shared_lock lk(mtx_);
    auto const it = order_.find(bot);
    return it == order_.end() ? uint16(0xFFFF) : it->second;
}

void OwnerRegistry::SetStoredOrder(BotId bot, uint16 order)
{
    {
        std::unique_lock lk(mtx_);
        auto const it = order_.find(bot);
        if (order > 254)
        {
            if (it == order_.end())
                return;
            order_.erase(it);
        }
        else
        {
            if (it != order_.end() && it->second == order)
                return;
            order_[bot] = order;
        }
    }
    Db().DirectPExecute("UPDATE playerbot_v2_character SET companion_order = {} WHERE character_guid_low = {}",
        order > 254 ? 0u : uint32(order) + 1u, bot);
}

uint32 OwnerRegistry::StoredSpec(BotId bot) const
{
    std::shared_lock lk(mtx_);
    auto const it = spec_.find(bot);
    return it == spec_.end() ? 0u : it->second;
}

void OwnerRegistry::SetStoredSpec(BotId bot, uint32 spec)
{
    {
        std::unique_lock lk(mtx_);
        auto const it = spec_.find(bot);
        if (spec == 0)
        {
            if (it == spec_.end())
                return;
            spec_.erase(it);
        }
        else
        {
            if (it != spec_.end() && it->second == spec)
                return;
            spec_[bot] = spec;
        }
    }
    Db().DirectPExecute("UPDATE playerbot_v2_character SET companion_spec = {} WHERE character_guid_low = {}", spec, bot);
}

bool OwnerRegistry::IsOwner(BotId bot, uint32 sender_account, uint64 sender_guid) const
{
    OwnerBinding const b = GetOwner(bot);
    if (b.account_id == 0) return false;          // unowned
    if (b.account_id != sender_account) return false;
    if (b.player_guid != 0 && b.player_guid != sender_guid) return false;
    return true;
}

OwnerRegistry::SquadState OwnerRegistry::LoadSquadState(BotId bot) const
{
    auto result = Db().PQuery(
        "SELECT {} FROM playerbot_v2_character WHERE character_guid_low = {}",
        kSquadStateColumns, bot);
    if (!result) return SquadState{};
    return SquadStateFromFields(result->Fetch());
}

OwnerRegistry::SquadState OwnerRegistry::SquadStateFromFields(Field const* f)
{
    SquadState out;
    out.formation_type   = f[0].GetUInt8();
    out.formation_slot   = f[1].GetUInt8();
    out.follow_distance  = f[2].GetFloat();
    out.owner_verbose    = f[3].GetBool();
    return out;
}

void OwnerRegistry::SaveSquadState(BotId bot, SquadState const& s) const
{
    Db().DirectPExecute(
        "UPDATE playerbot_v2_character "
        "SET formation_type = {}, formation_slot = {}, "
        "    follow_distance_yd = {:.2f}, owner_verbose = {} "
        "WHERE character_guid_low = {}",
        s.formation_type, s.formation_slot,
        s.follow_distance, s.owner_verbose ? 1 : 0,
        bot);
}

size_t OwnerRegistry::SaveSquadPreset(uint32 owner_account,
                                       std::string const& preset_name) const
{
    if (owner_account == 0 || preset_name.empty()) return 0;
    std::vector<BotId> const owned = BotsOwnedBy(owner_account);
    if (owned.empty()) return 0;
    // Compose the payload: `bot:type:slot:fd|bot:type:slot:fd|...`.
    // No JSON parser dependency; the format is fixed and round-trips
    // exactly via std::stoull / std::stof on Load.
    std::string payload;
    payload.reserve(owned.size() * 24);
    for (BotId bot : owned)
    {
        SquadState const s = LoadSquadState(bot);
        if (!payload.empty()) payload.push_back('|');
        char buf[64];
        const int n = snprintf(buf, sizeof(buf),
            "%llu:%u:%u:%.2f",
            static_cast<unsigned long long>(bot),
            unsigned(s.formation_type),
            unsigned(s.formation_slot),
            double(s.follow_distance));
        payload.append(buf, buf + (n > 0 ? n : 0));
    }
    // Upsert (REPLACE semantics — owner may overwrite a preset).
    // EscapeString on preset_name: owner-supplied input that may contain
    // apostrophes. payload is `bot:type:slot:fd|...` format — digits +
    // colons + pipes only, no escape needed.
    std::string esc_name = preset_name;
    Db().EscapeString(esc_name);
    Db().DirectPExecute(
        "REPLACE INTO playerbot_v2_squad_preset "
        "(owner_account_id, preset_name, payload_json) "
        "VALUES ({}, '{}', '{}')",
        owner_account, esc_name, payload);
    return owned.size();
}

size_t OwnerRegistry::LoadSquadPreset(uint32 owner_account,
                                       std::string const& preset_name) const
{
    if (owner_account == 0 || preset_name.empty()) return 0;
    std::string esc_name = preset_name;
    Db().EscapeString(esc_name);
    auto result = Db().PQuery(
        "SELECT payload_json FROM playerbot_v2_squad_preset "
        "WHERE owner_account_id = {} AND preset_name = '{}'",
        owner_account, esc_name);
    if (!result) return 0;
    Field* f = result->Fetch();
    std::string const payload = f[0].GetString();
    if (payload.empty()) return 0;
    // Parse: split by '|', then ':' inside each. Skip entries whose
    // bot is no longer owned by this account (owner may have
    // disowned mid-preset; loading would otherwise stomp another
    // owner's bots).
    std::unordered_set<BotId> still_owned;
    for (BotId b : BotsOwnedBy(owner_account)) still_owned.insert(b);

    size_t applied = 0;
    size_t pos = 0;
    while (pos < payload.size())
    {
        size_t end = payload.find('|', pos);
        if (end == std::string::npos) end = payload.size();
        std::string const part = payload.substr(pos, end - pos);
        pos = end + 1;
        // Parse bot:type:slot:fd
        size_t a = part.find(':');
        if (a == std::string::npos) continue;
        size_t b = part.find(':', a + 1);
        if (b == std::string::npos) continue;
        size_t c = part.find(':', b + 1);
        if (c == std::string::npos) continue;
        try
        {
            BotId bot = std::stoull(part.substr(0, a));
            uint8 ftype = static_cast<uint8>(std::stoul(part.substr(a + 1, b - a - 1)));
            uint8 fslot = static_cast<uint8>(std::stoul(part.substr(b + 1, c - b - 1)));
            float fd    = std::stof(part.substr(c + 1));
            if (!still_owned.count(bot)) continue;
            SquadState s;
            s.formation_type   = ftype;
            s.formation_slot   = fslot;
            s.follow_distance  = fd;
            s.owner_verbose    = LoadSquadState(bot).owner_verbose; // preserve verbose
            SaveSquadState(bot, s);
            ++applied;
        }
        catch (std::exception const&) { /* malformed entry — skip */ }
    }
    return applied;
}

std::vector<BotId> OwnerRegistry::BotsOwnedBy(uint32 account_id) const
{
    std::vector<BotId> out;
    if (account_id == 0) return out;
    std::shared_lock lk(mtx_);
    out.reserve(owners_.size() / 16 + 1);
    for (auto const& [bot, binding] : owners_)
        if (binding.account_id == account_id)
            out.push_back(bot);
    return out;
}

} // namespace Playerbot
