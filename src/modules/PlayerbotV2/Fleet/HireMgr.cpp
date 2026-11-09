#include "HireMgr.h"
#include "../Bot/ClassTables.h"
#include "../Bot/Gear/OwnedGearPolicy.h"
#include "Cell.h"
#include "CellImpl.h"
#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Timer.h"
#include "Group.h"
#include "InstanceGroupCap.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "../Services.h"
#include "WorldSession.h"
#include "OwnerRegistry.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <list>
#include <mutex>

namespace Playerbot::V2::Fleet {

namespace {
auto& Db() { return CharacterDatabase; }

uint64 PriceKey(char const* key, uint64 def)
{
    int64 const v = sConfigMgr->GetInt64Default(key, int64(def), true);
    return v < 0 ? 0 : uint64(v);
}
}

HireMgr& HireMgr::Instance()
{
    static HireMgr instance;
    return instance;
}

Hire::Mode HireMgr::ConfigMode()
{
    int32 const m = sConfigMgr->GetIntDefault("Playerbot.Hire.Mode", 0, true);
    return (m == 1 || m == 2) ? Hire::Mode(m) : Hire::Mode::Off;
}

Hire::Prices HireMgr::ConfigPrices()
{
    Hire::Prices p;
    using Hire::Grade;
    p.grade[uint8(Grade::Sellsword)].party = PriceKey("Playerbot.Hire.Sellsword.PartyPrice", p.grade[uint8(Grade::Sellsword)].party);
    p.grade[uint8(Grade::Sellsword)].raid  = PriceKey("Playerbot.Hire.Sellsword.RaidPrice",  p.grade[uint8(Grade::Sellsword)].raid);
    p.grade[uint8(Grade::Veteran)].party   = PriceKey("Playerbot.Hire.Veteran.PartyPrice",   p.grade[uint8(Grade::Veteran)].party);
    p.grade[uint8(Grade::Veteran)].raid    = PriceKey("Playerbot.Hire.Veteran.RaidPrice",    p.grade[uint8(Grade::Veteran)].raid);
    p.grade[uint8(Grade::Legend)].party    = PriceKey("Playerbot.Hire.Legend.PartyPrice",    p.grade[uint8(Grade::Legend)].party);
    p.grade[uint8(Grade::Legend)].raid     = PriceKey("Playerbot.Hire.Legend.RaidPrice",     p.grade[uint8(Grade::Legend)].raid);
    p.upkeep_pct = uint32(std::clamp(sConfigMgr->GetIntDefault("Playerbot.Hire.UpkeepPct", int32(p.upkeep_pct), true), 0, 1000));
    return p;
}

Hire::Rules HireMgr::ConfigRules()
{
    Hire::Rules r;
    using Hire::Grade;
    struct GradeKeys { Grade grade; char const* gear; char const* health; };
    static constexpr GradeKeys kGrades[] = {
        { Grade::Sellsword, "Playerbot.Hire.Sellsword.Gear", "Playerbot.Hire.Sellsword.HealthPct" },
        { Grade::Veteran,   "Playerbot.Hire.Veteran.Gear",   "Playerbot.Hire.Veteran.HealthPct" },
        { Grade::Legend,    "Playerbot.Hire.Legend.Gear",    "Playerbot.Hire.Legend.HealthPct" },
    };
    for (GradeKeys const& k : kGrades)
    {
        uint8 const g = uint8(k.grade);
        std::string word = sConfigMgr->GetStringDefault(k.gear, "", true);
        std::transform(word.begin(), word.end(), word.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        Gear::Owned::GearMode mode;
        if (!word.empty() && Gear::Owned::ParseGearMode(word, mode))
            r.gear_mode[g] = int(mode);
        else if (!word.empty())
            TC_LOG_ERROR("playerbot.v2", "[hire] {} = \"{}\" is no gear mode (match, best, bis, t1, t2, t3): the default stays", k.gear, word);
        int32 const pct = sConfigMgr->GetIntDefault(k.health, int32(r.health_pct[g]), true);
        r.health_pct[g] = uint32(std::clamp<int32>(pct, int32(Hire::kMinHealthPct), int32(Hire::kMaxHealthPct)));
    }
    struct BrokerKey { Hire::Broker broker; char const* key; };
    static constexpr BrokerKey kBrokers[] = {
        { Hire::Broker::Inn,         "Playerbot.Hire.Inn.Sells" },
        { Hire::Broker::Capital,     "Playerbot.Hire.Capital.Sells" },
        { Hire::Broker::BlackMarket, "Playerbot.Hire.BlackMarket.Sells" },
    };
    for (BrokerKey const& k : kBrokers)
    {
        // A missing line keeps the default; a line that is there (even empty or "none") is the server owner's list.
        std::string const unset = "\x01";
        std::string const list = sConfigMgr->GetStringDefault(k.key, unset, true);
        if (list != unset)
            r.sells[uint8(k.broker)] = Hire::ParseGradeList(list);
    }
    return r;
}

float HireMgr::BrokerRange()
{
    return std::clamp(sConfigMgr->GetFloatDefault("Playerbot.Hire.BrokerRange", 15.0f, true), 3.0f, 100.0f);
}

uint32 HireMgr::MaxPerAccount()
{
    return uint32(std::max(sConfigMgr->GetIntDefault("Playerbot.Hire.MaxPerAccount", 0, true), 0));
}

uint32 HireMgr::LegendMaxPerAccount()
{
    return uint32(std::max(sConfigMgr->GetIntDefault("Playerbot.Hire.Legend.MaxPerAccount", 0, true), 0));
}

void HireMgr::LoadFromDb()
{
    auto rows = Db().Query("SELECT character_guid_low, hire_grade, hire_price, hire_paid_until FROM playerbot_v2_character WHERE hire_grade != 0");
    auto prefs = Db().Query("SELECT account_id FROM playerbot_v2_hire_pref WHERE hired_only != 0");
    std::unique_lock lk(mtx_);
    hired_.clear();
    opted_in_.clear();
    loaned_.clear();
    if (rows)
        do
        {
            Field* f = rows->Fetch();
            uint8 const g = f[1].GetUInt8();
            if (g == Hire::kLoanGradeDb)
            {
                loaned_[f[0].GetUInt64()] = 0;
                continue;
            }
            if (g == 0 || g >= Hire::kGradeCount)
                continue;
            hired_[f[0].GetUInt64()] = HireRecord{ Hire::Grade(g), f[2].GetUInt64(), f[3].GetUInt64() };
        } while (rows->NextRow());
    if (prefs)
        do { opted_in_.insert(prefs->Fetch()[0].GetUInt32()); } while (prefs->NextRow());
    TC_LOG_INFO("playerbot.v2", "[hire] mode={} hired bots={} finder loan bots={} accounts choosing hired bots={}", uint32(ConfigMode()),
                hired_.size(), loaned_.size(), opted_in_.size());
}

bool HireMgr::OptedIn(uint32 account_id) const
{
    std::shared_lock lk(mtx_);
    return opted_in_.contains(account_id);
}

void HireMgr::SetOptedIn(uint32 account_id, bool on)
{
    {
        std::unique_lock lk(mtx_);
        if (on ? !opted_in_.insert(account_id).second : opted_in_.erase(account_id) == 0)
            return;
    }
    Db().DirectPExecute("REPLACE INTO playerbot_v2_hire_pref (account_id, hired_only) VALUES ({}, {})", account_id, on ? 1 : 0);
}

bool HireMgr::MustHire(Player const* player) const
{
    if (!player || !player->GetSession() || player->GetSession()->IsBot())
        return false;
    bool const gm = player->GetSession()->GetSecurity() > SEC_PLAYER;
    bool const bypass = sConfigMgr->GetBoolDefault("Playerbot.Hire.GmBypass", false, true);
    return Hire::Required(ConfigMode(), OptedIn(player->GetSession()->GetAccountId()), gm, bypass);
}

bool HireMgr::MustHireAccount(uint32 account_id) const
{
    if (!account_id)
        return false;
    return Hire::Required(ConfigMode(), OptedIn(account_id), false, false);
}

HireRecord HireMgr::RecordOf(BotId bot) const
{
    std::shared_lock lk(mtx_);
    auto const it = hired_.find(bot);
    return it == hired_.end() ? HireRecord{} : it->second;
}

void HireMgr::SetHired(BotId bot, Hire::Grade grade, uint64 price)
{
    uint64 const until = uint64(GameTime::GetGameTime()) + Hire::kDaySeconds;
    {
        std::unique_lock lk(mtx_);
        hired_[bot] = HireRecord{ grade, price, until };
    }
    Db().DirectPExecute("UPDATE playerbot_v2_character SET hire_grade = {}, hire_price = {}, hire_paid_until = {} WHERE character_guid_low = {}",
                        uint32(grade), price, until, bot);
}

void HireMgr::Forget(BotId bot)
{
    {
        std::unique_lock lk(mtx_);
        bool const was_hired = hired_.erase(bot) != 0;
        bool const was_loaned = loaned_.erase(bot) != 0;
        if (!was_hired && !was_loaned)
            return;
        called_ms_.erase(bot);
    }
    Db().DirectPExecute("UPDATE playerbot_v2_character SET hire_grade = 0, hire_price = 0, hire_paid_until = 0 WHERE character_guid_low = {}", bot);
}

void HireMgr::SetLoaned(BotId bot, uint32 now_ms)
{
    {
        std::unique_lock lk(mtx_);
        hired_.erase(bot);
        loaned_[bot] = now_ms ? now_ms : 1;
    }
    Db().DirectPExecute("UPDATE playerbot_v2_character SET hire_grade = {}, hire_price = 0, hire_paid_until = 0 WHERE character_guid_low = {}",
                        uint32(Hire::kLoanGradeDb), bot);
}

bool HireMgr::IsLoaned(BotId bot) const
{
    std::shared_lock lk(mtx_);
    return loaned_.contains(bot);
}

bool HireMgr::LoanSettled(BotId bot, uint32 now_ms) const
{
    std::shared_lock lk(mtx_);
    auto const it = loaned_.find(bot);
    return it != loaned_.end() && (it->second == 0 || getMSTimeDiff(it->second, now_ms) >= Hire::kLoanGraceMs);
}

std::vector<BotId> HireMgr::LoanedBots() const
{
    std::shared_lock lk(mtx_);
    std::vector<BotId> out;
    out.reserve(loaned_.size());
    for (auto const& [id, _] : loaned_)
        out.push_back(id);
    return out;
}

uint32 HireMgr::CountGrade(uint32 account_id, Hire::Grade grade) const
{
    uint32 n = 0;
    for (BotId id : Services::Owners().BotsOwnedBy(account_id))
        if (RecordOf(id).grade == grade)
            ++n;
    return n;
}

int HireMgr::GearModeOverride(BotId bot, uint32 owner_account) const
{
    Hire::Grade const grade = RecordOf(bot).grade;
    if (grade == Hire::Grade::None && ConfigMode() == Hire::Mode::Off)
        return -1;
    bool must = MustHireAccount(owner_account);
    // A GM's own companions keep the GM's gear choice (testing), as long as the GM is online to be asked.
    if (must)
        if (uint64 const owner_guid = Services::Owners().OwnerCharacter(bot))
            if (Player const* owner = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(owner_guid)))
                must = MustHire(owner);
    return Hire::GearModeFor(ConfigRules(), grade, must);
}

Creature* HireMgr::NearbyBroker(Player* player, Hire::Grade grade) const
{
    if (!player || !player->IsInWorld())
        return nullptr;
    float const range = BrokerRange();
    Hire::Rules const rules = ConfigRules();
    std::list<Creature*> near;
    Trinity::AnyUnitInObjectRangeCheck check(player, range);
    Trinity::CreatureListSearcher<Trinity::AnyUnitInObjectRangeCheck> searcher(player, near, check);
    Cell::VisitGridObjects(player, searcher, range);
    for (Creature* c : near)
        if (c && c->IsAlive() && Hire::BrokerSells(rules, Hire::BrokerOfEntry(c->GetEntry()), grade))
            return c;
    return nullptr;
}

void HireMgr::ApplyHealth(Player* bot)
{
    if (!bot || !bot->IsInWorld())
        return;
    BotId const id = bot->GetGUID().GetCounter();
    float given = 0.0f;
    if (auto const it = health_bonus_.find(id); it != health_bonus_.end() && it->second.who == bot)
        given = it->second.amount;
    uint32 const pct = Hire::HealthPctFor(ConfigRules(), RecordOf(id).grade);
    // The bot's health without the bonus, at its class, level and gear now (the bonus follows a level-up or new gear here).
    float const normal = std::max(float(bot->GetMaxHealth()) - given, 1.0f);
    float want = float(Hire::HealthBonusFor(uint64(normal), pct));
    if (normal + want < 1.0f)
        want = 1.0f - normal;
    if (std::abs(want - given) < 1.0f)
    {
        if (given == 0.0f)
            health_bonus_.erase(id);
        return;
    }
    bool const was_full = bot->IsAlive() && bot->GetHealth() >= bot->GetMaxHealth();
    if (given != 0.0f)
        bot->HandleStatFlatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, given, false);
    if (want != 0.0f)
        bot->HandleStatFlatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, want, true);
    if (was_full && !bot->IsInCombat())
        bot->SetFullHealth();
    if (want != 0.0f)
        health_bonus_[id] = HealthBonus{ bot, want };
    else
        health_bonus_.erase(id);
    TC_LOG_DEBUG("playerbot.v2", "[hire] {} health {}% ({:+.0f}, max {})", bot->GetName(), pct, want, bot->GetMaxHealth());
}

