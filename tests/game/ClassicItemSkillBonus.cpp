#include "tc_catch2.h"
#include "ClassicItemSkillBonus.h"
#include <array>

namespace
{
    template<class Setter, class T>
    void Set(Setter setter, T value) { UF::SetUpdateFieldValue(setter, std::move(value)); }

    struct Fields
    {
        UF::ActivePlayerData Data;
        SkillStatusMap Status;
        void Add(uint32 id, uint16 rank, int16 temporary, uint16 permanent = 0)
        {
            uint32 pos = uint32(Status.size());
            Status.emplace(id, SkillStatusData(pos, SKILL_UNCHANGED));
            auto fields = UF::MutableFieldReference<UF::ActivePlayerData, false>(Data).ModifyValue(&UF::ActivePlayerData::Skill);
            Set(fields.ModifyValue(&UF::SkillInfo::SkillLineID, pos), uint16(id));
            Set(fields.ModifyValue(&UF::SkillInfo::SkillRank, pos), rank);
            Set(fields.ModifyValue(&UF::SkillInfo::SkillMaxRank, pos), uint16(300));
            Set(fields.ModifyValue(&UF::SkillInfo::SkillTempBonus, pos), temporary);
            Set(fields.ModifyValue(&UF::SkillInfo::SkillPermBonus, pos), permanent);
        }
        bool Restore(uint32 id, int32 bonus)
        {
            return ClassicItemSkillBonus::Restore(id, bonus, Status, Data,
                UF::MutableFieldReference<UF::ActivePlayerData, false>(Data));
        }
    };
}

TEST_CASE("Equipped Classic skill bonuses restore only the newly activated skill fields", "[ClassicItemSkillBonus]")
{
    Fields fields;
    CHECK_FALSE(fields.Restore(SKILL_HERBALISM, 5));
    fields.Add(SKILL_HERBALISM, 25, 9, 7);
    fields.Add(SKILL_MINING, 50, 3, 2);
    REQUIRE(fields.Restore(SKILL_HERBALISM, 5));
    CHECK(fields.Data.Skill->SkillTempBonus[0] == 14);
    CHECK(fields.Data.Skill->SkillRank[0] == 25);
    CHECK(fields.Data.Skill->SkillMaxRank[0] == 300);
    CHECK(fields.Data.Skill->SkillPermBonus[0] == 7);
    CHECK(fields.Data.Skill->SkillTempBonus[1] == 3);
    CHECK(fields.Status.at(SKILL_HERBALISM).uState == SKILL_UNCHANGED);
}

TEST_CASE("Classic item restoration preserves deleted zero-rank and zero-bonus skills", "[ClassicItemSkillBonus]")
{
    Fields fields;
    fields.Add(SKILL_HERBALISM, 25, 9);
    CHECK_FALSE(fields.Restore(SKILL_HERBALISM, 0));
    fields.Status.at(SKILL_HERBALISM).uState = SKILL_DELETED;
    CHECK_FALSE(fields.Restore(SKILL_HERBALISM, 5));
    fields.Status.at(SKILL_HERBALISM).uState = SKILL_UNCHANGED;
    auto fieldsRef = UF::MutableFieldReference<UF::ActivePlayerData, false>(fields.Data).ModifyValue(&UF::ActivePlayerData::Skill);
    Set(fieldsRef.ModifyValue(&UF::SkillInfo::SkillRank, 0), uint16(0));
    CHECK_FALSE(fields.Restore(SKILL_HERBALISM, 5));
    CHECK(fields.Data.Skill->SkillTempBonus[0] == 9);
}

TEST_CASE("Stored item skill bonus includes ancestors once and survives equip-before-learning", "[ClassicItemSkillBonus]")
{
    std::array<uint32, 3> skills{SKILL_HERBALISM, SKILL_MINING, 10001};
    std::array<int32, 3> amounts{5, 7, 2};
    auto parent = [](uint32 skill) { return skill == 10001 ? uint32(SKILL_HERBALISM) : 0u; };
    CHECK(ClassicItemSkillBonus::StoredBonus(SKILL_HERBALISM, skills, amounts, parent) == 5);
    CHECK(ClassicItemSkillBonus::StoredBonus(10001, skills, amounts, parent) == 7);
    CHECK(ClassicItemSkillBonus::StoredBonus(SKILL_FISHING, skills, amounts, parent) == 0);
    amounts[0] = 0; // removed while the profession was unlearned
    CHECK(ClassicItemSkillBonus::StoredBonus(SKILL_HERBALISM, skills, amounts, parent) == 0);
    CHECK(ClassicItemSkillBonus::StoredBonus(10001, skills, amounts, parent) == 2);
    CHECK(ClassicItemSkillBonus::StoredBonus(0, skills, amounts, parent) == 0);
    auto cycle = [](uint32 skill) { return skill == SKILL_MINING ? uint32(SKILL_HERBALISM) : uint32(SKILL_MINING); };
    CHECK(ClassicItemSkillBonus::StoredBonus(SKILL_HERBALISM, skills, amounts, cycle) == 7);
}
