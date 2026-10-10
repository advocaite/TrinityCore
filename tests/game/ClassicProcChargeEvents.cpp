#include "tc_catch2.h"
#include "Creature.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <memory>

namespace
{
    struct NativeSpell
    {
        SpellNameEntry Name{};
        SpellMiscEntry Misc{};
        SpellAuraOptionsEntry Options{};
        SpellEffectEntry Effect{};
        SpellDurationEntry Duration{};
        std::unique_ptr<SpellInfo> Info;

        explicit NativeSpell(uint32 spell, bool native = true)
        {
            // Native70124 Windfury AP children: failure burns charge, three
            // charges for melee swings, and 1500ms duration. Other IDs below
            // are explicitly synthetic controls for the shared charge path.
            Name.ID = Misc.SpellID = Options.SpellID = Effect.SpellID = spell;
            Misc.Attributes[0] = SPELL_ATTR0_PROC_FAILURE_BURNS_CHARGE;
            Misc.Attributes[15] = native ? SPELL_ATTR15_UNK13 : 0;
            Options.ProcChance = 100;
            Options.ProcCharges = 3;
            Options.ProcTypeMask[0] = PROC_FLAG_DEAL_MELEE_SWING;
            Effect.Effect = SPELL_EFFECT_APPLY_AURA;
            Effect.EffectAura = SPELL_AURA_MOD_ATTACK_POWER;
            Effect.EffectBasePoints = 46;
            Effect.ImplicitTarget[0] = TARGET_UNIT_CASTER;
            Duration.ID = 65;
            Duration.Duration = Duration.MaxDuration = 1500;
            SpellInfoLoadHelper load;
            load.Misc = &Misc;
            load.AuraOptions = &Options;
            load.Effects[0] = &Effect;
            Info = std::make_unique<SpellInfo>(&Name, DIFFICULTY_NONE, load);
            Info->DurationEntry = &Duration;
        }
    };

    struct DetachedAura final : Aura
    {
        using Aura::Aura;
        void Remove(AuraRemoveMode mode) override { _Remove(mode); }
        void FillTargetMap(std::unordered_map<Unit*, uint32>&, Unit*) override { }
    };

    struct Fixture
    {
        NativeSpell Spell;
        Creature Owner{false}, Victim{false};
        DetachedAura Instance;
        SpellProcEntry Proc{};

        explicit Fixture(uint32 spell = 8233, bool native = true)
            : Spell(spell, native), Instance(AuraCreateInfo(ObjectGuid::Empty, Spell.Info.get(), DIFFICULTY_NONE, 1, &Owner))
        {
            Instance._InitEffects(1, nullptr, nullptr);
            Proc.ProcFlags = PROC_FLAG_DEAL_MELEE_SWING;
            Proc.Charges = 3;
        }

        void Run(ProcFlagsInit flags, ProcFlagsSpellPhase phase, ProcFlagsHit hit = PROC_HIT_NORMAL)
        {
            DamageInfo damage(&Owner, &Victim, 10, nullptr, SPELL_SCHOOL_MASK_NORMAL, DIRECT_DAMAGE, BASE_ATTACK);
            ProcEventInfo event(&Owner, &Victim, flags, PROC_SPELL_TYPE_DAMAGE, phase, hit, nullptr, &damage, nullptr);
            Instance.PrepareProcChargeDrop(&Proc, event);
            Instance.ConsumeProcCharges(&Proc);
        }
    };
}

TEST_CASE("Native Windfury AP charges survive unrelated spell cast hit and finish events", "[ClassicProcChargeEvents][Forever70124]")
{
    for (uint32 spell : {8233u, 8236u, 10484u, 16361u})
    {
        CAPTURE(spell);
        Fixture f(spell);
        REQUIRE(f.Instance.GetCharges() == 3);
        REQUIRE(f.Instance.GetMaxDuration() == 1500);
        for (auto phase : {PROC_SPELL_PHASE_CAST, PROC_SPELL_PHASE_HIT, PROC_SPELL_PHASE_FINISH})
        {
            f.Run(PROC_FLAG_DEAL_HELPFUL_SPELL, phase);
            CHECK(f.Instance.GetCharges() == 3);
            CHECK_FALSE(f.Instance.IsRemoved());
        }
    }
}

