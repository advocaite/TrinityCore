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

#include "WorldSession.h"
#include "AchievementPackets.h"
#include "Common.h"
#include "StringFormat.h"
#include "DatabaseEnv.h"
#include "CharacterCache.h"
#include "Config.h"
#include "GameTime.h"
#include "GossipDef.h"
#include "Group.h"
#include "GroupFinderListings.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "GuildPackets.h"
#include "LFG.h"
#include "LFGPacketsCommon.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "World.h"

void WorldSession::HandleGuildQueryOpcode(WorldPackets::Guild::QueryGuildInfo& query)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_QUERY [{}]: Guild: {} Target: {}",
        GetPlayerInfo(), query.GuildGuid.ToString(), query.PlayerGuid.ToString());

    if (Guild* guild = sGuildMgr->GetGuildByGuid(query.GuildGuid))
    {
        guild->HandleQuery(this);
        return;
    }

    WorldPackets::Guild::QueryGuildInfoResponse response;
    response.GuildGuid = query.GuildGuid;
    SendPacket(response.Write());

    TC_LOG_DEBUG("guild", "SMSG_GUILD_QUERY_RESPONSE [{}]", GetPlayerInfo());
}

void WorldSession::HandleGuildInviteByName(WorldPackets::Guild::GuildInviteByName& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_INVITE [{}]: Invited: {}", GetPlayerInfo(), packet.Name);
    if (normalizePlayerName(packet.Name))
        if (Guild* guild = GetPlayer()->GetGuild())
            guild->HandleInviteMember(this, packet.Name);
}

void WorldSession::HandleGuildOfficerRemoveMember(WorldPackets::Guild::GuildOfficerRemoveMember& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_REMOVE [{}]: Target: {}", GetPlayerInfo(), packet.Removee.ToString());

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleRemoveMember(this, packet.Removee);
}

void WorldSession::HandleGuildAcceptInvite(WorldPackets::Guild::AcceptGuildInvite& /*invite*/)
{
    if (!GetPlayer()->GetGuildId())
        if (Guild* guild = sGuildMgr->GetGuildById(GetPlayer()->GetGuildIdInvited()))
            guild->HandleAcceptMember(this);
}

void WorldSession::HandleGuildDeclineInvitation(WorldPackets::Guild::GuildDeclineInvitation& /*decline*/)
{
    if (GetPlayer()->GetGuildId())
        return;

    GetPlayer()->SetGuildIdInvited(UI64LIT(0));
}

void WorldSession::HandleGuildGetRoster(WorldPackets::Guild::GuildGetRoster& /*packet*/)
{
    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleRoster(this);
    else
        Guild::SendCommandResult(this, GUILD_COMMAND_GET_ROSTER, ERR_GUILD_PLAYER_NOT_IN_GUILD);
}

void WorldSession::HandleGuildPromoteMember(WorldPackets::Guild::GuildPromoteMember& promote)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_PROMOTE [{}]: Target: {}", GetPlayerInfo(), promote.Promotee.ToString());

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleUpdateMemberRank(this, promote.Promotee, false);
}

void WorldSession::HandleGuildDemoteMember(WorldPackets::Guild::GuildDemoteMember& demote)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_DEMOTE [{}]: Target: {}", GetPlayerInfo(), demote.Demotee.ToString());

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleUpdateMemberRank(this, demote.Demotee, true);
}

void WorldSession::HandleGuildAssignRank(WorldPackets::Guild::GuildAssignMemberRank& packet)
{
    ObjectGuid setterGuid = GetPlayer()->GetGUID();

    TC_LOG_DEBUG("guild", "CMSG_GUILD_ASSIGN_MEMBER_RANK [{}]: Target: {} Rank: {}, Issuer: {}",
        GetPlayerInfo(), packet.Member.ToString(), packet.RankOrder, setterGuid.ToString());

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleSetMemberRank(this, packet.Member, setterGuid, GuildRankOrder(packet.RankOrder));
}

void WorldSession::HandleGuildLeave(WorldPackets::Guild::GuildLeave& /*leave*/)
{
    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleLeaveMember(this);
}

void WorldSession::HandleGuildDelete(WorldPackets::Guild::GuildDelete& /*packet*/)
{
    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleDelete(this);
}

void WorldSession::HandleGuildUpdateMotdText(WorldPackets::Guild::GuildUpdateMotdText& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_UPDATE_MOTD_TEXT [{}]: MOTD: {}", GetPlayerInfo(), std::string_view(packet.MotdText));

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleSetMOTD(this, packet.MotdText);
}

void WorldSession::HandleGuildSetMemberNote(WorldPackets::Guild::GuildSetMemberNote& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_SET_NOTE [{}]: Target: {}, Note: {}, Public: {}",
        GetPlayerInfo(), packet.NoteeGUID.ToString(), std::string_view(packet.Note), packet.IsPublic);

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleSetMemberNote(this, packet.Note, packet.NoteeGUID, packet.IsPublic);
}

void WorldSession::HandleGuildGetRanks(WorldPackets::Guild::GuildGetRanks& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_GET_RANKS [{}]: Guild: {}",
        GetPlayerInfo(), packet.GuildGUID.ToString());

    if (Guild* guild = sGuildMgr->GetGuildByGuid(packet.GuildGUID))
        if (guild->IsMember(_player->GetGUID()))
            guild->SendGuildRankInfo(this);
}

void WorldSession::HandleGuildAddRank(WorldPackets::Guild::GuildAddRank& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_ADD_RANK [{}]: Rank: {}", GetPlayerInfo(), std::string_view(packet.Name));

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleAddNewRank(this, packet.Name);
}

void WorldSession::HandleGuildDeleteRank(WorldPackets::Guild::GuildDeleteRank& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_DELETE_RANK [{}]: Rank: {}", GetPlayerInfo(), packet.RankOrder);

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleRemoveRank(this, GuildRankOrder(packet.RankOrder));
}

void WorldSession::HandleGuildShiftRank(WorldPackets::Guild::GuildShiftRank& shiftRank)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_SHIFT_RANK [{}]: RankOrder: {}, ShiftUp: {}", GetPlayerInfo(), shiftRank.RankOrder, shiftRank.ShiftUp ? "true" : "false");

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleShiftRank(this, GuildRankOrder(shiftRank.RankOrder), shiftRank.ShiftUp);
}

void WorldSession::HandleGuildUpdateInfoText(WorldPackets::Guild::GuildUpdateInfoText& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_UPDATE_INFO_TEXT [{}]: {}", GetPlayerInfo(), std::string_view(packet.InfoText));

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleSetInfo(this, packet.InfoText);
}

void WorldSession::HandleSaveGuildEmblem(WorldPackets::Guild::SaveGuildEmblem& packet)
{
    EmblemInfo emblemInfo;
    emblemInfo.ReadPacket(packet);

    TC_LOG_DEBUG("guild", "CMSG_SAVE_GUILD_EMBLEM [{}]: Guid: [{}] Style: {}, Color: {}, BorderStyle: {}, BorderColor: {}, BackgroundColor: {}"
        , GetPlayerInfo(), packet.Vendor.ToString(), emblemInfo.GetStyle()
        , emblemInfo.GetColor(), emblemInfo.GetBorderStyle()
        , emblemInfo.GetBorderColor(), emblemInfo.GetBackgroundColor());

    if (GetPlayer()->GetNPCIfCanInteractWith(packet.Vendor, UNIT_NPC_FLAG_TABARDDESIGNER, UNIT_NPC_FLAG_2_NONE))
    {
        // Remove fake death
        if (GetPlayer()->HasUnitState(UNIT_STATE_DIED))
            GetPlayer()->RemoveAurasByType(SPELL_AURA_FEIGN_DEATH);

        if (!emblemInfo.ValidateEmblemColors())
        {
            Guild::SendSaveEmblemResult(this, ERR_GUILDEMBLEM_INVALID_TABARD_COLORS);
            return;
        }

        if (Guild* guild = GetPlayer()->GetGuild())
            guild->HandleSetEmblem(this, emblemInfo);
        else
            Guild::SendSaveEmblemResult(this, ERR_GUILDEMBLEM_NOGUILD); // "You are not part of a guild!";
    }
    else
        Guild::SendSaveEmblemResult(this, ERR_GUILDEMBLEM_INVALIDVENDOR); // "That's not an emblem vendor!"
}

void WorldSession::HandleGuildEventLogQuery(WorldPackets::Guild::GuildEventLogQuery& /*packet*/)
{
    TC_LOG_DEBUG("guild", "MSG_GUILD_EVENT_LOG_QUERY [{}]", GetPlayerInfo());

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->SendEventLog(this);
}

