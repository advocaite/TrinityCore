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

#include "tc_catch2.h"
#include "DB2Stores.h"
#include "DummyData.h"
#include "TraitMgr.h"
#include "TraitPacketsCommon.h"

namespace
{
    // One process-lifetime fixture: TraitMgr::Load is a startup operation.
    // Skill 0 is the isolated fixture's class-to-tree route; no Player or DB is used.
    struct TraitData
    {
        UnitTestDataLoader::DB2<ChrSpecializationEntry, &ChrSpecializationEntry::ID> Specs{sChrSpecializationStore};
        UnitTestDataLoader::DB2<TraitTreeEntry, &TraitTreeEntry::ID> Trees{sTraitTreeStore};
        UnitTestDataLoader::DB2<TraitNodeEntry, &TraitNodeEntry::ID> Nodes{sTraitNodeStore};
        UnitTestDataLoader::DB2<TraitNodeEntryEntry, &TraitNodeEntryEntry::ID> Entries{sTraitNodeEntryStore};
        UnitTestDataLoader::DB2<TraitNodeXTraitNodeEntryEntry, &TraitNodeXTraitNodeEntryEntry::ID> NodeEntries{sTraitNodeXTraitNodeEntryStore};
        UnitTestDataLoader::DB2<TraitCurrencyEntry, &TraitCurrencyEntry::ID> Currencies{sTraitCurrencyStore};
        UnitTestDataLoader::DB2<TraitCurrencySourceEntry, &TraitCurrencySourceEntry::ID> Sources{sTraitCurrencySourceStore};
        UnitTestDataLoader::DB2<TraitTreeXTraitCurrencyEntry, &TraitTreeXTraitCurrencyEntry::ID> TreeCurrencies{sTraitTreeXTraitCurrencyStore};
        UnitTestDataLoader::DB2<TraitCostEntry, &TraitCostEntry::ID> Costs{sTraitCostStore};
        UnitTestDataLoader::DB2<TraitNodeXTraitCostEntry, &TraitNodeXTraitCostEntry::ID> NodeCosts{sTraitNodeXTraitCostStore};
        UnitTestDataLoader::DB2<TraitEdgeEntry, &TraitEdgeEntry::ID> Edges{sTraitEdgeStore};
        UnitTestDataLoader::DB2<SkillLineEntry, &SkillLineEntry::ID> Skills{sSkillLineStore};
        UnitTestDataLoader::DB2<SkillLineXTraitTreeEntry, &SkillLineXTraitTreeEntry::ID> SkillTrees{sSkillLineXTraitTreeStore};

        TraitData()
        {
            {
                auto loader = Specs.Loader();
                // Native 70124 identity rows; class 13 is a future-layout control.
                for (auto [classId, id] : {std::pair{1, 1491}, {2, 1486}, {3, 1485},
                    {4, 1488}, {5, 1487}, {7, 1489}, {8, 1482}, {9, 1490}, {11, 1484}, {13, 113}})
                {
                    auto& spec = loader.Add();
                    spec.ID = id;
                    spec.ClassID = classId;
                }
                for (int8 index : {0, 1})
                {
                    auto& spec = loader.Add();
                    spec.ID = 200 + index;
                    spec.ClassID = 6;
                    spec.OrderIndex = index;
                }
                auto& petSpec = loader.Add();
                petSpec.ID = 300;
                auto& nonzeroIndex = loader.Add();
                nonzeroIndex.ID = 301;
                nonzeroIndex.ClassID = 10;
                nonzeroIndex.OrderIndex = 1;
            }
            { auto loader = Trees.Loader(); loader.Add().ID = 10001; }
            {
                auto loader = Nodes.Loader();
                for (uint32 id : {10001, 10002, 10003})
                {
                    auto& node = loader.Add();
                    node.ID = id;
                    node.TraitTreeID = 10001;
                }
            }
            {
                auto loader = Entries.Loader();
                for (uint32 id : {10001, 10002, 10003})
                {
                    auto& entry = loader.Add();
                    entry.ID = id;
                    entry.MaxRanks = id == 10002 ? 2 : 5;
                }
            }
            {
                auto loader = NodeEntries.Loader();
                for (uint32 id : {10001, 10002, 10003})
                {
                    auto& link = loader.Add();
                    link.ID = link.TraitNodeID = link.TraitNodeEntryID = id;
                }
            }
            {
                auto loader = Currencies.Loader();
                auto& currency = loader.Add();
                currency.ID = 10001;
                currency.Type = int32(TraitCurrencyType::TraitSourced);
            }
            {
                auto loader = Sources.Loader();
                auto& source = loader.Add();
                source.ID = source.TraitCurrencyID = 10001;
                source.Amount = 2;
            }
            {
                auto loader = TreeCurrencies.Loader();
                auto& link = loader.Add();
                link.ID = link.TraitTreeID = link.TraitCurrencyID = 10001;
            }
            {
                auto loader = Costs.Loader();
                auto& cost = loader.Add();
                cost.ID = cost.TraitCurrencyID = 10001;
                cost.Amount = 1;
            }
            {
                auto loader = NodeCosts.Loader();
                for (uint32 id : {10001, 10002, 10003})
                {
                    auto& link = loader.Add();
                    link.ID = link.TraitNodeID = id;
                    link.TraitCostID = 10001;
                }
            }
            {
                auto loader = Edges.Loader();
                auto& edge = loader.Add();
                edge.ID = 10001;
                edge.LeftTraitNodeID = 10002;
                edge.RightTraitNodeID = 10003;
                edge.Type = int32(TraitEdgeType::RequiredForAvailability);
            }
            { auto loader = Skills.Loader(); loader.Add().ID = 0; }
            {
                auto loader = SkillTrees.Loader();
                auto& link = loader.Add();
                link.ID = link.TraitTreeID = 10001;
                link.SkillLineID = 0;
            }
            TraitMgr::Load();
        }
    };

