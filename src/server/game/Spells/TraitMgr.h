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

#ifndef TRINITY_TRAIT_MGR_H
#define TRINITY_TRAIT_MGR_H

#include "Define.h"
#include <map>
#include <variant>
#include <vector>

class Player;
struct TraitDefinitionEffectPointsEntry;
enum class TraitConfigType : int32;
enum TalentLearnResult : int32;

namespace UF
{
struct TraitConfig;
struct TraitEntry;
}

namespace WorldPackets::Traits
{
struct TraitConfig;
struct TraitEntry;
}

namespace TraitMgr
{
constexpr uint32 COMMIT_COMBAT_TRAIT_CONFIG_CHANGES_SPELL_ID = 384255u;
constexpr uint32 MAX_COMBAT_TRAIT_CONFIGS = 10u;

enum class LearnResult : int32
{
    Ok                              = 0,
    Unknown                         = 1,
    NotEnoughTalentsInPrimaryTree   = 2,
    NoPrimaryTreeSelected           = 3,
    CantDoThatRightNow              = 4,
    AffectingCombat                 = 5,
    CantRemoveTalent                = 6,
    CantDoThatChallengeModeActive   = 7,
    RestArea                        = 8,
    UnspentTalentPoints             = 9,
    InPvpMatch                      = 10
};

struct PlayerDataAccessor
{
    /*implicit*/ PlayerDataAccessor(Player const* player) : _player(player) { }

    uint64 GetMoney() const;
    int32 GetCurrencyQuantity(int32 currencyId) const;
    int32 GetLevel() const;
    int32 GetClassicTalentedStartLevel() const;
    bool IsQuestRewarded(int32 questId) const;
    bool HasAchieved(int32 achievementId) const;
    uint32 GetPrimarySpecialization() const;
    std::variant<int64, float> GetDataElementAccount(uint32 dataElementId) const;
    std::variant<int64, float> GetDataElementCharacter(uint32 dataElementId) const;

private:
    Player const* _player;
};

struct SpentCurrency
{
    int32 Total = 0;
    std::vector<std::pair<int32, int32>> ByGate;
};

void Load();
int32 GenerateNewTraitConfigId();
TraitConfigType GetConfigTypeForTree(int32 traitTreeId);
void FillSpentCurrenciesMap(std::vector<WorldPackets::Traits::TraitEntry> const& traitEntries, std::map<int32, SpentCurrency>& cachedCurrencies);
std::vector<UF::TraitEntry> GetGrantedTraitEntriesForConfig(WorldPackets::Traits::TraitConfig const& traitConfig, PlayerDataAccessor player);
bool IsValidEntry(WorldPackets::Traits::TraitEntry const& traitEntry);
LearnResult ValidateConfig(WorldPackets::Traits::TraitConfig& traitConfig, PlayerDataAccessor player, bool requireSpendingAllCurrencies = false, bool removeInvalidEntries = false);
LearnResult ValidateConfigForCommit(WorldPackets::Traits::TraitConfig& traitConfig, PlayerDataAccessor player);
bool CanApplyTraitNode(UF::TraitConfig const& traitConfig, UF::TraitEntry const& traitEntry);
std::vector<TraitDefinitionEffectPointsEntry const*> const* GetTraitDefinitionEffectPointModifiers(int32 traitDefinitionId);
void InitializeStarterBuildTraitConfig(WorldPackets::Traits::TraitConfig& traitConfig, PlayerDataAccessor player);

// Every node entry of the trees a config can spend in (the class talent trees for a Combat config). Used by
// PlayerbotV2 to resolve talents by spell when it builds a config for a bot.
struct NodeEntryInfo
{
    int32 TraitNodeID = 0;
    int32 TraitNodeEntryID = 0;
    int32 MaxRanks = 0;
    int32 TraitDefinitionID = 0;
};
std::vector<NodeEntryInfo> GetNodeEntriesForConfig(WorldPackets::Traits::TraitConfig const& traitConfig);
// The currencies buying a rank of this node spends, sorted. A tier gate counts the spend of one of them, so nodes that
// share a gate's currency share a tree; Classic talent tabs differ here even when they share a TraitTree.
std::vector<int32> GetCostCurrenciesForNode(int32 traitNodeId);
}

#endif // TRINITY_TRAIT_MGR_H