bool HireMgr::RaidPlace(Player* player)
{
    if (!player)
        return false;
    if (Group const* g = player->GetGroup(); g && g->isRaidGroup())
        return true;
    Map const* map = player->GetMap();
    return map && map->IsDungeon() && InstanceGroupCapFor(player) > 5;
}

uint64 HireMgr::PriceHere(Player* player, Hire::Grade grade)
{
    if (!player)
        return 0;
    return Hire::PriceFor(ConfigPrices(), grade, player->GetLevel(), MaxPlayerLevel(), RaidPlace(player));
}

bool HireMgr::TakeUpkeep(Player* owner, BotId bot, std::string const& bot_name, std::string& why)
{
    HireRecord const rec = RecordOf(bot);
    uint64 const now = uint64(GameTime::GetGameTime());
    if (rec.grade == Hire::Grade::None || !Hire::UpkeepDue(rec.paid_until, now) || !owner || !MustHire(owner))
        return true;
    uint64 const fee = Hire::UpkeepFor(ConfigPrices(), rec.price);
    if (fee && !owner->HasEnoughMoney(fee))
    {
        why = bot_name + " wants a day's wages first (" + Hire::MoneyText(fee) + ").";
        return false;
    }
    if (fee)
        owner->ModifyMoney(-int64(fee));
    uint64 const until = now + Hire::kDaySeconds;
    {
        std::unique_lock lk(mtx_);
        if (auto it = hired_.find(bot); it != hired_.end())
            it->second.paid_until = until;
    }
    Db().DirectPExecute("UPDATE playerbot_v2_character SET hire_paid_until = {} WHERE character_guid_low = {}", until, bot);
    if (fee && owner->GetSession())
        ChatHandler(owner->GetSession()).PSendSysMessage("You paid %s %s for today.", bot_name.c_str(), Hire::MoneyText(fee).c_str());
    TC_LOG_INFO("playerbot.v2", "[hire] {} paid {} copper upkeep for {}", owner->GetName(), fee, bot_name);
    return true;
}

void HireMgr::NoteCalled(BotId bot, uint32 now_ms)
{
    std::unique_lock lk(mtx_);
    called_ms_[bot] = now_ms;
}

bool HireMgr::RecentlyCalled(BotId bot, uint32 now_ms) const
{
    std::shared_lock lk(mtx_);
    auto const it = called_ms_.find(bot);
    return it != called_ms_.end() && now_ms - it->second < 30000u;
}

}  // namespace Playerbot::V2::Fleet
