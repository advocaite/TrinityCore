#include "BotSession.h"
#include "DatabaseEnv.h"
#include "LoginQueryHolder.h"
#include "Log.h"
#include "Group.h"
#include "Player.h"
#include "WorldPacket.h"
#include "WorldSocket.h"

#include "ClientBuildInfo.h"
#include "ObjectMgr.h"
#include "BattlenetAccountMgr.h"
#include "../Services.h"
#include "../Util/ConfigReader.h"

#include <mutex>
#include <unordered_map>

namespace Playerbot::V2 {

namespace {
// The battlenet account a real login of this game account would carry (account.battlenet_account,
// provisioned by BotAccountMgr::BackfillBnet). A name query for the bot, the PlayerData BnetAccount
// field and every per-bnet-account save key on this id; the previous "bnet id = game account id"
// gave bots a BnetAccount that no auth.battlenet_accounts row owns (or worse, one that belongs to
// another player), so the client's whisper/popup checks saw a bot that does not exist on any account.
uint32 ResolveBnetAccountId(uint32 gameAccountId)
{
    if (!Services::Config().behavior_bot_whisperable())
        return gameAccountId;
    static std::mutex mtx;
    static std::unordered_map<uint32, uint32> cache;
    {
        std::lock_guard lk(mtx);
        auto it = cache.find(gameAccountId);
        if (it != cache.end())
            return it->second;
    }
    uint32 bnet = ::Battlenet::AccountMgr::GetIdByGameAccount(gameAccountId);
    if (!bnet)
        return gameAccountId; // no link yet (BackfillBnet not run): keep the legacy value rather than 0
    std::lock_guard lk(mtx);
    cache[gameAccountId] = bnet;
    return bnet;
}
} // namespace

BotSession::BotSession(uint32 accountId, std::string name)
    : WorldSession(
        accountId,                              // account id (from `characters.account`)
        std::move(name),                        // account name (looked up by mgr)
        ResolveBnetAccountId(accountId),        // the linked battlenet account (a name query / whisper entry keys on it)
        std::string(),                          // bnet email
        std::shared_ptr<WorldSocket>(),         // null socket (the V2 trick)
        SEC_PLAYER,                             // security: player tier
        EXPANSION_LEVEL_CURRENT,                // current expansion
        0,                                      // mute time
        std::string(),                          // OS
        Minutes(0),                             // timezone offset
        0,                                      // client build (irrelevant — no client)
        ClientBuild::VariantId{},               // build variant
        LOCALE_enUS,                            // locale
        0,                                      // recruiter
        false,                                  // is recruiter
        true)                                   // is_bot=true → IsBot() returns true
{
    // Kick off the per-realm + login account-info holders so toys / heirlooms
    // / mounts / item appearances / transmog illusions / transmog outfits /
    // warband scenes / per-character account-data and tutorials are loaded
    // before (or shortly after) the player enters world. Normal sessions get
    // this from World::AddSession_; bots bypass that path because BotSessionMgr
    // keeps them outside sWorld->m_sessions (no MaxPlayers / queue interaction
    // and no auth-success packet flurry through our dropped SendPacket).
    //
    // The InitializeSessionCallback emits a flurry of handshake packets
    // (SendAuthResponse, SendSetTimeZoneInformation, SendFeatureSystemStatus,
    // hotfix list, account-data times, tutorials, bnet ConnectionStatus) —
    // all dropped by our SendPacket override. The data loads themselves
    // (LoadAccountToys, LoadAccountMounts, etc.) hit CollectionMgr, which
    // is constructed by the WorldSession base, so they run safely.
    InitializeSession();
}

bool BotSession::BeginLogin(ObjectGuid characterGuid)
{
    if (PlayerLoading() || GetPlayer())
    {
        TC_LOG_WARN("playerbot.v2", "[BotSession] BeginLogin called while already loading/loaded for account {}",
                    GetAccountId());
        return false;
    }

    // Mirrors WorldSession::HandleContinuePlayerLogin: stash the in-flight
    // guid, build a LoginQueryHolder, submit it, and let the callback run
    // the existing HandlePlayerLogin path which spawns the Player in-world.
    m_playerLoading = characterGuid;

    auto holder = std::make_shared<LoginQueryHolder>(GetAccountId(), characterGuid);
    if (!holder->Initialize())
    {
        TC_LOG_ERROR("playerbot.v2", "[BotSession] LoginQueryHolder::Initialize failed for character {}",
                     characterGuid.ToString());
        m_playerLoading.Clear();
        return false;
    }

    AddQueryHolderCallback(CharacterDatabase.DelayQueryHolder(holder)).AfterComplete(
        [this](SQLQueryHolderBase const& done) {
            HandlePlayerLogin(static_cast<LoginQueryHolder const&>(done));
        });

    TC_LOG_INFO("playerbot.v2", "[BotSession] Async login submitted for character {} (account {})",
                characterGuid.ToString(), GetAccountId());
    return true;
}

void BotSession::SendPacket(WorldPacket const* /*packet*/, bool /*forced*/)
{
    // No client → drop. Base implementation would push to the socket queue
    // which is null for bots. ScriptMgr::OnPacketSend hooks fire elsewhere
    // for normal sessions; bots intentionally stay invisible to that pipeline.
}

bool BotSession::Update(uint32 /*diff*/, PacketFilter& /*updater*/)
{
    // Base WorldSession::Update walks the socket recv queue and ends with
    // `if (!m_Socket[REALM]) return false;` — which would fire every tick
    // for a socketless bot, causing the mgr to reap us immediately. So we
    // run the parts that DO matter (query holder callbacks for the async
    // login + any deferred work) and return true.
    ProcessQueryCallbacks();

    // Logout request: KickPlayer (our override) sets forceExit. We do the
    // standard logout dance here and return false so BotSessionMgr drops
    // the session. Mirrors what the base does in its cleanup block when an
    // open socket has been closed.
    if (forceExit)
    {
        if (_player && !m_playerLogout)
        {
            // A bot that leaves the world must not stay on a battleground group's member list as an "Offline" ghost (the owner,
            // 2026-10-08: five logged-out BG bots shown Offline in his frames). Leave the BG group first.
            if (Group* group = _player->GetGroup(); group && group->isBGGroup())
            {
                TC_LOG_INFO("playerbot.v2", "[bot_logout] {} leaves its battleground group before logging out", _player->GetName());
                group->RemoveMember(_player->GetGUID(), GROUP_REMOVEMETHOD_LEAVE);
            }
            LogoutPlayer(true);
        }
        return false;
    }
    return true;
}

void BotSession::KickPlayer(std::string_view reason)
{
    TC_LOG_INFO("playerbot.v2", "[BotSession] KickPlayer: account={} reason='{}'",
                GetAccountId(), reason);
    forceExit = true;
}

} // namespace Playerbot::V2
