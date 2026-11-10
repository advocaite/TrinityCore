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

#ifndef TRINITYCORE_IP_CONNECTION_LIMIT_H
#define TRINITYCORE_IP_CONNECTION_LIMIT_H

#include "Define.h"
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Trinity::Net
{
// Open connections per remote address on one listener. A socket holds its Slot for as long as it lives and the count drops
// when the Slot goes. A limit of 0 and the server's own PC (loopback, where a local launcher or bridge connects from) are
// never limited.
class IpConnectionLimit
{
public:
    class Slot
    {
    public:
        Slot(IpConnectionLimit* owner, std::string key) : _owner(owner), _key(std::move(key)) { }
        Slot(Slot const&) = delete;
        Slot& operator=(Slot const&) = delete;
        ~Slot() { if (_owner) _owner->Release(_key); }

    private:
        IpConnectionLimit* _owner;
        std::string _key;
    };

    // nullptr: the address already has `limit` open connections
    std::unique_ptr<Slot> TryAcquire(std::string const& address, bool isLoopback, uint32 limit)
    {
        if (!limit || isLoopback)
            return std::make_unique<Slot>(nullptr, std::string());

        std::scoped_lock lock(_lock);
        uint32& count = _counts[address];
        if (count >= limit)
        {
            if (!count)
                _counts.erase(address);
            return nullptr;
        }

        ++count;
        return std::make_unique<Slot>(this, address);
    }

    uint32 GetCount(std::string const& address) const
    {
        std::scoped_lock lock(_lock);
        auto itr = _counts.find(address);
        return itr != _counts.end() ? itr->second : 0;
    }

private:
    void Release(std::string const& address)
    {
        std::scoped_lock lock(_lock);
        auto itr = _counts.find(address);
        if (itr == _counts.end())
            return;

        if (!--itr->second)
            _counts.erase(itr);
    }

    mutable std::mutex _lock;
    std::unordered_map<std::string, uint32> _counts;
};
}

#endif // TRINITYCORE_IP_CONNECTION_LIMIT_H
