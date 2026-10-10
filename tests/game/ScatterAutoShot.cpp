/* This file is part of TrinityCore; licensed under GPL version 2 or later. */
#include "tc_catch2.h"
#include "Creature.h"
#include "DummyData.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <memory>

namespace
{
    struct Record
    {
        SpellNameEntry Name{};
        SpellMiscEntry Misc{};
        SpellClassOptionsEntry Family{};
        std::unique_ptr<SpellInfo> Info;
        Record(uint32 id, bool native = true, bool stopAttack = true, uint32 family = SPELLFAMILY_HUNTER)
        {
            Name.ID = Misc.SpellID = Family.SpellID = id;
            Misc.Attributes[0] = stopAttack ? uint32(SPELL_ATTR0_CANCELS_AUTO_ATTACK_COMBAT) : 0;
            Misc.Attributes[15] = native ? uint32(SPELL_ATTR15_UNK13) : 0;
            Family.SpellClassSet = family;
            SpellInfoLoadHelper load;
            load.Misc = &Misc;
            load.ClassOptions = &Family;
            Info = std::make_unique<SpellInfo>(&Name, DIFFICULTY_NONE, load);
        }
    };

    void Finish(SpellCastResult result, bool shouldCancel, uint32 spellId = 19503,
        bool native = true, bool stopAttack = true, uint32 family = SPELLFAMILY_HUNTER,
        uint8 playerClass = CLASS_HUNTER, uint32 autoId = 75)
    {
        UnitTestDataLoader::MapFixture map;
        Creature owner(false);
        owner.SetClass(playerClass);
        owner.SetMap(&map.Get());
        Record autoInfo(autoId, native, false);
        Record scatterInfo(spellId, native, stopAttack, family);
        {
            Spell autoShot(&owner, autoInfo.Info.get(), TRIGGERED_NONE);
            autoShot.SetAutoRepeat(true);
            autoShot.setState(SPELL_STATE_IDLE);
            owner.SetCurrentCastSpell(&autoShot);
            REQUIRE(owner.GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL) == &autoShot);
            Spell scatter(&owner, scatterInfo.Info.get(), TRIGGERED_NONE);
            scatter.finish(result);
            CHECK((owner.GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL) == nullptr) == shouldCancel);
            owner.CancelAutoRepeatSpell();
        }
        owner.ResetMap();
    }
}

TEST_CASE("Native Scatter Shot cancels the real Auto Shot at successful finish", "[ScatterAutoShot]")
{
    Finish(SPELL_CAST_OK, true);
}

TEST_CASE("Scatter cancellation preserves failed casts and unrelated spell contexts", "[ScatterAutoShot]")
{
    SECTION("failed Scatter") { Finish(SPELL_FAILED_INTERRUPTED, false); }
    SECTION("other Hunter spell") { Finish(SPELL_CAST_OK, false, 19434); }
    SECTION("retail identity") { Finish(SPELL_CAST_OK, false, 19503, false); }
    SECTION("wrong family") { Finish(SPELL_CAST_OK, false, 19503, true, true, SPELLFAMILY_MAGE); }
    SECTION("wrong class") { Finish(SPELL_CAST_OK, false, 19503, true, true, SPELLFAMILY_HUNTER, CLASS_MAGE); }
    SECTION("no stop-attack attribute") { Finish(SPELL_CAST_OK, false, 19503, true, false); }
    SECTION("other autorepeat") { Finish(SPELL_CAST_OK, false, 19503, true, true, SPELLFAMILY_HUNTER, CLASS_HUNTER, 5019); }
}
