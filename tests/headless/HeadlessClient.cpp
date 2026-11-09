#include "HeadlessClient.h"
#include "ClassicOpcodes.h"
#include "ClassicWireSpecReader.h"
#include "CryptoHash.h"
#include "CryptoRandom.h"
#include "HMAC.h"
#include "Opcodes.h"
#include "ProtobufJSON.h"
#include "RealmList.pb.h"
#include "SessionKeyGenerator.h"
#include <boost/asio.hpp>
#include <cstring>
#include <map>
#include <span>
#include <stdexcept>
#include <zlib.h>

namespace Headless
{
namespace
{
using boost::asio::ip::tcp;
using ClassicOpcodes::Build;

constexpr std::string_view ServerHello = "WORLD OF WARCRAFT CONNECTION - SERVER TO CLIENT - V2\n";
constexpr std::string_view ClientHello = "WORLD OF WARCRAFT CONNECTION - CLIENT TO SERVER - V2\n";

// Protocol constants of WorldSocket.cpp: they must match it byte for byte.
constexpr std::array<uint8, 32> SessionKeySeed = { 0xE8, 0x1E, 0x8B, 0x59, 0x27, 0x62, 0x1E, 0xAA, 0x86, 0x15, 0x18, 0xEA, 0xC0, 0xBF, 0x66, 0x8C,
    0x6D, 0xBF, 0x83, 0x93, 0xBC, 0xAA, 0x80, 0x52, 0x5B, 0x1E, 0xDC, 0x23, 0xA0, 0x12, 0xB7, 0x50 };
constexpr std::array<uint8, 32> EncryptionKeySeed = { 0x71, 0xC9, 0xED, 0x5A, 0xA7, 0x0E, 0x4D, 0xFF, 0x4C, 0x36, 0xA6, 0x5A, 0x3E, 0x46, 0x8A, 0x4A,
    0x5D, 0xA1, 0x48, 0xC8, 0x30, 0x47, 0x4A, 0xDE, 0xF6, 0x0D, 0x6C, 0xBE, 0x6F, 0xE4, 0x55, 0x73 };
constexpr uint32 ClientToServerMagic = 0x544E4C43;      // "CLNT" in the GCM iv of client packets
constexpr uint32 ServerToClientMagic = 0x52565253;      // "SRVR"
constexpr uint32 CompressionAdlerSeed = 0x9827D8F1;
constexpr uint32 ConnectionGroup70291 = 0x46;           // the client's connection group (70245: 0x45)
constexpr uint32 ConnectionGroup70245 = 0x45;

#pragma pack(push, 1)
struct FrameHeader
{
    uint32 Size;
    uint8 Tag[12];
};
struct CompressionInfo
{
    uint32 UncompressedSize;
    uint32 UncompressedAdler;
    uint32 CompressedAdler;
};
#pragma pack(pop)

std::array<uint8, 12> MakeIv(uint64 counter, uint32 magic)
{
    std::array<uint8, 12> iv;
    std::memcpy(iv.data(), &counter, sizeof(counter));
    std::memcpy(iv.data() + 8, &magic, sizeof(magic));
    return iv;
}

struct OpcodeMaps
{
    std::map<uint32, uint32> ServerWireToCore;
    std::map<uint32, uint32> ClientCoreToWire;

