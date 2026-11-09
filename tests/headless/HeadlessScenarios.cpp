#include "tc_catch2.h"
#include "HeadlessClient.h"
#include "ByteBuffer.h"
#include "ObjectGuid.h"
#include "Opcodes.h"
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>

// Headless scenarios against a running test realm. Nothing here runs without the environment below (every scenario is skipped), so the
// executable is safe to start anywhere.
//
//   HEADLESS_ACCOUNT      game account name of the seeded account ("900001#1", see sql/custom/headless)
//   HEADLESS_KEY          its session_key_bnet as hex (128 hex digits)
//   HEADLESS_HOST / HEADLESS_PORT   world server (default 127.0.0.1 / 8085)
//   HEADLESS_CHARACTER    low guid of a character of that account (login, party, trade, inspect, whisper scenarios)
//   HEADLESS_BOT_NAME / HEADLESS_BOT_GUID   a bot of the test realm that is online (party, trade, inspect, whisper scenarios)
//   HEADLESS_PLATFORM / HEADLESS_ARCH / HEADLESS_TYPE   ids of the join ticket (default 0)
//
// Every scenario ends by running everything it received through the reader specs of the real client (Client::CheckLog).
namespace
{
using namespace Headless;

std::string Env(char const* name, std::string fallback = { })
{
    char const* value = std::getenv(name);
    return value && *value ? value : fallback;
}

struct Config
{
    std::string Host;
    uint16 Port = 0;
    Credentials Account;
    uint64 Character = 0;
    std::string BotName;
    uint64 BotGuid = 0;
    bool Valid = false;
};

std::vector<uint8> FromHex(std::string const& hex)
{
    std::vector<uint8> out;
    for (size_t i = 0; i + 1 < hex.size(); i += 2)
        out.push_back(uint8(std::stoul(hex.substr(i, 2), nullptr, 16)));
    return out;
}

Config const& GetConfig()
{
    static Config const config = []
    {
        Config c;
        c.Account.GameAccount = Env("HEADLESS_ACCOUNT");
        c.Account.KeyData = FromHex(Env("HEADLESS_KEY"));
        c.Host = Env("HEADLESS_HOST", "127.0.0.1");
        c.Port = uint16(std::stoul(Env("HEADLESS_PORT", "8085")));
        c.Account.Platform = uint32(std::stoul(Env("HEADLESS_PLATFORM", "0")));
        c.Account.Arch = uint32(std::stoul(Env("HEADLESS_ARCH", "0")));
        c.Account.Type = uint32(std::stoul(Env("HEADLESS_TYPE", "0")));
        c.Character = std::stoull(Env("HEADLESS_CHARACTER", "0"));
        c.BotName = Env("HEADLESS_BOT_NAME");
        c.BotGuid = std::stoull(Env("HEADLESS_BOT_GUID", "0"));
        c.Valid = !c.Account.GameAccount.empty() && c.Account.KeyData.size() == 64;
        return c;
    }();
    return config;
}

#define REQUIRE_REALM() \
    if (!GetConfig().Valid) \
        SKIP("no test realm: set HEADLESS_ACCOUNT and HEADLESS_KEY")

#define REQUIRE_CHARACTER() \
    REQUIRE_REALM(); \
    if (!GetConfig().Character) \
        SKIP("set HEADLESS_CHARACTER")

#define REQUIRE_BOT() \
    REQUIRE_CHARACTER(); \
    if (GetConfig().BotName.empty() || !GetConfig().BotGuid) \
        SKIP("set HEADLESS_BOT_NAME and HEADLESS_BOT_GUID")

// ---- client packets in the layout our server reads (see the Read() functions named in the comments) ----

ObjectGuid PlayerGuid(uint64 low)
{
    return ObjectGuid::Create<HighGuid::Player>(low);
}

ByteBuffer PlayerLoginPayload(uint64 low)           // PlayerLogin::Read: guid, float far clip, 1 bit
{
    ByteBuffer buffer;
    buffer << PlayerGuid(low);
    buffer << float(0.0f);
    buffer.WriteBit(false);
    buffer.FlushBits();
    return buffer;
}

ByteBuffer PartyInvitePayload(std::string const& name)  // PartyInviteClient::Read
{
    ByteBuffer buffer;
    buffer.WriteBit(false);                         // no party index
    buffer.FlushBits();
    buffer.WriteBits(name.size(), 9);               // target name
    buffer.WriteBits(0, 9);                         // target realm
    buffer.FlushBits();
    buffer << uint32(0);                            // realm address
    buffer << ObjectGuid::Empty;                    // target guid
    buffer.append(reinterpret_cast<uint8 const*>(name.data()), name.size());
    return buffer;
}

ByteBuffer GuidPayload(ObjectGuid guid)             // InitiateTrade::Read, Inspect::Read
{
    ByteBuffer buffer;
    buffer << guid;
    return buffer;
}

ByteBuffer WhisperPayload(std::string const& target, std::string const& text)   // ChatMessageWhisper::Read
{
    ByteBuffer buffer;
    buffer << int32(7);                             // common
    buffer << ObjectGuid::Empty;
    buffer << uint32(0);
    buffer.WriteBits(target.size(), 9);
    buffer.WriteBits(0, 9);
    buffer.WriteBits(text.size(), 11);
    buffer.FlushBits();
    buffer.append(reinterpret_cast<uint8 const*>(target.data()), target.size());
    buffer.append(reinterpret_cast<uint8 const*>(text.data()), text.size());
    return buffer;
}

std::string Describe(SpecCoverage const& coverage)
{
    std::ostringstream out;
    out << coverage.Checked << " packets checked against the client's reader, " << coverage.Unresolved << " with an undecoded field, "
        << coverage.Undecrypted << " not decrypted in the dump, " << coverage.NoSpec << " outside the main group";
    return out.str();
}

void RequireClean(Client& client)
{
    SpecCoverage const coverage = client.CheckLog();
    INFO(Describe(coverage));
    for (SpecIssue const& issue : coverage.Issues)
        UNSCOPED_INFO(issue.Name << " (wire 0x" << std::hex << issue.Wire << std::dec << "): " << issue.Problem);
    CHECK(coverage.Issues.empty());
}

// Authenticates and enters the world with HEADLESS_CHARACTER; the whole login burst stays in the client's log.
void EnterWorld(Client& client)
{
    Config const& config = GetConfig();
    REQUIRE(client.Authenticate(config.Account) == 0);

    client.Send(CMSG_ENUM_CHARACTERS);
    REQUIRE(client.WaitFor(SMSG_ENUM_CHARACTERS_RESULT, Milliseconds(15000)).has_value());

    client.Send(CMSG_PLAYER_LOGIN, PlayerLoginPayload(config.Character));
    REQUIRE(client.WaitFor(SMSG_LOGIN_VERIFY_WORLD, Milliseconds(60000)).has_value());
    client.Pump(Milliseconds(5000));                // the rest of the login burst
}
}

