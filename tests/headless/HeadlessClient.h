#ifndef TRINITYCORE_TESTS_HEADLESS_CLIENT_H
#define TRINITYCORE_TESTS_HEADLESS_CLIENT_H

#include "AES.h"
#include "ByteBuffer.h"
#include "Define.h"
#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// A packet-level WoW Classic 1.60.1.70291 client for tests: no game client, no graphics. It speaks the world socket protocol of
// WorldSocket.cpp (hello strings, framed packets, AES-GCM after SMSG_ENTER_ENCRYPTED_MODE, zlib compressed packets) and numbers its
// opcodes like the 70291 client (ClassicOpcodes). Blocking, single threaded: every call has a timeout and throws on a protocol error.
namespace Headless
{
using Milliseconds = std::chrono::milliseconds;

struct Credentials
{
    std::string GameAccount;                // account.username of the seeded game account ("<bnet id>#<index>")
    std::vector<uint8> KeyData;             // account.session_key_bnet of that account (what the Battle.net login stores)
    uint32 RealmId = 1;
    uint32 RegionId = 1;
    uint32 BattlegroupId = 1;
    uint32 Platform = 0;                    // ClientBuild platform / arch / type ids of the ticket
    uint32 Arch = 0;
    uint32 Type = 0;
};

struct ReceivedPacket
{
    uint32 Wire = 0;                        // the number on the wire
    uint32 Core = 0;                        // the core OpcodeServer it maps to (0 if unknown)
    std::vector<uint8> Data;                // payload without the opcode
    std::chrono::steady_clock::time_point At;
};

/// What the reader specs of the real client say about one received packet.
struct SpecIssue
{
    std::string Name;
    uint32 Wire = 0;
    std::string Problem;
};

struct SpecCoverage
{
    uint32 Checked = 0;                     // packets whose reader spec is resolved
    uint32 Unresolved = 0;                  // spec has a field the analysis could not resolve ("?")
    uint32 Undecrypted = 0;                 // reader page not in the dump ("!")
    uint32 NoSpec = 0;                      // not in the main group, or unknown wire number
    std::vector<SpecIssue> Issues;
};

std::string OpcodeName(uint32 coreOpcode);

class Client
{
public:
    Client(std::string host, uint16 port);
    ~Client();
    Client(Client const&) = delete;
    Client& operator=(Client const&) = delete;

    /// TCP connect, hello strings, SMSG_AUTH_CHALLENGE, CMSG_AUTH_SESSION, SMSG_ENTER_ENCRYPTED_MODE + ack, SMSG_AUTH_RESPONSE.
    /// Returns the result code of SMSG_AUTH_RESPONSE (0 = ok); throws on a protocol error or timeout.
    uint32 Authenticate(Credentials const& credentials, Milliseconds timeout = Milliseconds(15000));

    /// Sends a client packet by its core OpcodeClient value; the payload is written in the layout the server reads.
    void Send(uint32 coreOpcode, ByteBuffer const& payload = ByteBuffer());

    /// The next packet from the server (any), or nothing after the timeout.
    std::optional<ReceivedPacket> Next(Milliseconds timeout);

    /// Receives until the given core opcode arrives (everything else is kept in Log()); nothing after the timeout.
    std::optional<ReceivedPacket> WaitFor(uint32 coreOpcode, Milliseconds timeout);

    /// Receives for the given time, collecting into Log().
    void Pump(Milliseconds duration);

    std::vector<ReceivedPacket> const& Log() const { return _log; }
    void ClearLog() { _log.clear(); }

    /// Every packet in Log() against the reader specs of the real client.
    SpecCoverage CheckLog() const;

    bool IsEncrypted() const { return _encrypted; }

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
    std::vector<ReceivedPacket> _log;
    bool _encrypted = false;
};
}

#endif