    OpcodeMaps()
    {
        opcodeTable.Initialize();

        for (uint32 group = 0x45; group <= 0x6A; ++group)
        {
            for (uint32 index = 0; index < 0x400; ++index)
            {
                OpcodeServer const opcode = OpcodeServer((group << 16) | index);
                if (!opcodeTable.IsValid(opcode) || !opcodeTable[opcode] || ClassicOpcodes::IsServerOpcodeBlocked(opcode))
                    continue;
                ServerWireToCore.try_emplace(ClassicOpcodes::TranslateServerOpcode(Build::Build70291, opcode), uint32(opcode));
            }
        }

        // Client opcodes: the inverse of TranslateClientOpcode70245 (a table plus group - 1), then the 70291 move of the connection group.
        // Where several wire numbers translate to one core opcode the one on the arithmetic rule (core group + 1) wins.
        std::map<uint32, uint32> best;
        for (uint32 group = 0x25; group <= 0x47; ++group)
        {
            for (uint32 index = 0; index < 0x400; ++index)
            {
                uint32 const wire = (group << 16) | index;
                uint32 const core = ClassicOpcodes::TranslateClientOpcode70245(wire);
                OpcodeClient const opcode = OpcodeClient(core);
                if (!opcodeTable.IsValid(opcode) || !opcodeTable[opcode])
                    continue;
                auto itr = best.find(core);
                if (itr == best.end() || ((core >> 16) + 1 == group && (itr->second >> 16) != group))
                    best[core] = wire;
            }
        }
        for (auto const& [core, wire] : best)
        {
            uint32 shifted = wire;
            if ((wire >> 16) == ConnectionGroup70245)
                shifted = (ConnectionGroup70291 << 16) | (wire & 0xFFFF);
            ClientCoreToWire[core] = shifted;
        }
    }
};

OpcodeMaps const& Maps()
{
    static OpcodeMaps const maps;
    return maps;
}
}

std::string OpcodeName(uint32 coreOpcode)
{
    opcodeTable.Initialize();
    if (OpcodeServer const server = OpcodeServer(coreOpcode); opcodeTable.IsValid(server) && opcodeTable[server])
        return opcodeTable[server]->Name;
    if (OpcodeClient const client = OpcodeClient(coreOpcode); opcodeTable.IsValid(client) && opcodeTable[client])
        return opcodeTable[client]->Name;
    return "?";
}

struct Client::Impl
{
    Impl(std::string host, uint16 port) : Socket(Context), Host(std::move(host)), Port(port) { }

    ~Impl()
    {
        if (InflateReady)
            inflateEnd(&Inflate);
    }

    boost::asio::io_context Context;
    tcp::socket Socket;
    std::string Host;
    uint16 Port;

    std::vector<uint8> In;                                  // received, not yet consumed
    bool Encrypted = false;
    uint64 SendCounter = 0;                                 // every frame sent, encrypted or not (the server counts like that)
    uint64 ReceiveCounter = 0;
    std::unique_ptr<Trinity::Crypto::AES> Encrypt;          // client -> server
    std::unique_ptr<Trinity::Crypto::AES> Decrypt;          // server -> client
    z_stream Inflate = { };
    bool InflateReady = false;

    // Reads at least `count` bytes into In; false on timeout, throws if the server closed the connection.
    bool Fill(size_t count, std::chrono::steady_clock::time_point deadline)
    {
        while (In.size() < count)
        {
            auto const now = std::chrono::steady_clock::now();
            if (now >= deadline)
                return false;

            std::array<uint8, 16384> chunk;
            boost::system::error_code error;
            size_t received = 0;
            bool done = false;
            Socket.async_read_some(boost::asio::buffer(chunk), [&](boost::system::error_code ec, size_t size)
            {
                error = ec;
                received = size;
                done = true;
            });
            Context.restart();
            Context.run_for(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now));
            if (!done)
            {
                // timed out: cancel the read and let its handler run (it completes with operation_aborted and maybe some bytes)
                Socket.cancel();
                Context.restart();
                Context.run();
            }
            else if (error)
                throw std::runtime_error("connection closed by the server: " + error.message());

            if (received == 0)
                return false;

            In.insert(In.end(), chunk.begin(), chunk.begin() + received);
        }
        return true;
    }

    void Consume(size_t count) { In.erase(In.begin(), In.begin() + count); }

    void WriteAll(uint8 const* data, size_t size)
    {
        boost::system::error_code error;
        boost::asio::write(Socket, boost::asio::buffer(data, size), error);
        if (error)
            throw std::runtime_error("send failed: " + error.message());
    }

    void SendFrame(uint32 wire, uint8 const* payload, size_t size)
    {
        std::vector<uint8> frame(sizeof(FrameHeader) + 4 + size);
        FrameHeader header = { };
        header.Size = uint32(4 + size);
        std::memcpy(frame.data() + sizeof(FrameHeader), &wire, 4);
        if (size)
            std::memcpy(frame.data() + sizeof(FrameHeader) + 4, payload, size);

        if (Encrypted)
        {
            auto const iv = MakeIv(SendCounter, ClientToServerMagic);
            if (!Encrypt->Process(iv, frame.data() + sizeof(FrameHeader), 4 + size, header.Tag))
                throw std::runtime_error("encrypting a packet failed");
        }
        ++SendCounter;

        std::memcpy(frame.data(), &header, sizeof(header));
        WriteAll(frame.data(), frame.size());
    }