TEST_CASE("headless: connect, authenticate, enter encrypted mode", "[headless]")
{
    REQUIRE_REALM();
    Config const& config = GetConfig();
    Client client(config.Host, config.Port);
    CHECK(client.Authenticate(config.Account) == 0);
    CHECK(client.IsEncrypted());
    RequireClean(client);
}

TEST_CASE("headless: character list", "[headless]")
{
    REQUIRE_REALM();
    Config const& config = GetConfig();
    Client client(config.Host, config.Port);
    REQUIRE(client.Authenticate(config.Account) == 0);

    client.Send(CMSG_ENUM_CHARACTERS);
    CHECK(client.WaitFor(SMSG_ENUM_CHARACTERS_RESULT, Milliseconds(15000)).has_value());
    RequireClean(client);
}

TEST_CASE("headless: login burst is read to the last byte by the client's readers", "[headless]")
{
    REQUIRE_CHARACTER();
    Config const& config = GetConfig();
    Client client(config.Host, config.Port);
    EnterWorld(client);
    RequireClean(client);
}

TEST_CASE("headless: invite a bot to a party", "[headless]")
{
    REQUIRE_BOT();
    Config const& config = GetConfig();
    Client client(config.Host, config.Port);
    EnterWorld(client);
    client.ClearLog();

    client.Send(CMSG_PARTY_INVITE, PartyInvitePayload(config.BotName));
    // the bot accepts on its own; the server then sends the roster to both
    std::optional<ReceivedPacket> update = client.WaitFor(SMSG_PARTY_UPDATE, Milliseconds(20000));
    CHECK(update.has_value());
    client.Pump(Milliseconds(3000));
    RequireClean(client);
}

TEST_CASE("headless: trade with a bot", "[headless]")
{
    REQUIRE_BOT();
    Config const& config = GetConfig();
    Client client(config.Host, config.Port);
    EnterWorld(client);
    client.ClearLog();

    client.Send(CMSG_INITIATE_TRADE, GuidPayload(PlayerGuid(config.BotGuid)));
    CHECK(client.WaitFor(SMSG_TRADE_STATUS, Milliseconds(10000)).has_value());
    client.Pump(Milliseconds(2000));
    RequireClean(client);
}

TEST_CASE("headless: inspect a bot", "[headless]")
{
    REQUIRE_BOT();
    Config const& config = GetConfig();
    Client client(config.Host, config.Port);
    EnterWorld(client);
    client.ClearLog();

    client.Send(CMSG_INSPECT, GuidPayload(PlayerGuid(config.BotGuid)));
    CHECK(client.WaitFor(SMSG_INSPECT_RESULT, Milliseconds(10000)).has_value());
    RequireClean(client);
}

TEST_CASE("headless: whisper a bot", "[headless]")
{
    REQUIRE_BOT();
    Config const& config = GetConfig();
    Client client(config.Host, config.Port);
    EnterWorld(client);
    client.ClearLog();

    client.Send(CMSG_CHAT_MESSAGE_WHISPER, WhisperPayload(config.BotName, "hello"));
    CHECK(client.WaitFor(SMSG_CHAT, Milliseconds(10000)).has_value());      // at least the "to <bot>" echo
    RequireClean(client);
}
