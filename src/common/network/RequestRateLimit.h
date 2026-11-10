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
#ifndef TRINITYCORE_REQUEST_RATE_LIMIT_H
#define TRINITYCORE_REQUEST_RATE_LIMIT_H

#include "Define.h"
#include <algorithm>

namespace Trinity::Net
{
// Requests one connection may send: `perSecond` on average, with room for a burst of BurstSeconds worth (a login sends a few dozen
// requests at once, then a trickle). One per connection, not per address, so players behind one address (a tunnel, a household)
// never share it. A limit of 0 is no limit. Times are milliseconds from a steady clock.
class RequestRateLimit
{
public:
    static constexpr uint32 BurstSeconds = 10;

    explicit RequestRateLimit(uint32 perSecond = 0, int64 nowMs = 0)
        : _perSecond(perSecond), _capacity(double(perSecond) * BurstSeconds), _tokens(_capacity), _lastMs(nowMs) { }

    // false: the connection is over its limit and this request is refused
    bool Allow(int64 nowMs)
    {
        if (!_perSecond)
            return true;

        if (nowMs > _lastMs)
        {
            _tokens = std::min(_capacity, _tokens + double(nowMs - _lastMs) * _perSecond / 1000.0);
            _lastMs = nowMs;
        }

        if (_tokens < 1.0)
            return false;

        _tokens -= 1.0;
        return true;
    }

private:
    uint32 _perSecond;
    double _capacity;
    double _tokens;
    int64 _lastMs;
};
}

#endif // TRINITYCORE_REQUEST_RATE_LIMIT_H