    // One frame from the server: wire opcode + payload (decrypted, decompressed). Nothing after the timeout.
    std::optional<ReceivedPacket> ReadFrame(Milliseconds timeout)
    {
        auto const deadline = std::chrono::steady_clock::now() + timeout;
        if (!Fill(sizeof(FrameHeader), deadline))
            return std::nullopt;

        FrameHeader header;
        std::memcpy(&header, In.data(), sizeof(header));
        if (header.Size < 4 || header.Size > 0x400000)
            throw std::runtime_error("bad frame size " + std::to_string(header.Size));
        if (!Fill(sizeof(FrameHeader) + header.Size, deadline))
            return std::nullopt;

        std::vector<uint8> body(In.begin() + sizeof(FrameHeader), In.begin() + sizeof(FrameHeader) + header.Size);
        Consume(sizeof(FrameHeader) + header.Size);

        if (Encrypted)
        {
            auto const iv = MakeIv(ReceiveCounter, ServerToClientMagic);
            if (!Decrypt->Process(iv, body.data(), body.size(), header.Tag))
                throw std::runtime_error("server packet failed its integrity check (counter " + std::to_string(ReceiveCounter) + ")");
        }
        ++ReceiveCounter;

        ReceivedPacket packet;
        packet.At = std::chrono::steady_clock::now();
        std::memcpy(&packet.Wire, body.data(), 4);
        packet.Data.assign(body.begin() + 4, body.end());

        static uint32 const compressedWire = ClassicOpcodes::TranslateServerOpcode(Build::Build70291, SMSG_COMPRESSED_PACKET);
        if (packet.Wire == compressedWire)
            Decompress(packet);

        auto const& map = Maps().ServerWireToCore;
        if (auto itr = map.find(packet.Wire); itr != map.end())
            packet.Core = itr->second;
        return packet;
    }

    void Decompress(ReceivedPacket& packet)
    {
        if (packet.Data.size() < sizeof(CompressionInfo))
            throw std::runtime_error("compressed packet without its header");

        CompressionInfo info;
        std::memcpy(&info, packet.Data.data(), sizeof(info));
        if (info.UncompressedSize < 4 || info.UncompressedSize > 0x1000000)
            throw std::runtime_error("bad uncompressed size " + std::to_string(info.UncompressedSize));

        uint8 const* compressed = packet.Data.data() + sizeof(info);
        size_t const compressedSize = packet.Data.size() - sizeof(info);
        if (adler32(CompressionAdlerSeed, compressed, uInt(compressedSize)) != info.CompressedAdler)
            throw std::runtime_error("compressed data checksum mismatch");

        if (!InflateReady)
        {
            if (inflateInit2(&Inflate, -15) != Z_OK)
                throw std::runtime_error("inflateInit2 failed");
            InflateReady = true;
        }

        std::vector<uint8> out(info.UncompressedSize);
        Inflate.next_in = const_cast<Bytef*>(compressed);
        Inflate.avail_in = uInt(compressedSize);
        Inflate.next_out = out.data();
        Inflate.avail_out = uInt(out.size());
        int const result = inflate(&Inflate, Z_SYNC_FLUSH);
        if ((result != Z_OK && result != Z_BUF_ERROR) || Inflate.avail_out != 0)
            throw std::runtime_error("inflate failed, code " + std::to_string(result));
        if (adler32(CompressionAdlerSeed, out.data(), uInt(out.size())) != info.UncompressedAdler)
            throw std::runtime_error("uncompressed data checksum mismatch");

        std::memcpy(&packet.Wire, out.data(), 4);
        packet.Data.assign(out.begin() + 4, out.end());
    }
};

Client::Client(std::string host, uint16 port) : _impl(std::make_unique<Impl>(std::move(host), port))
{
}

Client::~Client() = default;

