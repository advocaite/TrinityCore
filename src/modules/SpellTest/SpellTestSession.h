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

#ifndef TRINITY_SPELL_TEST_SESSION_H
#define TRINITY_SPELL_TEST_SESSION_H

#include "WorldSession.h"

// The session of a test character: no client and no socket. The packets the server sends it are dropped, and the
// module's world script runs its database callbacks (WorldSession::UpdateWithoutClient) every world update.
class SpellTestSession final : public WorldSession
{
public:
    SpellTestSession(uint32 accountId, std::string accountName);

    void SendPacket(WorldPacket const* packet, bool forced = false) override;
};

#endif // TRINITY_SPELL_TEST_SESSION_H
