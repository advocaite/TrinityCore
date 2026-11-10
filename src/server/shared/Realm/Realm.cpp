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

#include "Realm.h"
#include "IpAddress.h"
#include "IpNetwork.h"
#include "StringFormat.h"
#include <algorithm>
#include <cctype>

Realm::Realm() = default;
Realm::Realm(Realm const& other) = default;
Realm::Realm(Realm&& other) noexcept = default;
Realm& Realm::operator=(Realm const& other) = default;
Realm& Realm::operator=(Realm&& other) noexcept = default;
Realm::~Realm() = default;

void Realm::SetName(std::string name)
{
    Name = name;
    NormalizedName = std::move(name);
    std::erase_if(NormalizedName, [](char c) { return std::isspace(static_cast<unsigned char>(c)); });
}

boost::asio::ip::address Realm::GetAddressForClient(boost::asio::ip::address const& clientAddr) const
{
    // A game on the server's own PC (or a friend coming in through the launcher's tunnel, which also arrives on this PC)
    // always reaches the world server at 127.0.0.1: both BindIP settings the repack uses (127.0.0.1 and 0.0.0.0) listen there.
    // Picking a network address instead tied the login to whatever network card was up when the server started: with a VPN
    // on at start, the realm handed out the VPN's address and logins broke once the VPN was turned off.
    if (clientAddr.is_loopback())
        return boost::asio::ip::address_v4::loopback();

    if (auto addressIndex = Trinity::Net::SelectAddressForClient(clientAddr, Addresses))
        return Addresses[*addressIndex];

    if (Addresses.size() > 1 && clientAddr.is_loopback())
        return Addresses[1];

    return Addresses[0];
}

uint32 GetClassicSuperDistrictForContentSet(uint32 contentSetId)
{
    switch (contentSetId)
    {
        case 136: return 1; // PvP
        case 137: return 2; // Normal
        case 138: return 3; // Roleplay
        case 140: return 4; // Hardcore
        default:  return 0;
    }
}

uint32 GetClassicContentSetForSuperDistrict(uint32 superDistrictId)
{
    switch (superDistrictId)
    {
        case 1: return 136; // PvP
        case 2: return 137; // Normal
        case 3: return 138; // Roleplay
        case 4: return 140; // Hardcore
        default: return 0;
    }
}

uint32 Realm::GetConfigId() const
{
    return ConfigIdByType[Type];
}

uint32 const Realm::ConfigIdByType[MAX_CLIENT_REALM_TYPE] =
{
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
};

std::string Battlenet::RealmHandle::GetAddressString() const
{
    return Trinity::StringFormat("{}-{}-{}", Region, Site, Realm);
}

std::string Battlenet::RealmHandle::GetSubRegionAddress() const
{
    // Classic (1.60+) "super realm" clients use the full realm address as sub-region (hard-coded 70-1-70)
    return Trinity::StringFormat("{}-{}-{}", Region, Site, Realm);
}