uint32 Client::Authenticate(Credentials const& credentials, Milliseconds timeout)
{
    Impl& impl = *_impl;
    auto const deadline = std::chrono::steady_clock::now() + timeout;
    auto remaining = [&]
    {
        return std::chrono::duration_cast<Milliseconds>(deadline - std::chrono::steady_clock::now());
    };

    tcp::resolver resolver(impl.Context);
    boost::system::error_code error;
    auto endpoints = resolver.resolve(impl.Host, std::to_string(impl.Port), error);
    if (error)
        throw std::runtime_error("cannot resolve " + impl.Host + ": " + error.message());
    boost::asio::connect(impl.Socket, endpoints, error);
    if (error)
        throw std::runtime_error("cannot connect to " + impl.Host + ":" + std::to_string(impl.Port) + ": " + error.message());
    impl.Socket.set_option(tcp::no_delay(true), error);

    // hello strings
    if (!impl.Fill(ServerHello.size(), deadline))
        throw std::runtime_error("no server hello");
    if (std::string_view(reinterpret_cast<char const*>(impl.In.data()), ServerHello.size()) != ServerHello)
        throw std::runtime_error("unexpected server hello");
    impl.Consume(ServerHello.size());
    impl.WriteAll(reinterpret_cast<uint8 const*>(ClientHello.data()), ClientHello.size());

    // SMSG_AUTH_CHALLENGE: 32 byte dos challenge, 32 byte challenge, 1 byte zero bits
    std::optional<ReceivedPacket> challengePacket = impl.ReadFrame(remaining());
    if (!challengePacket)
        throw std::runtime_error("no SMSG_AUTH_CHALLENGE");
    if (challengePacket->Core != SMSG_AUTH_CHALLENGE || challengePacket->Data.size() < 65)
        throw std::runtime_error("expected SMSG_AUTH_CHALLENGE, got wire 0x" + std::to_string(challengePacket->Wire));
    std::array<uint8, 32> serverChallenge;
    std::memcpy(serverChallenge.data(), challengePacket->Data.data() + 32, 32);

    // CMSG_AUTH_SESSION
    std::array<uint8, 32> localChallenge = Trinity::Crypto::GetRandomBytes<32>();

    JSON::RealmList::RealmJoinTicket joinTicket;
    joinTicket.set_gameaccount(credentials.GameAccount);
    joinTicket.set_platform(credentials.Platform);
    joinTicket.set_clientarch(credentials.Arch);
    joinTicket.set_type(credentials.Type);
    std::string const ticket = JSON::Serialize(joinTicket);

    ByteBuffer auth;
    auth << uint64(0);                              // DosResponse
    auth << uint32(credentials.RegionId);
    auth << uint32(credentials.BattlegroupId);
    auth << uint32(credentials.RealmId);
    auth.append(localChallenge.data(), localChallenge.size());
    std::array<uint8, 24> const digest = { };       // the test realm runs with Network.SkipBuildAuthKeyCheck = 1
    auth.append(digest.data(), digest.size());
    auth.WriteBit(false);                           // UseIPv6
    auth.FlushBits();
    auth << uint32(ticket.size());
    auth.append(reinterpret_cast<uint8 const*>(ticket.data()), ticket.size());
    Send(CMSG_AUTH_SESSION, auth);

    // keys, exactly as WorldSocket::HandleAuthSessionCallback derives them
    Trinity::Crypto::SHA512 keyDataHash;
    keyDataHash.UpdateData(credentials.KeyData.data(), credentials.KeyData.size());
    keyDataHash.Finalize();

    Trinity::Crypto::HMAC_SHA512 sessionKeyHmac(keyDataHash.GetDigest());
    sessionKeyHmac.UpdateData(serverChallenge);
    sessionKeyHmac.UpdateData(localChallenge);
    sessionKeyHmac.UpdateData(SessionKeySeed);
    sessionKeyHmac.Finalize();

    std::array<uint8, 40> sessionKey;
    SessionKeyGenerator<Trinity::Crypto::SHA512> sessionKeyGenerator(sessionKeyHmac.GetDigest());
    sessionKeyGenerator.Generate(sessionKey.data(), uint32(sessionKey.size()));

    Trinity::Crypto::HMAC_SHA512 encryptKeyHmac(sessionKey);
    encryptKeyHmac.UpdateData(localChallenge);
    encryptKeyHmac.UpdateData(serverChallenge);
    encryptKeyHmac.UpdateData(EncryptionKeySeed);
    encryptKeyHmac.Finalize();
    std::array<uint8, 32> encryptKey;
    std::memcpy(encryptKey.data(), encryptKeyHmac.GetDigest().data(), 32);

    // SMSG_ENTER_ENCRYPTED_MODE (int32 region group, 64 byte Ed25519 signature, enabled bit; the signature is not verified here: this
    // client only has to agree on the key), answered by CMSG_ENTER_ENCRYPTED_MODE_ACK; both directions are encrypted from then on.
    std::optional<ReceivedPacket> encryptedMode;
    while ((encryptedMode = impl.ReadFrame(remaining())))
    {
        if (encryptedMode->Core == SMSG_ENTER_ENCRYPTED_MODE)
            break;
        if (encryptedMode->Core == SMSG_AUTH_RESPONSE && encryptedMode->Data.size() >= 4)
        {
            uint32 result;
            std::memcpy(&result, encryptedMode->Data.data(), 4);
            return result ? result : 0xFFFFFFFF;       // a rejection comes before the encrypted mode
        }
        _log.push_back(std::move(*encryptedMode));
    }
    if (!encryptedMode)
        throw std::runtime_error("no SMSG_ENTER_ENCRYPTED_MODE (wrong account, session key or a closed realm?)");

    Send(CMSG_ENTER_ENCRYPTED_MODE_ACK);
    impl.Encrypt = std::make_unique<Trinity::Crypto::AES>(true, 256);
    impl.Decrypt = std::make_unique<Trinity::Crypto::AES>(false, 256);
    impl.Encrypt->Init(std::span<uint8 const>(encryptKey));
    impl.Decrypt->Init(std::span<uint8 const>(encryptKey));
    impl.Encrypted = true;
    _encrypted = true;

    std::optional<ReceivedPacket> response = WaitFor(SMSG_AUTH_RESPONSE, remaining());
    if (!response || response->Data.size() < 4)
        throw std::runtime_error("no SMSG_AUTH_RESPONSE");
    uint32 result;
    std::memcpy(&result, response->Data.data(), 4);
    return result;
}

