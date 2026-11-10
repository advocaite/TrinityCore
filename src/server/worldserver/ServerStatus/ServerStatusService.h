/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRINITYCORE_SERVER_STATUS_SERVICE_H
#define TRINITYCORE_SERVER_STATUS_SERVICE_H

#include "HttpService.h"
#include "HttpSocket.h"
#include <atomic>

/*
 * Small read-only heartbeat for the worldserver: GET /status answers a JSON line with
 * up/stalled state, uptime, online counts and world update lag. Meant for a launcher's
 * realm status dot and for "server is down" alerts. It cannot run commands or change anything.
 *
 * Off unless StatusServer.Token is set. Every request must carry that token.
 * The numbers are written by the world thread once per world tick (relaxed atomics)
 * and only read by the HTTP thread, so a request never touches game objects.
 */

namespace ServerStatus
{
class StatusHttpSocket final : public Trinity::Net::Http::Socket
{
public:
    using Socket::Socket;

    Trinity::Net::Http::RequestHandlerResult RequestHandler(Trinity::Net::Http::RequestContext& context) override;

protected:
    // no cookies or login state: every request is checked on its own
    std::shared_ptr<Trinity::Net::Http::SessionState> ObtainSessionState(Trinity::Net::Http::RequestContext& /*context*/) const override { return nullptr; }
};

class StatusService final : public Trinity::Net::Http::HttpService<StatusHttpSocket>
{
public:
    using RequestHandlerResult = Trinity::Net::Http::RequestHandlerResult;
    using HttpRequestContext = Trinity::Net::Http::RequestContext;

    static StatusService& Instance();

    // reads StatusServer.* from the config; returns false (and logs why) when the service stays off
    bool Start(Trinity::Asio::IoContext& ioContext, uint32 realmId, std::string realmName);
    void Stop();
    bool IsRunning() const { return _running; }

    // world thread, once per world update
    void OnWorldTick(uint32 diff);

private:
    StatusService();

    RequestHandlerResult HandleGetStatus(std::shared_ptr<StatusHttpSocket> session, HttpRequestContext& context) const;
    bool IsAuthorized(HttpRequestContext const& context) const;

    std::string _token;
    uint32 _realmId;
    std::string _realmName;
    uint32 _stallMs;
    bool _running;

    // world thread only
    uint64 _avgDiffX16;         // smoothed diff, fixed point (x16)
    uint32 _peakWindowStartMs;
    uint32 _peakCurrent;
    uint32 _peakPrevious;

    // written by the world thread, read by the HTTP thread
    std::atomic<uint32> _lastTickMs;      // getMSTime() of the last world update
    std::atomic<uint32> _lastDiff;
    std::atomic<uint32> _avgDiff;
    std::atomic<uint32> _peakDiff;        // worst diff of the last 30-60 seconds
    std::atomic<uint32> _uptime;
    std::atomic<uint32> _players;
    std::atomic<uint32> _playersPeak;
    std::atomic<uint32> _sessions;
    std::atomic<uint32> _queued;
    std::atomic<uint32> _shutdownIn;      // seconds left on a pending shutdown, 0 when none
    std::atomic<bool> _ticked;
};
}

#define sServerStatus ServerStatus::StatusService::Instance()

#endif // TRINITYCORE_SERVER_STATUS_SERVICE_H
