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

#include "SpellTestSession.h"
#include "BattlenetAccountMgr.h"
#include "ClientBuildInfo.h"
#include "World.h"

SpellTestSession::SpellTestSession(uint32 accountId, std::string accountName)
    : WorldSession(accountId, std::move(accountName), Battlenet::AccountMgr::GetIdByGameAccount(accountId), std::string(),
        std::shared_ptr<WorldSocket>(), SEC_PLAYER, uint8(sWorld->getIntConfig(CONFIG_EXPANSION)), 0, std::string(), Minutes(0), 0,
        ClientBuild::VariantId{}, LOCALE_enUS, 0, false)
{
}

void SpellTestSession::SendPacket(WorldPacket const* /*packet*/, bool /*forced*/)
{
}