void WorldSession::HandleGuildBankMoneyWithdrawn(WorldPackets::Guild::GuildBankRemainingWithdrawMoneyQuery& /*packet*/)
{
    if (Guild* guild = GetPlayer()->GetGuild())
        guild->SendMoneyInfo(this);
}

void WorldSession::HandleGuildPermissionsQuery(WorldPackets::Guild::GuildPermissionsQuery& /* packet */)
{
    if (Guild* guild = GetPlayer()->GetGuild())
        guild->SendPermissions(this);
}

// Called when clicking on Guild bank gameobject
void WorldSession::HandleGuildBankActivate(WorldPackets::Guild::GuildBankActivate& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_BANK_ACTIVATE [{}]: [{}] AllSlots: {}"
        , GetPlayerInfo(), packet.Banker.ToString(), packet.FullUpdate);

    GameObject const* const go = GetPlayer()->GetGameObjectIfCanInteractWith(packet.Banker, GAMEOBJECT_TYPE_GUILD_BANK);
    if (!go)
        return;

    Guild* const guild = GetPlayer()->GetGuild();
    if (!guild)
    {
        Guild::SendCommandResult(this, GUILD_COMMAND_VIEW_TAB, ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    GetPlayer()->PlayerTalkClass->GetInteractionData().StartInteraction(packet.Banker, PlayerInteractionType::GuildBanker);

    guild->SendBankList(this, 0, packet.FullUpdate);
}

// Called when opening guild bank tab only (first one)
void WorldSession::HandleGuildBankQueryTab(WorldPackets::Guild::GuildBankQueryTab& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_BANK_QUERY_TAB [{}]: {}, TabId: {}, ShowTabs: {}"
        , GetPlayerInfo(), packet.Banker.ToString(), packet.Tab, packet.FullUpdate);

    if (GetPlayer()->GetGameObjectIfCanInteractWith(packet.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        if (Guild* guild = GetPlayer()->GetGuild())
            guild->SendBankList(this, packet.Tab, packet.FullUpdate);
}

void WorldSession::HandleGuildBankDepositMoney(WorldPackets::Guild::GuildBankDepositMoney& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_BANK_DEPOSIT_MONEY [{}]: [{}], money: {}",
        GetPlayerInfo(), packet.Banker.ToString(), packet.Money);

    if (GetPlayer()->GetGameObjectIfCanInteractWith(packet.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        if (packet.Money && GetPlayer()->HasEnoughMoney(packet.Money))
            if (Guild* guild = GetPlayer()->GetGuild())
                guild->HandleMemberDepositMoney(this, packet.Money);
}

void WorldSession::HandleGuildBankWithdrawMoney(WorldPackets::Guild::GuildBankWithdrawMoney& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_BANK_WITHDRAW_MONEY [{}]: [{}], money: {}",
        GetPlayerInfo(), packet.Banker.ToString(), packet.Money);

    if (packet.Money && GetPlayer()->GetGameObjectIfCanInteractWith(packet.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        if (Guild* guild = GetPlayer()->GetGuild())
            guild->HandleMemberWithdrawMoney(this, packet.Money);
}

void WorldSession::HandleAutoGuildBankItem(WorldPackets::Guild::AutoGuildBankItem& depositGuildBankItem)
{
    if (!GetPlayer()->GetGameObjectIfCanInteractWith(depositGuildBankItem.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    if (!Player::IsInventoryPos(depositGuildBankItem.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), depositGuildBankItem.ContainerItemSlot))
        GetPlayer()->SendEquipError(EQUIP_ERR_INTERNAL_BAG_ERROR, nullptr);
    else
        guild->SwapItemsWithInventory(GetPlayer(), false, depositGuildBankItem.BankTab, depositGuildBankItem.BankSlot,
            depositGuildBankItem.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), depositGuildBankItem.ContainerItemSlot, 0);
}

void WorldSession::HandleStoreGuildBankItem(WorldPackets::Guild::StoreGuildBankItem& storeGuildBankItem)
{
    if (!GetPlayer()->GetGameObjectIfCanInteractWith(storeGuildBankItem.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    if (!Player::IsInventoryPos(storeGuildBankItem.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), storeGuildBankItem.ContainerItemSlot))
        GetPlayer()->SendEquipError(EQUIP_ERR_INTERNAL_BAG_ERROR, nullptr);
    else
        guild->SwapItemsWithInventory(GetPlayer(), true, storeGuildBankItem.BankTab, storeGuildBankItem.BankSlot,
            storeGuildBankItem.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), storeGuildBankItem.ContainerItemSlot, 0);
}

void WorldSession::HandleSwapItemWithGuildBankItem(WorldPackets::Guild::SwapItemWithGuildBankItem& swapItemWithGuildBankItem)
{
    if (!GetPlayer()->GetGameObjectIfCanInteractWith(swapItemWithGuildBankItem.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    if (!Player::IsInventoryPos(swapItemWithGuildBankItem.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), swapItemWithGuildBankItem.ContainerItemSlot))
        GetPlayer()->SendEquipError(EQUIP_ERR_INTERNAL_BAG_ERROR, nullptr);
    else
        guild->SwapItemsWithInventory(GetPlayer(), false, swapItemWithGuildBankItem.BankTab, swapItemWithGuildBankItem.BankSlot,
            swapItemWithGuildBankItem.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), swapItemWithGuildBankItem.ContainerItemSlot, 0);
}

void WorldSession::HandleSwapGuildBankItemWithGuildBankItem(WorldPackets::Guild::SwapGuildBankItemWithGuildBankItem& swapGuildBankItemWithGuildBankItem)
{
    if (!GetPlayer()->GetGameObjectIfCanInteractWith(swapGuildBankItemWithGuildBankItem.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    guild->SwapItems(GetPlayer(), swapGuildBankItemWithGuildBankItem.BankTab[0], swapGuildBankItemWithGuildBankItem.BankSlot[0],
        swapGuildBankItemWithGuildBankItem.BankTab[1], swapGuildBankItemWithGuildBankItem.BankSlot[1], 0);
}

void WorldSession::HandleMoveGuildBankItem(WorldPackets::Guild::MoveGuildBankItem& moveGuildBankItem)
{
    if (!GetPlayer()->GetGameObjectIfCanInteractWith(moveGuildBankItem.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    guild->SwapItems(GetPlayer(), moveGuildBankItem.BankTab, moveGuildBankItem.BankSlot, moveGuildBankItem.BankTab1, moveGuildBankItem.BankSlot1, 0);
}

void WorldSession::HandleMergeItemWithGuildBankItem(WorldPackets::Guild::MergeItemWithGuildBankItem& mergeItemWithGuildBankItem)
{
    if (!GetPlayer()->GetGameObjectIfCanInteractWith(mergeItemWithGuildBankItem.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    if (!Player::IsInventoryPos(mergeItemWithGuildBankItem.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), mergeItemWithGuildBankItem.ContainerItemSlot))
        GetPlayer()->SendEquipError(EQUIP_ERR_INTERNAL_BAG_ERROR, nullptr);
    else
        guild->SwapItemsWithInventory(GetPlayer(), false, mergeItemWithGuildBankItem.BankTab, mergeItemWithGuildBankItem.BankSlot,
            mergeItemWithGuildBankItem.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), mergeItemWithGuildBankItem.ContainerItemSlot, mergeItemWithGuildBankItem.StackCount);
}

void WorldSession::HandleSplitItemToGuildBank(WorldPackets::Guild::SplitItemToGuildBank& splitItemToGuildBank)
{
    if (!GetPlayer()->GetGameObjectIfCanInteractWith(splitItemToGuildBank.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    if (!Player::IsInventoryPos(splitItemToGuildBank.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), splitItemToGuildBank.ContainerItemSlot))
        GetPlayer()->SendEquipError(EQUIP_ERR_INTERNAL_BAG_ERROR, nullptr);
    else
        guild->SwapItemsWithInventory(GetPlayer(), false, splitItemToGuildBank.BankTab, splitItemToGuildBank.BankSlot,
            splitItemToGuildBank.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), splitItemToGuildBank.ContainerItemSlot, splitItemToGuildBank.StackCount);
}

void WorldSession::HandleMergeGuildBankItemWithItem(WorldPackets::Guild::MergeGuildBankItemWithItem& mergeGuildBankItemWithItem)
{
    if (!GetPlayer()->GetGameObjectIfCanInteractWith(mergeGuildBankItemWithItem.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    if (!Player::IsInventoryPos(mergeGuildBankItemWithItem.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), mergeGuildBankItemWithItem.ContainerItemSlot))
        GetPlayer()->SendEquipError(EQUIP_ERR_INTERNAL_BAG_ERROR, nullptr);
    else
        guild->SwapItemsWithInventory(GetPlayer(), true, mergeGuildBankItemWithItem.BankTab, mergeGuildBankItemWithItem.BankSlot,
            mergeGuildBankItemWithItem.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), mergeGuildBankItemWithItem.ContainerItemSlot, mergeGuildBankItemWithItem.StackCount);
}

void WorldSession::HandleSplitGuildBankItemToInventory(WorldPackets::Guild::SplitGuildBankItemToInventory& splitGuildBankItemToInventory)
{
    if (!GetPlayer()->GetGameObjectIfCanInteractWith(splitGuildBankItemToInventory.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    if (!Player::IsInventoryPos(splitGuildBankItemToInventory.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), splitGuildBankItemToInventory.ContainerItemSlot))
        GetPlayer()->SendEquipError(EQUIP_ERR_INTERNAL_BAG_ERROR, nullptr);
    else
        guild->SwapItemsWithInventory(GetPlayer(), true, splitGuildBankItemToInventory.BankTab, splitGuildBankItemToInventory.BankSlot,
            splitGuildBankItemToInventory.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), splitGuildBankItemToInventory.ContainerItemSlot, splitGuildBankItemToInventory.StackCount);
}

void WorldSession::HandleAutoStoreGuildBankItem(WorldPackets::Guild::AutoStoreGuildBankItem& autoStoreGuildBankItem)
{
    if (!GetPlayer()->GetGameObjectIfCanInteractWith(autoStoreGuildBankItem.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    guild->SwapItemsWithInventory(GetPlayer(), true, autoStoreGuildBankItem.BankTab, autoStoreGuildBankItem.BankSlot,
        INVENTORY_SLOT_BAG_0, NULL_SLOT, 0);
}

void WorldSession::HandleMergeGuildBankItemWithGuildBankItem(WorldPackets::Guild::MergeGuildBankItemWithGuildBankItem& mergeGuildBankItemWithGuildBankItem)
{
    if (!GetPlayer()->GetGameObjectIfCanInteractWith(mergeGuildBankItemWithGuildBankItem.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    guild->SwapItems(GetPlayer(), mergeGuildBankItemWithGuildBankItem.BankTab, mergeGuildBankItemWithGuildBankItem.BankSlot,
        mergeGuildBankItemWithGuildBankItem.BankTab1, mergeGuildBankItemWithGuildBankItem.BankSlot1, mergeGuildBankItemWithGuildBankItem.StackCount);
}

void WorldSession::HandleSplitGuildBankItem(WorldPackets::Guild::SplitGuildBankItem& splitGuildBankItem)
{
    if (!GetPlayer()->GetGameObjectIfCanInteractWith(splitGuildBankItem.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    guild->SwapItems(GetPlayer(), splitGuildBankItem.BankTab, splitGuildBankItem.BankSlot,
        splitGuildBankItem.BankTab1, splitGuildBankItem.BankSlot1, splitGuildBankItem.StackCount);
}

void WorldSession::HandleGuildBankBuyTab(WorldPackets::Guild::GuildBankBuyTab& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_BANK_BUY_TAB [{}]: [{}[, TabId: {}", GetPlayerInfo(), packet.Banker.ToString(), packet.BankTab);

    if (!packet.Banker || GetPlayer()->GetGameObjectIfCanInteractWith(packet.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        if (Guild* guild = GetPlayer()->GetGuild())
            guild->HandleBuyBankTab(this, packet.BankTab);
}

void WorldSession::HandleGuildBankUpdateTab(WorldPackets::Guild::GuildBankUpdateTab& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_BANK_UPDATE_TAB [{}]: [{}], TabId: {}, Name: {}, Icon: {}"
        , GetPlayerInfo(), packet.Banker.ToString(), packet.BankTab, std::string_view(packet.Name), std::string_view(packet.Icon));

    if (!packet.Name.empty() && !packet.Icon.empty())
        if (GetPlayer()->GetGameObjectIfCanInteractWith(packet.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
            if (Guild* guild = GetPlayer()->GetGuild())
                guild->HandleSetBankTabInfo(this, packet.BankTab, packet.Name, packet.Icon);
}

void WorldSession::HandleGuildBankLogQuery(WorldPackets::Guild::GuildBankLogQuery& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_BANK_LOG_QUERY [{}]: TabId: {}", GetPlayerInfo(), packet.Tab);

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->SendBankLog(this, packet.Tab);
}

void WorldSession::HandleGuildBankTextQuery(WorldPackets::Guild::GuildBankTextQuery& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_BANK_QUERY_TEXT [{}]: TabId: {}", GetPlayerInfo(), packet.Tab);

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->SendBankTabText(this, packet.Tab);
}

void WorldSession::HandleGuildBankSetTabText(WorldPackets::Guild::GuildBankSetTabText& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_SET_GUILD_BANK_TEXT [{}]: TabId: {}, Text: {}", GetPlayerInfo(), packet.Tab, std::string_view(packet.TabText));

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->SetBankTabText(packet.Tab, packet.TabText);
}

void WorldSession::HandleGuildSetRankPermissions(WorldPackets::Guild::GuildSetRankPermissions& packet)
{
    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    std::array<GuildBankRightsAndSlots, GUILD_BANK_MAX_TABS> rightsAndSlots;
    for (uint8 tabId = 0; tabId < GUILD_BANK_MAX_TABS; ++tabId)
        rightsAndSlots[tabId] = GuildBankRightsAndSlots(tabId, uint8(packet.TabFlags[tabId]), uint32(packet.TabWithdrawItemLimit[tabId]));

    TC_LOG_DEBUG("guild", "CMSG_GUILD_SET_RANK_PERMISSIONS [{}]: Rank: {} ({})", GetPlayerInfo(), std::string_view(packet.RankName), packet.RankOrder);

    guild->HandleSetRankInfo(this, GuildRankId(packet.RankID), packet.RankName, packet.Flags, packet.WithdrawGoldLimit, rightsAndSlots);
}

void WorldSession::HandleGuildRequestPartyState(WorldPackets::Guild::RequestGuildPartyState& packet)
{
    if (Guild* guild = sGuildMgr->GetGuildByGuid(packet.GuildGUID))
        guild->HandleGuildPartyRequest(this);
}

void WorldSession::HandleGuildChallengeUpdateRequest(WorldPackets::Guild::GuildChallengeUpdateRequest& /*packet*/)
{
    if (Guild* guild = _player->GetGuild())
        guild->HandleGuildRequestChallengeUpdate(this);
}

void WorldSession::HandleDeclineGuildInvites(WorldPackets::Guild::DeclineGuildInvites& packet)
{
    if (packet.Allow)
        GetPlayer()->SetPlayerFlag(PLAYER_FLAGS_AUTO_DECLINE_GUILD);
    else
        GetPlayer()->RemovePlayerFlag(PLAYER_FLAGS_AUTO_DECLINE_GUILD);
}

void WorldSession::HandleRequestGuildRewardsList(WorldPackets::Guild::RequestGuildRewardsList& /*packet*/)
{
    if (sGuildMgr->GetGuildById(_player->GetGuildId()))
    {
        std::vector<GuildReward> const& rewards = sGuildMgr->GetGuildRewards();

        WorldPackets::Guild::GuildRewardList rewardList;
        rewardList.Version = GameTime::GetSystemTime();
        rewardList.RewardItems.reserve(rewards.size());

        for (uint32 i = 0; i < rewards.size(); i++)
        {
            WorldPackets::Guild::GuildRewardItem rewardItem;
            rewardItem.ItemID = rewards[i].ItemID;
            rewardItem.RaceMask = rewards[i].RaceMask;
            rewardItem.MinGuildLevel = 0;
            rewardItem.MinGuildRep = rewards[i].MinGuildRep;
            rewardItem.AchievementsRequired = rewards[i].AchievementsRequired;
            rewardItem.Cost = rewards[i].Cost;
            rewardList.RewardItems.push_back(rewardItem);
        }

        SendPacket(rewardList.Write());
    }
}

void WorldSession::HandleGuildQueryNews(WorldPackets::Guild::GuildQueryNews& newsQuery)
{
    if (Guild* guild = GetPlayer()->GetGuild())
        if (guild->GetGUID() == newsQuery.GuildGUID)
            guild->SendNewsUpdate(this);
}

void WorldSession::HandleGuildNewsUpdateSticky(WorldPackets::Guild::GuildNewsUpdateSticky& packet)
{
    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleNewsSetSticky(this, packet.NewsID, packet.Sticky);
}

void WorldSession::HandleGuildReplaceGuildMaster(WorldPackets::Guild::GuildReplaceGuildMaster& /*replaceGuildMaster*/)
{
    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleSetNewGuildMaster(this, "", true);
}

void WorldSession::HandleGuildSetGuildMaster(WorldPackets::Guild::GuildSetGuildMaster& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_SET_GUILD_MASTER [{}]: Target: {}", GetPlayerInfo(), packet.NewMasterName);

    if (normalizePlayerName(packet.NewMasterName))
        if (Guild* guild = GetPlayer()->GetGuild())
            guild->HandleSetNewGuildMaster(this, packet.NewMasterName, false);
}

void WorldSession::HandleGuildSetAchievementTracking(WorldPackets::Guild::GuildSetAchievementTracking& packet)
{
    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleSetAchievementTracking(this, packet.AchievementIDs.data(), packet.AchievementIDs.data() + packet.AchievementIDs.size());
}

void WorldSession::HandleGuildGetAchievementMembers(WorldPackets::Achievement::GuildGetAchievementMembers& getAchievementMembers)
{
    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleGetAchievementMembers(this, uint32(getAchievementMembers.AchievementID));
}

// Classic 1.60 Guild Finder (Club Finder, guilds only). Layouts read from the 70058 client (classic_re/readseq58.py):
//   CMSG_CLUB_FINDER_POST (writer rva 0x8C4520): bits name 7, description 12, type 3, crossFaction 1; uint64 clubId (guild id),
//     uint64 specs, int32 recruitment flags (1 << ClubFinderSettingFlags), int32 min item level, uint32 avatar; name, description
//   SMSG_CLUB_FINDER_RESPONSE_POST_RECRUITMENT_MESSAGE (0x4602E6): guid clubFinderGUID, bits type 3 + 3
//   SMSG_CLUB_FINDER_LOOKUP_CLUB_POSTINGS_LIST (0x4602E5): uint32 count, postings (reader 0xA46BC0), bits type 3 + last 1
// Requests not understood yet are logged with their bytes and answered with an empty reply.
namespace
{
    constexpr uint8 CLUB_FINDER_TYPE_GUILD = 1;

    struct GuildFinderPosting
    {
        ObjectGuid::LowType GuildId = 0;
        ObjectGuid::LowType Poster = 0;
        std::string Description;
        uint64 Specs = 0;
        int32 Flags = 0;
        int32 MinItemLevel = 0;
        uint32 Avatar = 0;
        int64 Updated = 0;
    };

    std::vector<GuildFinderPosting> LoadGuildFinderPostings(Optional<ObjectGuid::LowType> guildId = {})
    {
        std::vector<GuildFinderPosting> postings;
        std::string query = "SELECT guildId, poster, description, specs, flags, minItemLevel, avatar, updated FROM guild_finder_posting";
        if (guildId)
            query += Trinity::StringFormat(" WHERE guildId = {}", *guildId);
        if (QueryResult result = CharacterDatabase.Query(query.c_str()))
        {
            do
            {
                Field* fields = result->Fetch();
                GuildFinderPosting& posting = postings.emplace_back();
                posting.GuildId = fields[0].GetUInt64();
                posting.Poster = fields[1].GetUInt64();
                posting.Description = fields[2].GetString();
                posting.Specs = fields[3].GetUInt64();
                posting.Flags = fields[4].GetInt32();
                posting.MinItemLevel = fields[5].GetInt32();
                posting.Avatar = fields[6].GetUInt32();
                posting.Updated = fields[7].GetInt64();
            } while (result->NextRow());
        }
        return postings;
    }

    ObjectGuid GuildFinderGuid(ObjectGuid::LowType guildId)
    {
        return ObjectGuid::Create<HighGuid::ClubFinder>(CLUB_FINDER_TYPE_GUILD, uint32(guildId), guildId);
    }

    // one entry of SMSG_CLUB_FINDER_LOOKUP_CLUB_POSTINGS_LIST (client reader rva 0xA46BC0)
    void WriteGuildFinderPosting(WorldPacket& data, GuildFinderPosting const& posting, Guild const* guild)
    {
        std::string leaderName;
        sCharacterCache->GetCharacterNameByGuid(guild->GetLeaderGUID(), leaderName);
        std::string const& name = guild->GetName();

        data.WriteBits(name.size(), 7);
        data.WriteBits(posting.Description.size(), 12);
        data.WriteBits(leaderName.size(), 6);
        data.FlushBits();

        data << GuildFinderGuid(posting.GuildId);
        data << uint32(guild->GetMembersCount());               // numActiveMembers
        data << int64(posting.GuildId);                          // clubId
        data << int32(posting.MinItemLevel);
        data << int32(posting.Avatar);                           // emblemInfo
        data << uint32(posting.Flags);                           // recruitmentFlags
        data << ObjectGuid::Create<HighGuid::Player>(posting.Poster); // lastPosterGUID
        data << int64(posting.Updated);                          // lastUpdatedTime
        data << uint64(posting.Specs);                           // recruitingSpecIds
        data.WriteString(name);
        data.WriteString(posting.Description);
        data.WriteString(leaderName);
    }

    void SendGuildFinderPostings(WorldSession* session, std::vector<GuildFinderPosting> const& postings, bool listedOnly)
    {
        WorldPacket data(SMSG_CLUB_FINDER_LOOKUP_CLUB_POSTINGS_LIST, 64);
        size_t countPos = data.wpos();
        data << uint32(0);
        uint32 count = 0;
        for (GuildFinderPosting const& posting : postings)
        {
            Guild const* guild = sGuildMgr->GetGuildById(posting.GuildId);
            if (!guild || (listedOnly && !(posting.Flags & (1 << 12))))  // ClubFinderSettingFlags::EnableListing
                continue;
            WriteGuildFinderPosting(data, posting, guild);
            ++count;
        }
        data.put<uint32>(countPos, count);
        data.WriteBits(CLUB_FINDER_TYPE_GUILD, 3);
        data.WriteBit(true);                                     // last part of the list
        data.FlushBits();
        session->SendPacket(&data);
    }

    // ---- applications (guild_finder_application.status = PlayerClubRequestStatus: 1 Pending, 3 Declined, 4 Approved, 5 Joined,
    // 6 JoinedAnother)
    struct GuildFinderApplication
    {
        ObjectGuid::LowType GuildId = 0;
        ObjectGuid::LowType Player = 0;
        std::string Comment;
        uint64 Specs = 0;
        uint8 Status = 1;
        int64 Created = 0;
    };

    std::vector<GuildFinderApplication> LoadGuildFinderApplications(char const* column, ObjectGuid::LowType id)
    {
        std::vector<GuildFinderApplication> applications;
        if (QueryResult result = CharacterDatabase.Query(Trinity::StringFormat(
            "SELECT guildId, player, comment, specs, status, created FROM guild_finder_application WHERE {} = {}", column, id).c_str()))
        {
            do
            {
                Field* fields = result->Fetch();
                GuildFinderApplication& application = applications.emplace_back();
                application.GuildId = fields[0].GetUInt64();
                application.Player = fields[1].GetUInt64();
                application.Comment = fields[2].GetString();
                application.Specs = fields[3].GetUInt64();
                application.Status = fields[4].GetUInt8();
                application.Created = fields[5].GetInt64();
            } while (result->NextRow());
        }
        return applications;
    }

    // SMSG_RETURN_APPLICANT_LIST (client reader rva 0x8304F0): the applicants the guild's officers see
    void SendGuildFinderApplicants(WorldSession* session, ObjectGuid::LowType guildId)
    {
        std::vector<GuildFinderApplication> applications = LoadGuildFinderApplications("guildId", guildId);
        std::erase_if(applications, [](GuildFinderApplication const& application) { return application.Status != 1; });

        WorldPacket data(SMSG_RETURN_APPLICANT_LIST, 64);
        data << GuildFinderGuid(guildId);
        data << uint32(applications.size());
        for (GuildFinderApplication const& application : applications)
        {
            ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(application.Player);
            CharacterCacheEntry const* info = sCharacterCache->GetCharacterCacheByGuid(playerGuid);
            Player const* online = ObjectAccessor::FindConnectedPlayer(playerGuid);
            std::string const name = info ? info->Name : "";
            data << GuildFinderGuid(guildId);
            data << playerGuid;
            data << uint32(0);                                               // closed
            data << int8(info ? info->Level : 1);                            // level
            data << int8(info ? info->Class : 0);                            // classID
            data << int32(online ? int32(online->GetAverageItemLevel()) : 0); // ilvl
            data << int32(0);
            data << int64(application.Created);                              // lastUpdatedTime
            data << uint64(application.Specs);                               // specIds
            data << int8(info && Player::TeamForRace(info->Race) == HORDE ? 1 : 0); // faction
            data.WriteBits(name.size(), 6);
            data.WriteBits(application.Comment.size(), 10);
            data.WriteBits(application.Status, 4);                           // requestStatus
            data.WriteBit(true);                                             // lookupSuccess
            data.FlushBits();
            data.WriteString(name);
            data.WriteString(application.Comment);
        }
        data.WriteBits(CLUB_FINDER_TYPE_GUILD, 3);
        data.FlushBits();
        session->SendPacket(&data);
    }

    // SMSG_CLUB_FINDER_RESPONSE_CHARACTER_APPLICATION_LIST (reader rva 0x830870): the applications of the player
    void SendGuildFinderPlayerApplications(WorldSession* session, ObjectGuid const& playerGuid)
    {
        std::vector<GuildFinderApplication> applications = LoadGuildFinderApplications("player", playerGuid.GetCounter());

        WorldPacket data(SMSG_CLUB_FINDER_RESPONSE_CHARACTER_APPLICATION_LIST, 32);
        data << uint32(applications.size());
        for (GuildFinderApplication const& application : applications)
        {
            data << GuildFinderGuid(application.GuildId);
            data << playerGuid;
            data << uint32(0);
            data << uint64(application.Created);
            data.WriteBits(application.Status, 4);
            data.FlushBits();
        }
        data.WriteBits(CLUB_FINDER_TYPE_GUILD, 3);
        data.FlushBits();
        session->SendPacket(&data);
    }

    // refreshes the applicant list of the online officers (guild master and the rank below)
    void NotifyGuildFinderOfficers(ObjectGuid::LowType guildId)
    {
        Guild* guild = sGuildMgr->GetGuildById(guildId);
        if (!guild)
            return;
        for (auto const& [guid, member] : guild->GetMembers())
            if (uint8(member.GetRankId()) <= 1)
                if (Player* officer = ObjectAccessor::FindConnectedPlayer(guid))
                    SendGuildFinderApplicants(officer->GetSession(), guildId);
    }
}

void WorldSession::HandleClubFinderProbe(WorldPackets::Null& packet)
{
    WorldPacket const* raw = packet.GetRawPacket();
    std::string const hex = Trinity::Impl::ByteArrayToHexStr(raw->data(), std::min<size_t>(raw->size(), 256));
    TC_LOG_INFO("network.opcode", "ClubFinder: {} size {} data {} from {}", GetOpcodeNameForLogging(packet.GetOpcode()), raw->size(), hex, GetPlayerInfo());

    if (!sConfigMgr->GetBoolDefault("Classic.GuildFinder", false))
        return;

    WorldPacket data(*raw);
    data.rpos(4);   // skip the opcode

    auto sendEmpty = [this](OpcodeServer opcode, size_t zeroBytes)
    {
        WorldPacket response(opcode, zeroBytes);
        for (size_t i = 0; i < zeroBytes; ++i)
            response << uint8(0);
        SendPacket(&response);
    };

    try
    {
        switch (packet.GetOpcode())
        {
            case CMSG_CLUB_FINDER_POST:
            {
                uint32 nameLength = data.ReadBits(7);
                uint32 descriptionLength = data.ReadBits(12);
                uint32 type = data.ReadBits(3);
                data.ReadBit();                                  // cross faction
                data.ResetBitPos();
                uint64 clubId = data.read<uint64>();
                uint64 specs = data.read<uint64>();
                int32 flags = data.read<int32>();
                int32 minItemLevel = data.read<int32>();
                uint32 avatar = data.read<uint32>();
                data.ReadString(nameLength);
                std::string description(data.ReadString(descriptionLength));

                Guild const* guild = _player->GetGuild();
                Guild::Member const* member = guild ? static_cast<Guild const*>(guild)->GetMember(_player->GetGUID()) : nullptr;
                if (type != CLUB_FINDER_TYPE_GUILD || !guild || guild->GetId() != clubId || !member
                    || uint8(member->GetRankId()) > 1)          // guild master or the rank below
                {
                    sendEmpty(SMSG_CLUB_FINDER_RESPONSE_POST_RECRUITMENT_MESSAGE, 3);
                    break;
                }

                CharacterDatabase.DirectExecute(Trinity::StringFormat(
                    "REPLACE INTO guild_finder_posting (guildId, poster, name, description, specs, flags, minItemLevel, avatar, crossFaction, updated) "
                    "VALUES ({}, {}, '', '{}', {}, {}, {}, {}, 0, {})",
                    guild->GetId(), _player->GetGUID().GetCounter(), [&] { std::string escaped = description; CharacterDatabase.EscapeString(escaped); return escaped; }(),
                    specs, flags, minItemLevel, avatar, int64(GameTime::GetGameTime())).c_str());

                WorldPacket response(SMSG_CLUB_FINDER_RESPONSE_POST_RECRUITMENT_MESSAGE, 20);
                response << GuildFinderGuid(guild->GetId());
                response.WriteBits(CLUB_FINDER_TYPE_GUILD, 3);
                response.WriteBits(0, 3);
                response.FlushBits();
                SendPacket(&response);
                break;
            }
            case CMSG_CLUB_FINDER_REQUEST_CLUBS_LIST:
                // search filters are not applied yet: every listed guild is returned
                SendGuildFinderPostings(this, LoadGuildFinderPostings(), true);
                break;
            case CMSG_CLUB_FINDER_REQUEST_CLUBS_DATA:
                // postings asked for by id (the recruitment dialog loads its own guild's, listed or not)
                SendGuildFinderPostings(this, LoadGuildFinderPostings(), false);
                break;
            case CMSG_CLUB_FINDER_REQUEST_SUBSCRIBED_CLUB_POSTING_IDS:
            {
                // which posting belongs to each of the player's clubs: the recruiter UI finds its applicants through it
                // (reply reader rva 0x8310C0: uint32 count, {int64 clubId, uint32, uint32})
                WorldPacket response(SMSG_CLUB_FINDER_GET_CLUB_POSTING_IDS_RESPONSE, 20);
                Guild const* guild = _player->GetGuild();
                bool posted = guild && !LoadGuildFinderPostings(guild->GetId()).empty();
                response << uint32(posted ? 1 : 0);
                if (posted)
                {
                    response << int64(guild->GetId());              // clubId
                    response << uint32(guild->GetId());             // clubFinderId (posting)
                    response << uint32(CLUB_FINDER_TYPE_GUILD);
                }
                SendPacket(&response);
                break;
            }
            case CMSG_CLUB_FINDER_GET_APPLICANTS_LIST:          // officer: who applied to the guild
                if (Guild const* guild = _player->GetGuild())
                    SendGuildFinderApplicants(this, guild->GetId());
                break;
            case CMSG_CLUB_FINDER_REQUEST_PENDING_CLUBS_LIST:   // player: own applications
                SendGuildFinderPlayerApplications(this, _player->GetGUID());
                break;
            case CMSG_CLUB_FINDER_REQUEST_MEMBERSHIP_TO_CLUB:   // apply (writer rva 0x8C4C20): guid posting, uint64 specs, comment (10 bit length)
            {
                ObjectGuid posting;
                data >> posting;
                uint64 specs = data.read<uint64>();
                uint32 commentLength = data.ReadBits(10);
                data.ResetBitPos();
                std::string comment(data.ReadString(commentLength));
                ObjectGuid::LowType guildId = posting.GetCounter();
                if (!sGuildMgr->GetGuildById(guildId) || _player->GetGuildId())
                    break;
                CharacterDatabase.EscapeString(comment);
                CharacterDatabase.DirectExecute(Trinity::StringFormat(
                    "REPLACE INTO guild_finder_application (guildId, player, comment, specs, status, created) VALUES ({}, {}, '{}', {}, 1, {})",
                    guildId, _player->GetGUID().GetCounter(), comment, specs, int64(GameTime::GetGameTime())).c_str());
                SendGuildFinderPlayerApplications(this, _player->GetGUID());
                NotifyGuildFinderOfficers(guildId);
                break;
            }
            case CMSG_CLUB_FINDER_RESPOND_TO_APPLICANT:         // officer (writer rva 0x8C4F90): guid posting, guid player, accept, type 3, force
            {
                ObjectGuid posting, applicant;
                data >> posting >> applicant;
                bool accept = data.ReadBit();
                data.ReadBits(3);
                data.ReadBit();
                ObjectGuid::LowType guildId = posting.GetCounter();
                Guild* guild = sGuildMgr->GetGuildById(guildId);
                Guild::Member const* member = guild ? static_cast<Guild const*>(guild)->GetMember(_player->GetGUID()) : nullptr;
                if (!member || uint8(member->GetRankId()) > 1)
                    break;
                bool joined = false;
                if (accept && !sCharacterCache->GetCharacterGuildIdByGuid(applicant))
                {
                    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
                    joined = guild->AddMember(trans, applicant);
                    CharacterDatabase.CommitTransaction(trans);
                }
                CharacterDatabase.DirectExecute(Trinity::StringFormat(
                    "UPDATE guild_finder_application SET status = {} WHERE guildId = {} AND player = {}",
                    accept ? (joined ? 5 : 4) : 3, guildId, applicant.GetCounter()).c_str());   // Joined / Approved / Declined
                if (joined)   // only one guild: the other pending applications of the player are done
                    CharacterDatabase.DirectExecute(Trinity::StringFormat(
                        "UPDATE guild_finder_application SET status = 6 WHERE player = {} AND guildId <> {} AND status = 1",
                        applicant.GetCounter(), guildId).c_str());
                NotifyGuildFinderOfficers(guildId);
                if (Player* target = ObjectAccessor::FindConnectedPlayer(applicant))
                    SendGuildFinderPlayerApplications(target->GetSession(), applicant);
                break;
            }
            case CMSG_CLUB_FINDER_APPLICATION_RESPONSE:         // player (writer rva 0x8C51C0): guid posting, type 3, ClubFinderApplicationUpdateType 3
            {
                ObjectGuid posting;
                data >> posting;
                data.ReadBits(3);
                uint32 update = data.ReadBits(3);
                ObjectGuid::LowType guildId = posting.GetCounter();
                if (update == 3)   // Cancel
                    CharacterDatabase.DirectExecute(Trinity::StringFormat(
                        "DELETE FROM guild_finder_application WHERE guildId = {} AND player = {}", guildId, _player->GetGUID().GetCounter()).c_str());
                SendGuildFinderPlayerApplications(this, _player->GetGUID());
                NotifyGuildFinderOfficers(guildId);
                break;
            }
            default:
                break;
        }
    }
    catch (ByteBufferException const& e)
    {
        TC_LOG_ERROR("network.opcode", "ClubFinder: {} could not be read: {}", GetOpcodeNameForLogging(packet.GetOpcode()), e.what());
    }
}

// Classic 1.60 Group Finder (vanilla style premade groups, CMSG_LFG_LIST_*). Listings live in memory only.
namespace
{
    // One posted listing. Request = the client's LFGListJoinRequest bytes as sent (writer rva 0xA4D640); the client reads a
    // request with the mirrored reader (rva 0xA4D0A0), so it is echoed back unchanged in the status and the search results.
    struct LfgListing
    {
        uint32 Id = 0;
        uint32 CategoryId = 0;
        std::vector<uint32> Activities;
        std::vector<uint8> Request;
        time_t Created = 0;
        std::string Name, Comment;                  // as the leader typed them (for the Discord bot's LFG channel)
        uint8 PlayStyle = 0;                        // the play style dropdown: 1 Learning, 2 Relaxed, 3 Competitive, 4 Carry Offered
        bool Silent = false;                        // a group the bots made: not written to the "groupfinder" log (Discord)
    };

    std::unordered_map<ObjectGuid, LfgListing> LfgListings;
    std::unordered_map<ObjectGuid, uint8> LfgListRoles;     // roles the player ticked (Classic roles message)
    uint32 NextLfgListingId = 1;

    // LFGListJoinRequest: bits activity count 5, name 10, comment 11, voice chat 8, 9 flags (4 bools, has int32 x3, has uint8,
    // bool); uint32 category, float item level, {float, float, uint32 n, n x 16 bytes}, int8, n x uint32 activity ids, name,
    // comment, voice chat, the optional int32 x3 and uint8
    bool ReadLfgListRequest(ByteBuffer& data, LfgListing& listing)
    {
        size_t const start = data.rpos();
        uint32 const activityCount = data.ReadBits(5);
        uint32 const nameLength = data.ReadBits(10);
        uint32 const commentLength = data.ReadBits(11);
        uint32 const voiceChatLength = data.ReadBits(8);
        std::array<bool, 9> bits;
        for (bool& bit : bits)
            bit = data.ReadBit();
        data.ResetBitPos();

        listing.CategoryId = data.read<uint32>();
        data.read_skip<float>();                    // required item level
        data.read_skip<float>();
        data.read_skip<float>();
        uint32 const extraCount = data.read<uint32>();
        data.read_skip(extraCount * 16);
        data.read_skip<int8>();
        listing.Activities.clear();
        for (uint32 i = 0; i < activityCount; ++i)
            listing.Activities.push_back(data.read<uint32>());
        listing.Name = data.ReadString(nameLength, false);
        listing.Comment = data.ReadString(commentLength, false);
        data.read_skip(voiceChatLength);
        for (uint8 i = 4; i < 7; ++i)
            if (bits[i])
                data.read_skip<int32>();
        if (bits[7])
            listing.PlayStyle = data.read<uint8>();     // play style (retail LFGEntryGeneralPlaystyle)

        // The client's request ends with one more byte after the optional values (it is 0 in the captured request); the request is
        // echoed whole.
        listing.Request.assign(data.data() + start, data.data() + data.size());
        return data.rpos() == data.size() || data.rpos() + 1 == data.size();
    }

    WorldPackets::LFG::RideTicket LfgListTicket(ObjectGuid const& leader, LfgListing const& listing)
    {
        WorldPackets::LFG::RideTicket ticket;
        ticket.RequesterGuid = leader;
        ticket.Id = listing.Id;
        ticket.Type = WorldPackets::LFG::RideType::Lfg;
        ticket.Time = listing.Created;
        return ticket;
    }

    // SMSG_LFG_LIST_UPDATE_STATUS (Classic 0x5B000A, reader rva 0xA49980): ticket, request, uint64 remaining time, uint8 result,
    // bits listed, has guid, has uint8, then the optional guid and uint8
    void SendLfgListStatus(WorldSession* session, LfgListing const& listing, bool listed)
    {
        WorldPacket data(SMSG_LFG_LIST_UPDATE_STATUS, 64);
        data << LfgListTicket(session->GetPlayer()->GetGUID(), listing);
        data.append(listing.Request.data(), listing.Request.size());
        data << uint64(listed ? 1800 : 0);
        data << uint8(0);
        data.WriteBit(listed);
        data.WriteBit(false);
        data.WriteBit(false);
        data.FlushBits();
        session->SendPacket(&data);
    }

    // The request bytes of a listing the server makes itself, laid out as a request of the real client (captured 2026-10-20: all nine
    // flag bits off, no name or comment, the int8 after the three floats 2 = Relaxed, the activity id, one closing zero byte).
    std::vector<uint8> BuildLfgListRequest(uint32 categoryId, std::vector<uint32> const& activities, std::string_view name,
                                           std::string_view comment, uint8 playStyle)
    {
        ByteBuffer data;
        data.WriteBits(uint32(std::min<size_t>(activities.size(), 31)), 5);
        data.WriteBits(uint32(std::min<size_t>(name.size(), 1023)), 10);
        data.WriteBits(uint32(std::min<size_t>(comment.size(), 2047)), 11);
        data.WriteBits(0, 8);
        for (uint8 i = 0; i < 9; ++i)
            data.WriteBit(false);
        data.FlushBits();
        data << uint32(categoryId);
        data << float(0.0f) << float(0.0f) << float(0.0f);
        data << uint32(0);
        data << int8(playStyle);
        for (size_t i = 0; i < std::min<size_t>(activities.size(), 31); ++i)
            data << uint32(activities[i]);
        // ByteBuffer::append asserts on a zero-sized value: an empty name or comment writes nothing
        if (size_t const nameSize = std::min<size_t>(name.size(), 1023))
            data.append(name.data(), nameSize);
        if (size_t const commentSize = std::min<size_t>(comment.size(), 2047))
            data.append(comment.data(), commentSize);
        data << uint8(0);
        return std::vector<uint8>(data.data(), data.data() + data.size());
    }

    // one LFGListSearchResult (Classic reader rva 0xA4EC70)
    void WriteLfgListSearchResult(WorldPacket& data, ObjectGuid const& leader, LfgListing const& listing)
    {
        data << LfgListTicket(leader, listing);
        data << uint32(listing.Id);                 // sequence
        data.append(listing.Request.data(), listing.Request.size());
        data << uint8(0);
        data << leader;                             // leader
        data << leader;                             // last touched: any, name, comment
        data << leader;
        data << leader;
        data << ObjectGuid::Empty;                  // last touched voice chat
        data << uint32(GetVirtualRealmAddress());
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);                          // battle.net friends
        data << uint32(0);                          // character friends
        data << uint32(0);                          // guild mates
        // the leader's group as it is now (only the leader when not in a group)
        std::vector<ObjectGuid> members;
        Player const* leaderPlayer = ObjectAccessor::FindConnectedPlayer(leader);
        if (Group const* group = leaderPlayer ? leaderPlayer->GetGroup() : nullptr)
            for (Group::MemberSlot const& slot : group->GetMemberSlots())
                members.push_back(slot.guid);
        else
            members.push_back(leader);
        data << uint32(members.size());
        data << uint32(0);                          // completed encounters
        data << uint64(listing.Created);
        data << uint8(0);                           // application status
        data << ObjectGuid::Empty;                  // party
        data << float(0.0f);
        data << float(0.0f);
        data << uint32(0);
        for (uint8 i = 0; i < 9; ++i)
        {
            data << int32(0);
            data << int8(0);
        }
        data << uint8(0);
        data << uint8(0);

        // members (Classic reader rva 0xA4EA40): guid, int8 level, int8 class, int8 role, int32 area, uint8, {guid, float, uint32 x3,
        // int32, uint64 x2, int32, uint8}, bit leader. The role is the one of the character's talent specialization (0 tank,
        // 1 healer, 2 damage), not the roles ticked in the window: official search results of 2026-10-06 (70235) show the same
        // role for a member whatever they ticked, and tank/healer only for classes and specs that can do it.
        for (ObjectGuid const& guid : members)
        {
            Player const* member = ObjectAccessor::FindConnectedPlayer(guid);
            CharacterCacheEntry const* info = sCharacterCache->GetCharacterCacheByGuid(guid);
            ChrSpecializationEntry const* spec = member ? member->GetPrimarySpecializationEntry() : nullptr;
            data << guid;
            data << int8(member ? member->GetLevel() : info ? info->Level : 0);
            data << int8(member ? member->GetClass() : info ? info->Class : 0);
            // the role the group gave the member (a bot has no talent specialization of its own), else the specialization's
            uint8 const groupRole = leaderPlayer && leaderPlayer->GetGroup() ? leaderPlayer->GetGroup()->GetLfgRoles(guid) : 0;
            int8 const role = (groupRole & lfg::PLAYER_ROLE_TANK) ? 0 : (groupRole & lfg::PLAYER_ROLE_HEALER) ? 1
                : (groupRole & lfg::PLAYER_ROLE_DAMAGE) ? 2 : int8(spec ? spec->Role : 2);
            data << role;
            data << int32(member ? member->GetZoneId() : 0);
            data << uint8(member ? member->GetLevel() : 0);
            data << ObjectGuid::Empty;
            data << float(0.0f);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << int32(0);
            data << uint64(0);
            data << uint64(0);
            data << int32(0);
            data << uint8(0);
            data.WriteBit(guid == leader);
            data.FlushBits();
        }

        data.WriteBit(false);                       // (entry reader ends with one bit)
        data.FlushBits();
    }

    std::string JsonString(std::string_view text)
    {
        std::string out = "\"";
        for (char c : text)
        {
            switch (c)
            {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': case '\t': out += ' '; break;
                default:
                    if (uint8(c) < 0x20)
                        out += ' ';
                    else
                        out += c;
            }
        }
        return out + "\"";
    }

    // One line of the "groupfinder" log per change of a listing, read by the Discord bot (contrib/discord_bot, its LFG channel):
    // {"event": "listed" | "updated" | "delisted", "id", "faction", "activities", "title", "comment", "members": [...]}
    void LogLfgListing(char const* event, ObjectGuid const& leader, LfgListing const& listing)
    {
        if (listing.Silent)
            return;
        if (!sLog->ShouldLog("groupfinder", LOG_LEVEL_INFO))
            return;

        std::string json = Trinity::StringFormat(R"({{"event":"{}","id":{},"time":{})", event, listing.Id, uint64(listing.Created));
        if (strcmp(event, "delisted") != 0)
        {
            Player const* leaderPlayer = ObjectAccessor::FindConnectedPlayer(leader);
            json += Trinity::StringFormat(R"(,"faction":"{}","activities":[)", leaderPlayer && leaderPlayer->GetTeam() == ALLIANCE ? "A" : "H");
            for (size_t i = 0; i < listing.Activities.size(); ++i)
                json += (i ? "," : "") + std::to_string(listing.Activities[i]);
            json += "],\"title\":" + JsonString(listing.Name) + ",\"comment\":" + JsonString(listing.Comment)
                + ",\"playstyle\":" + std::to_string(listing.PlayStyle) + ",\"members\":[";

            // the leader's group as it is now (only the leader when not in a group)
            std::vector<ObjectGuid> members;
            Group const* group = leaderPlayer ? leaderPlayer->GetGroup() : nullptr;
            if (group)
                for (Group::MemberSlot const& slot : group->GetMemberSlots())
                    members.push_back(slot.guid);
            else
                members.push_back(leader);
            bool first = true;
            for (ObjectGuid const& guid : members)
            {
                CharacterCacheEntry const* info = sCharacterCache->GetCharacterCacheByGuid(guid);
                if (!info)
                    continue;
                // the party role (set in the group, CMSG_SET_ROLE) wins over the role ticked in the Group Finder window
                auto roles = LfgListRoles.find(guid);
                uint8 const partyRole = group ? group->GetLfgRoles(guid) : 0;
                uint8 const mask = (partyRole & (lfg::PLAYER_ROLE_TANK | lfg::PLAYER_ROLE_HEALER | lfg::PLAYER_ROLE_DAMAGE))
                    ? partyRole : (roles != LfgListRoles.end() ? roles->second : 0);
                json += Trinity::StringFormat(R"({}{{"name":{},"class":{},"level":{},"leader":{},"tank":{},"healer":{},"damage":{}}})",
                    first ? "" : ",", JsonString(info->Name), info->Class, info->Level, guid == leader ? "true" : "false",
                    (mask & lfg::PLAYER_ROLE_TANK) ? "true" : "false", (mask & lfg::PLAYER_ROLE_HEALER) ? "true" : "false",
                    (mask & lfg::PLAYER_ROLE_DAMAGE) ? "true" : "false");
                first = false;
            }
            json += "]";
        }
        json += "}";
        TC_LOG_INFO("groupfinder", "{}", json);
    }

    // the listing a player belongs to: their own, or their group leader's
    std::pair<ObjectGuid, LfgListing const*> LfgListingOf(Player const* player)
    {
        ObjectGuid leader = player->GetGroup() ? player->GetGroup()->GetLeaderGUID() : player->GetGUID();
        auto itr = LfgListings.find(leader);
        if (itr == LfgListings.end())
        {
            leader = player->GetGUID();
            itr = LfgListings.find(leader);
        }
        return { leader, itr != LfgListings.end() ? &itr->second : nullptr };
    }
}

// Discord bot LFG channel (GroupFinderListings.h): keep the posted group up to date when its members change
void GroupFinderListings::OnGroupChanged(Group const* group)
{
    if (!group)
        return;
    auto itr = LfgListings.find(group->GetLeaderGUID());
    if (itr != LfgListings.end())
        LogLfgListing("updated", itr->first, itr->second);
}

void GroupFinderListings::OnLogout(Player const* player)
{
    auto itr = LfgListings.find(player->GetGUID());
    if (itr == LfgListings.end())
        return;
    LogLfgListing("delisted", itr->first, itr->second);
    LfgListings.erase(itr);
}

void GroupFinderListings::PostBotListing(Player const* leader, uint32 categoryId, std::vector<uint32> const& activities,
                                         std::string const& name, std::string const& comment, uint8 playStyle)
{
    LfgListing listing;
    listing.Id = NextLfgListingId++;
    listing.CategoryId = categoryId;
    listing.Activities = activities;
    listing.Created = GameTime::GetGameTime();
    listing.Name = name;
    listing.Comment = comment;
    listing.PlayStyle = playStyle;
    listing.Silent = true;
    listing.Request = BuildLfgListRequest(categoryId, activities, name, comment, playStyle);
    LfgListings[leader->GetGUID()] = std::move(listing);
}

void GroupFinderListings::RemoveBotListing(Player const* leader)
{
    OnLogout(leader);
}

uint32 GroupFinderListings::CategoryOf(Player const* leader)
{
    auto itr = LfgListings.find(leader->GetGUID());
    return itr != LfgListings.end() ? itr->second.CategoryId : 0;
}

void WorldSession::HandleLfgListProbe(WorldPackets::Null& packet)
{
    WorldPacket const* raw = packet.GetRawPacket();
    std::string const hex = Trinity::Impl::ByteArrayToHexStr(raw->data(), std::min<size_t>(raw->size(), 256));
    TC_LOG_INFO("network.opcode", "GroupFinder: {} size {} data {} from {}", GetOpcodeNameForLogging(packet.GetOpcode()), raw->size(), hex, GetPlayerInfo());

    WorldPacket data(*raw);
    data.rpos(4);   // skip the opcode

    try
    {
        switch (packet.GetOpcode())
        {
            case CMSG_LFG_LIST_JOIN:            // create or re-post the player's listing
            {
                LfgListing listing;
                if (!ReadLfgListRequest(data, listing))
                    TC_LOG_ERROR("network.opcode", "GroupFinder: join request has {} unread bytes", data.size() - data.rpos());
                auto itr = LfgListings.find(_player->GetGUID());
                listing.Id = itr != LfgListings.end() ? itr->second.Id : NextLfgListingId++;
                listing.Created = GameTime::GetGameTime();
                LfgListing const& stored = LfgListings[_player->GetGUID()] = std::move(listing);

                // SMSG_LFG_LIST_JOIN_RESULT (Classic 0x5B0001, case 0xA4A958): ticket, int32, uint8 result, uint8 detail
                WorldPacket result(SMSG_LFG_LIST_JOIN_RESULT, 40);
                result << LfgListTicket(_player->GetGUID(), stored);
                result << int32(0);
                result << uint8(0);
                result << uint8(0);
                SendPacket(&result);
                SendLfgListStatus(this, stored, true);
                LogLfgListing("listed", _player->GetGUID(), stored);
                sScriptMgr->OnPlayerGroupFinderListing(_player, stored.CategoryId, stored.Activities, true);
                break;
            }
            case CMSG_LFG_LIST_APPLY_TO_GROUP:  // the reply layouts are not captured: the ticket is read as the one of the search result
            {
                // A bot group listed by the module takes the applicant at once (the same as inviting a member). The packet is in
                // the log above; a ticket that does not read or names no listed bot group is only logged.
                WorldPackets::LFG::RideTicket ticket;
                data >> ticket;
                auto itr = LfgListings.find(ticket.RequesterGuid);
                Player* leader = ObjectAccessor::FindConnectedPlayer(ticket.RequesterGuid);
                if (itr == LfgListings.end() || !itr->second.Silent || !leader || !leader->GetGroup())
                {
                    TC_LOG_INFO("network.opcode", "GroupFinder: apply by {} names ticket leader {} id {}: no listed bot group", GetPlayerInfo(),
                        ticket.RequesterGuid.ToString(), ticket.Id);
                    break;
                }
                bool const taken = !_player->GetGroup() && sScriptMgr->OnPlayerGroupFinderJoinRequest(leader, _player);
                TC_LOG_INFO("network.opcode", "GroupFinder: apply by {} to {}: {}", GetPlayerInfo(), leader->GetName(), taken ? "joined" : "not taken");
                break;
            }
            case CMSG_LFG_LIST_UPDATE_REQUEST:  // edit (serializer rva 0x944C00): ticket, request
            {
                WorldPackets::LFG::RideTicket ticket;
                data >> ticket;
                auto itr = LfgListings.find(_player->GetGUID());
                if (itr == LfgListings.end())
                    break;
                LfgListing listing;
                if (!ReadLfgListRequest(data, listing))
                    TC_LOG_ERROR("network.opcode", "GroupFinder: update request has {} unread bytes", data.size() - data.rpos());
                listing.Id = itr->second.Id;
                listing.Created = itr->second.Created;
                itr->second = std::move(listing);
                SendLfgListStatus(this, itr->second, true);
                LogLfgListing("updated", itr->first, itr->second);
                break;
            }
            case CMSG_LFG_LIST_LEAVE:           // delist (Classic 0x440038): ticket
            {
                auto itr = LfgListings.find(_player->GetGUID());
                if (itr == LfgListings.end())
                    break;
                LfgListing const listing = std::move(itr->second);
                LfgListings.erase(itr);
                SendLfgListStatus(this, listing, false);
                LogLfgListing("delisted", _player->GetGUID(), listing);
                sScriptMgr->OnPlayerGroupFinderListing(_player, listing.CategoryId, listing.Activities, false);
                break;
            }
            case CMSG_PERKS_PROGRAM_REQUEST_REFUND: // Classic 0x3E02B3: the roles ticked in the Group Finder (uint8 mask)
            {
                LfgListRoles[_player->GetGUID()] = data.read<uint8>();
                auto [leader, listing] = LfgListingOf(_player);
                if (listing)
                    LogLfgListing("updated", leader, *listing);
                break;
            }
            case CMSG_LFG_LIST_GET_STATUS:      // login
            {
                auto itr = LfgListings.find(_player->GetGUID());
                if (itr != LfgListings.end())
                    SendLfgListStatus(this, itr->second, true);
                break;
            }
            case CMSG_LFG_LIST_SEARCH:          // (writer rva 0xA4E3B0) see below; the ticked activity ids come last
            {
                // flags byte, uint32 category, uint32, uint32, int32, uint32 count A, uint32, uint32 count B, uint32 count C,
                // uint32, uint8, uint8, uint32 count D, D entries (not read: only 0 seen), A x uint32, B x uint32, C x activity id
                data.read_skip<uint8>();
                uint32 const categoryId = data.read<uint32>();
                data.read_skip(3 * sizeof(uint32));
                uint32 const countA = data.read<uint32>();
                data.read_skip<uint32>();
                uint32 const countB = data.read<uint32>();
                uint32 const countC = data.read<uint32>();
                data.read_skip<uint32>();
                data.read_skip(2 * sizeof(uint8));
                uint32 const countD = data.read<uint32>();
                std::unordered_set<uint32> activities;
                if (!countD)
                {
                    data.read_skip((countA + countB) * sizeof(uint32));
                    for (uint32 i = 0; i < countC; ++i)
                        activities.insert(data.read<uint32>());
                }

                sScriptMgr->OnPlayerGroupFinderSearch(_player);   // the bots list a group for this player's faction when there is none
                WorldPacket results(SMSG_LFG_LIST_SEARCH_RESULTS, 256);
                std::vector<std::pair<ObjectGuid, LfgListing const*>> found;
                bool const crossFaction = sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_GROUP);
                for (auto itr = LfgListings.begin(); itr != LfgListings.end();)
                {
                    Player* leaderPlayer = ObjectAccessor::FindConnectedPlayer(itr->first);
                    if (!leaderPlayer)
                    {
                        LogLfgListing("delisted", itr->first, itr->second);
                        itr = LfgListings.erase(itr);   // leader logged out
                        continue;
                    }
                    // only groups of the searcher's own faction, like groups themselves (AllowTwoSide.Interaction.Group)
                    if (!crossFaction && leaderPlayer->GetTeam() != GetPlayer()->GetTeam())
                    {
                        ++itr;
                        continue;
                    }
                    // A bot group does not know the client's category id: it is shown in the category the search asks for when one of its
                    // activities is among the searched ones, or when no activity is searched (an activity id belongs to one dungeon).
                    // A listing carries its activity id shifted left by 8 bits (Upper Blackrock Spire 214272 = 837 << 8) while the search
                    // filter carries the plain id (837): either form matches.
                    bool const activityMatch = std::ranges::any_of(itr->second.Activities,
                        [&](uint32 activityId) { return activities.contains(activityId) || activities.contains(activityId >> 8); });
                    bool const match = itr->second.Silent ? (activities.empty() || activityMatch)
                        : (itr->second.CategoryId == categoryId && (activities.empty() || activityMatch));
                    if (itr->second.Silent)
                    {
                        TC_LOG_INFO("network.opcode", "GroupFinder: bot listing of {} category {} activity {}: {}", leaderPlayer->GetName(),
                            itr->second.CategoryId, itr->second.Activities.empty() ? 0 : itr->second.Activities.front(), match ? "shown" : "no match");
                        if (match && itr->second.CategoryId != categoryId)
                        {
                            itr->second.CategoryId = categoryId;
                            itr->second.Request = BuildLfgListRequest(categoryId, itr->second.Activities, itr->second.Name, itr->second.Comment,
                                itr->second.PlayStyle);
                        }
                    }
                    if (match)
                        found.emplace_back(itr->first, &itr->second);
                    ++itr;
                }
                // the searched activity ids are how the id of a dungeon or raid is read: pick it in the filter and search
                std::string searched;
                for (uint32 activityId : activities)
                    searched += (searched.empty() ? "" : ",") + std::to_string(activityId);
                TC_LOG_INFO("network.opcode", "GroupFinder: search category {} activities [{}] by {}: {} of {} listing(s) found", categoryId,
                    searched, GetPlayerInfo(), found.size(), LfgListings.size());
                results << uint16(found.size());
                results << uint32(found.size());
                for (auto const& [leader, listing] : found)
                    WriteLfgListSearchResult(results, leader, *listing);
                SendPacket(&results);
                break;
            }
            case CMSG_REQUEST_LFG_LIST_BLACKLIST:     // login: empty blacklist (uint32 count), the category list waits for it
            {
                WorldPacket blacklist(SMSG_LFG_LIST_UPDATE_BLACKLIST, 4);
                blacklist << uint32(0);
                SendPacket(&blacklist);
                break;
            }
            default:
                break;
        }
    }
    catch (ByteBufferException const& e)
    {
        TC_LOG_ERROR("network.opcode", "GroupFinder: {} could not be read: {}", GetOpcodeNameForLogging(packet.GetOpcode()), e.what());
    }
}