TEST_CASE("Native matched melee attempts still consume every charge including failed hit outcomes", "[ClassicProcChargeEvents][Forever70124]")
{
    for (auto hit : {PROC_HIT_NORMAL, PROC_HIT_CRITICAL, PROC_HIT_MISS, PROC_HIT_DODGE, PROC_HIT_PARRY, PROC_HIT_BLOCK})
    {
        CAPTURE(hit);
        Fixture f;
        f.Run(PROC_FLAG_DEAL_MELEE_SWING, PROC_SPELL_PHASE_HIT, hit);
        CHECK(f.Instance.GetCharges() == 2);
        CHECK_FALSE(f.Instance.IsRemoved());
        f.Run(PROC_FLAG_DEAL_MELEE_SWING, PROC_SPELL_PHASE_HIT, hit);
        CHECK(f.Instance.GetCharges() == 1);
        CHECK_FALSE(f.Instance.IsRemoved());
        f.Run(PROC_FLAG_DEAL_MELEE_SWING, PROC_SPELL_PHASE_HIT, hit);
        CHECK(f.Instance.GetCharges() == 0);
        CHECK(f.Instance.IsRemoved());
    }
}

TEST_CASE("NonClassic charged auras retain the legacy unrelated event behavior", "[ClassicProcChargeEvents]")
{
    Fixture f(8233, false);
    f.Run(PROC_FLAG_DEAL_HELPFUL_SPELL, PROC_SPELL_PHASE_CAST);
    CHECK(f.Instance.GetCharges() == 2);
    CHECK_FALSE(f.Instance.IsRemoved());
}

TEST_CASE("Native charge eligibility follows both native ProcFlags words rather than spell IDs", "[ClassicProcChargeEvents]")
{
    Fixture f(90000001); // synthetic charged aura control
    f.Proc.ProcFlags = ProcFlagsInit(PROC_FLAG_DEAL_HELPFUL_SPELL, PROC_FLAG_2_CAST_SUCCESSFUL);
    f.Run(PROC_FLAG_TAKE_MELEE_SWING, PROC_SPELL_PHASE_HIT);
    CHECK(f.Instance.GetCharges() == 3);
    f.Run(ProcFlagsInit({}, PROC_FLAG_2_CAST_SUCCESSFUL), PROC_SPELL_PHASE_CAST);
    CHECK(f.Instance.GetCharges() == 2);
    f.Run(PROC_FLAG_DEAL_HELPFUL_SPELL, PROC_SPELL_PHASE_CAST);
    CHECK(f.Instance.GetCharges() == 1);
}

TEST_CASE("Stack-based charges and uncharged Classic auras keep their existing handling", "[ClassicProcChargeEvents]")
{
    Fixture stack;
    stack.Proc.AttributesMask = PROC_ATTR_USE_STACKS_FOR_CHARGES;
    DamageInfo damage(&stack.Owner, &stack.Victim, 10, nullptr, SPELL_SCHOOL_MASK_NORMAL, DIRECT_DAMAGE, BASE_ATTACK);
    ProcEventInfo event(&stack.Owner, &stack.Victim, PROC_FLAG_DEAL_MELEE_SWING, PROC_SPELL_TYPE_DAMAGE,
        PROC_SPELL_PHASE_HIT, PROC_HIT_NORMAL, nullptr, &damage, nullptr);
    stack.Instance.PrepareProcChargeDrop(&stack.Proc, event);
    CHECK(stack.Instance.GetCharges() == 3);
    Fixture uncharged;
    uncharged.Instance.SetCharges(0);
    uncharged.Run(PROC_FLAG_DEAL_MELEE_SWING, PROC_SPELL_PHASE_HIT);
    CHECK(uncharged.Instance.GetCharges() == 0);
    CHECK_FALSE(uncharged.Instance.IsRemoved());
}