void Client::Send(uint32 coreOpcode, ByteBuffer const& payload)
{
    auto const& map = Maps().ClientCoreToWire;
    auto itr = map.find(coreOpcode);
    if (itr == map.end())
        throw std::runtime_error("no 70291 wire number for " + OpcodeName(coreOpcode));

    _impl->SendFrame(itr->second, payload.data(), payload.size());
}

std::optional<ReceivedPacket> Client::Next(Milliseconds timeout)
{
    std::optional<ReceivedPacket> packet = _impl->ReadFrame(timeout);
    if (packet)
        _log.push_back(*packet);
    return packet;
}

std::optional<ReceivedPacket> Client::WaitFor(uint32 coreOpcode, Milliseconds timeout)
{
    auto const deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        std::optional<ReceivedPacket> packet = Next(std::chrono::duration_cast<Milliseconds>(deadline - std::chrono::steady_clock::now()));
        if (packet && packet->Core == coreOpcode)
            return packet;
    }
    return std::nullopt;
}

void Client::Pump(Milliseconds duration)
{
    auto const deadline = std::chrono::steady_clock::now() + duration;
    while (std::chrono::steady_clock::now() < deadline)
        Next(std::chrono::duration_cast<Milliseconds>(deadline - std::chrono::steady_clock::now()));
}

SpecCoverage Client::CheckLog() const
{
    SpecCoverage coverage;
    for (ReceivedPacket const& packet : _log)
    {
        ClassicWireSpec::SpecEntry const* entry = ClassicWireSpec::FindSpec(packet.Wire);
        if (!entry)
        {
            ++coverage.NoSpec;
            continue;
        }
        std::string_view const spec = entry->Spec;
        if (spec == "?")
        {
            ++coverage.Unresolved;
            continue;
        }
        if (spec == "!")
        {
            ++coverage.Undecrypted;
            continue;
        }

        ++coverage.Checked;
        std::string problem = ClassicWireSpec::ReadSpec(spec, packet.Data.data(), packet.Data.size());
        if (!problem.empty())
            coverage.Issues.push_back({ packet.Core ? OpcodeName(packet.Core) : std::string("?"), packet.Wire, std::move(problem) });
    }
    return coverage;
}
}