    WorldPackets::Traits::TraitConfig Config(int32 spec = 1482, int32 rank = 1, int32 node = 10001)
    {
        static TraitData data;
        WorldPackets::Traits::TraitConfig config;
        config.Type = TraitConfigType::Combat;
        config.ChrSpecializationID = spec;
        auto& entry = config.Entries.emplace_back();
        entry.TraitNodeID = entry.TraitNodeEntryID = node;
        entry.Rank = rank;
        return config;
    }
}

TEST_CASE("Single-specialization classes can commit one point with points left", "[IncrementalTalentCommit]")
{
    for (int32 spec : {1491, 1486, 1485, 1488, 1487, 1489, 1482, 1490, 1484, 113})
    {
        auto config = Config(spec);
        CHECK(TraitMgr::ValidateConfigForCommit(config, nullptr) == TraitMgr::LearnResult::Ok);
    }
}

TEST_CASE("Incremental commits retain currency rank and prerequisite checks", "[IncrementalTalentCommit]")
{
    auto overspend = Config(1482, 3);
    CHECK(TraitMgr::ValidateConfigForCommit(overspend, nullptr) == TraitMgr::LearnResult::NotEnoughTalentsInPrimaryTree);
    auto overflow = Config(1482, 6);
    CHECK(TraitMgr::ValidateConfigForCommit(overflow, nullptr) == TraitMgr::LearnResult::Unknown);
    auto missingParent = Config(1482, 1, 10003);
    CHECK(TraitMgr::ValidateConfigForCommit(missingParent, nullptr) == TraitMgr::LearnResult::NotEnoughTalentsInPrimaryTree);
}

TEST_CASE("Multi-specialization classes still require all points spent", "[IncrementalTalentCommit]")
{
    for (int32 spec : {200, 201})
    {
        auto partial = Config(spec);
        CHECK(TraitMgr::ValidateConfigForCommit(partial, nullptr) == TraitMgr::LearnResult::UnspentTalentPoints);
        auto full = Config(spec, 2);
        CHECK(TraitMgr::ValidateConfigForCommit(full, nullptr) == TraitMgr::LearnResult::Ok);
    }
}

TEST_CASE("Pet and incomplete specialization layouts retain strict commit policy", "[IncrementalTalentCommit]")
{
    for (int32 spec : {300, 301})
    {
        auto partial = Config(spec);
        CHECK(TraitMgr::ValidateConfigForCommit(partial, nullptr) == TraitMgr::LearnResult::UnspentTalentPoints);
    }
    auto unknown = Config(999);
    CHECK(TraitMgr::ValidateConfigForCommit(unknown, nullptr) != TraitMgr::LearnResult::Ok);
}
