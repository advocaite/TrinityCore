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

#include "ServerStatusService.h"
#include "Configuration/Config.h"
#include "GameTime.h"
#include "GitRevision.h"
#include "Log.h"
#include "StringFormat.h"
#include "Timer.h"
#include "World.h"
#include <algorithm>
#include <ctime>

namespace ServerStatus
{
namespace
{
constexpr uint32 PEAK_WINDOW_MS = 30 * IN_MILLISECONDS;
constexpr std::size_t MIN_TOKEN_LENGTH = 16;

// same time whatever the input, so the token can't be guessed one character at a time
bool TokenEquals(std::string_view a, std::string_view b)
{
    std::size_t const len = std::max(a.size(), b.size());
    uint8 diff = a.size() == b.size() ? 0 : 1;
    for (std::size_t i = 0; i < len; ++i)
    {
        uint8 const ca = i < a.size() ? uint8(a[i]) : 0;
        uint8 const cb = i < b.size() ? uint8(b[i]) : 0;
        diff |= ca ^ cb;
    }
    return diff == 0;
}

std::string JsonEscape(std::string_view in)
{
    std::string out;
    out.reserve(in.size() + 2);
    for (char c : in)
    {
        switch (c)
        {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (uint8(c) < 0x20)
                    out += Trinity::StringFormat("\\u{:04x}", uint8(c));
                else
                    out += c;
                break;
        }
    }
    return out;
}

// value of "name=" in the query string, empty when missing
std::string_view QueryValue(std::string_view target, std::string_view name)
{
    std::size_t const q = target.find('?');
    if (q == std::string_view::npos)
        return {};

    std::string_view query = target.substr(q + 1);
    while (!query.empty())
    {
        std::size_t const amp = query.find('&');
        std::string_view pair = query.substr(0, amp);
        if (pair.size() > name.size() && pair.starts_with(name) && pair[name.size()] == '=')
            return pair.substr(name.size() + 1);
        if (amp == std::string_view::npos)
            break;
        query.remove_prefix(amp + 1);
    }
    return {};
}
}

Trinity::Net::Http::RequestHandlerResult StatusHttpSocket::RequestHandler(Trinity::Net::Http::RequestContext& context)
{
    return sServerStatus.HandleRequest(std::static_pointer_cast<StatusHttpSocket>(this->shared_from_this()), context);
}

StatusService::StatusService() : HttpService("status"), _realmId(0), _stallMs(0), _running(false),
    _avgDiffX16(0), _peakWindowStartMs(0), _peakCurrent(0), _peakPrevious(0),
    _lastTickMs(0), _lastDiff(0), _avgDiff(0), _peakDiff(0), _uptime(0), _players(0), _playersPeak(0),
    _sessions(0), _queued(0), _shutdownIn(0), _ticked(false)
{
}

StatusService& StatusService::Instance()
{
    static StatusService instance;
    return instance;
}

bool StatusService::Start(Trinity::Asio::IoContext& ioContext, uint32 realmId, std::string realmName)
{
    _token = sConfigMgr->GetStringDefault("StatusServer.Token", "");
    if (_token.empty())
        return false;

    if (_token.size() < MIN_TOKEN_LENGTH)
    {
        TC_LOG_ERROR("server.worldserver", "StatusServer.Token is shorter than {} characters, status server not started", MIN_TOKEN_LENGTH);
        _token.clear();
        return false;
    }

    std::string bindIp = sConfigMgr->GetStringDefault("StatusServer.BindIP", "127.0.0.1");
    int32 port = sConfigMgr->GetIntDefault("StatusServer.Port", 8087);
    if (port <= 0 || port > 0xFFFF)
    {
        TC_LOG_ERROR("server.worldserver", "StatusServer.Port {} is not a valid port, status server not started", port);
        return false;
    }

    _stallMs = uint32(std::max(1, sConfigMgr->GetIntDefault("StatusServer.StallSeconds", 10))) * IN_MILLISECONDS;
    _realmId = realmId;
    _realmName = std::move(realmName);

    if (!HttpService::StartNetwork(ioContext, bindIp, uint16(port), 1))
    {
        TC_LOG_ERROR("server.worldserver", "Status server could not listen on {}:{}", bindIp, port);
        return false;
    }

    // the token can arrive in the query string, so never write request lines to the trace log
    RegisterHandler(boost::beast::http::verb::get, "/status", [this](std::shared_ptr<StatusHttpSocket> session, HttpRequestContext& context)
    {
        return HandleGetStatus(std::move(session), context);
    }, Trinity::Net::Http::RequestHandlerFlag::DoNotLogRequestContent);

    _running = true;
    TC_LOG_INFO("server.worldserver", "Status server listening on {}:{} (GET /status)", bindIp, port);
    return true;
}

void StatusService::Stop()
{
    if (!_running)
        return;

    _running = false;
    HttpService::StopNetwork();
}

void StatusService::OnWorldTick(uint32 diff)
{
    if (!_running)
        return;

    uint32 const now = getMSTime();

    // smoothed lag: each tick moves the average 1/16 of the way to the new diff
    if (!_ticked.load(std::memory_order_relaxed))
        _avgDiffX16 = uint64(diff) << 4;
    else
        _avgDiffX16 = _avgDiffX16 - (_avgDiffX16 >> 4) + diff;

    // worst diff over the last 30-60 seconds: two 30 second buckets
    if (getMSTimeDiff(_peakWindowStartMs, now) >= PEAK_WINDOW_MS)
    {
        _peakPrevious = _peakCurrent;
        _peakCurrent = 0;
        _peakWindowStartMs = now;
    }
    _peakCurrent = std::max(_peakCurrent, diff);

    _lastDiff.store(diff, std::memory_order_relaxed);
    _avgDiff.store(uint32(_avgDiffX16 >> 4), std::memory_order_relaxed);
    _peakDiff.store(std::max(_peakCurrent, _peakPrevious), std::memory_order_relaxed);
    _uptime.store(GameTime::GetUptime(), std::memory_order_relaxed);
    _players.store(sWorld->GetPlayerCount(), std::memory_order_relaxed);
    _playersPeak.store(sWorld->GetMaxPlayerCount(), std::memory_order_relaxed);
    _sessions.store(sWorld->GetActiveSessionCount(), std::memory_order_relaxed);
    _queued.store(sWorld->GetQueuedSessionCount(), std::memory_order_relaxed);
    _shutdownIn.store(sWorld->IsShuttingDown() ? sWorld->GetShutDownTimeLeft() : 0, std::memory_order_relaxed);
    _lastTickMs.store(now, std::memory_order_relaxed);
    _ticked.store(true, std::memory_order_release);
}

bool StatusService::IsAuthorized(HttpRequestContext const& context) const
{
    auto const& request = context.request;

    auto auth = request.find(boost::beast::http::field::authorization);
    if (auth != request.end())
    {
        std::string_view value = Trinity::Net::Http::ToStdStringView(auth->value());
        constexpr std::string_view bearer = "Bearer ";
        if (value.starts_with(bearer))
            return TokenEquals(value.substr(bearer.size()), _token);
    }

    auto header = request.find("X-Status-Token");
    if (header != request.end())
        return TokenEquals(Trinity::Net::Http::ToStdStringView(header->value()), _token);

    std::string_view queryToken = QueryValue(Trinity::Net::Http::ToStdStringView(request.target()), "token");
    if (!queryToken.empty())
        return TokenEquals(queryToken, _token);

    return false;
}

StatusService::RequestHandlerResult StatusService::HandleGetStatus(std::shared_ptr<StatusHttpSocket> session, HttpRequestContext& context) const
{
    context.response.set(boost::beast::http::field::cache_control, "no-store");

    if (!IsAuthorized(context))
    {
        TC_LOG_DEBUG("server.worldserver", "{} status request without a valid token", session->GetClientInfo());
        return HandleUnauthorized(std::move(session), context);
    }

    bool const ticked = _ticked.load(std::memory_order_acquire);
    uint32 const tickAge = ticked ? getMSTimeDiff(_lastTickMs.load(std::memory_order_relaxed), getMSTime()) : 0;
    uint32 const shutdownIn = _shutdownIn.load(std::memory_order_relaxed);

    // "stalled" = the process answers but the world loop has stopped moving (hang, long save, debugger)
    std::string_view state = "up";
    if (!ticked)
        state = "starting";
    else if (World::IsStopped())
        state = "stopping";
    else if (tickAge >= _stallMs)
        state = "stalled";
    else if (shutdownIn)
        state = "shutdown_pending";

    // monitors that only look at the HTTP code get 200 while players can play, 503 otherwise
    bool const healthy = state == "up" || state == "shutdown_pending";
    context.response.result(healthy ? boost::beast::http::status::ok : boost::beast::http::status::service_unavailable);
    context.response.set(boost::beast::http::field::content_type, "application/json");

    context.response.body() = Trinity::StringFormat(
        "{{\"status\":\"{}\",\"realm_id\":{},\"realm\":\"{}\",\"uptime_s\":{},"
        "\"players\":{},\"players_peak\":{},\"sessions\":{},\"queued\":{},"
        "\"lag_ms\":{{\"last\":{},\"avg\":{},\"peak\":{}}},\"last_tick_ms_ago\":{},"
        "\"shutdown_in_s\":{},\"version\":\"{}\",\"time\":{}}}",
        state, _realmId, JsonEscape(_realmName), _uptime.load(std::memory_order_relaxed),
        _players.load(std::memory_order_relaxed), _playersPeak.load(std::memory_order_relaxed),
        _sessions.load(std::memory_order_relaxed), _queued.load(std::memory_order_relaxed),
        _lastDiff.load(std::memory_order_relaxed), _avgDiff.load(std::memory_order_relaxed), _peakDiff.load(std::memory_order_relaxed),
        tickAge, shutdownIn, JsonEscape(GitRevision::GetHash()), uint64(std::time(nullptr)));

    return RequestHandlerResult::Handled;
}
}
