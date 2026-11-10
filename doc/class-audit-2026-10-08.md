# Class audit, 2026-10-08 (ranks, spells, overlap, sequence)

Read-only code audit of the nine Classic rotations plus ClassicUpkeep, MarkCc and the heal helpers. Nothing here was run: the cloud cannot compile and the PC build/test is the check. CERTAIN = verified by reading the code, UNSURE = depends on spell data or game rules.

## Root cause behind the rank 1 casts

The 70205 spell data has only ~605 rank chains (SkillLineAbility.SupercedesSpell is empty for most class spells). Healing Touch, Regrowth, Holy Light, Flash of Light, Greater Heal, Flash Heal, Frostbolt, Fireball, Shadow Bolt, Banish, the curses and more have no chain. `GetFirstSpellInChain` then returns the id itself, so the rank-1 ids in the rotations were cast, tracked and compared as rank 1.

Fix: `Bot/SpellFamily.h` indexes every spell by name + spell family + first effect (effect type, aura type), ascending by base level. A real chain wins where there is one. Used by: `known_ranks`/`resolve_rank`, `find_aura`, `find_pet_aura`, `weapon_enchant_from`, `totem_from`, API `HighestKnownRank`, `IsRezSpell`, curse root checks, CC `being_ccd`, taunt detection, paladin aura held, tracked raid buffs, Demoralizing Shout/Roar, HoT claim keys, heal downranking. Core spell chains were deliberately not changed (human players share them).

## Fixed in this pass

- Highest known rank for chainless spells (casts, heal sizing, aura "already up" checks: Immolate/Corruption/Curses/Serpent Sting/Slice and Dice/Renew/imbues/totems no longer recast every GCD).
- Double heals: the tank's own healers count each other's heals in flight unless the tank is under 50%; no heal picks on members at 95%+ unless assigned to that tank.
- HoT claim 8 s -> 11 s (Rejuvenation 12 s, Renew 15 s).
- Hunter's Mark, Sunder Armor stacks, Thunder Clap and Faerie Fire are checked against any caster, not only the bot's own aura.
- Slice and Dice is recast under 3 s left; Cold Blood needs 35 energy; Whirlwind waits for Bloodthirst below 55 rage.
- Fire mages in a boss fight keep Fire Vulnerability at 5 stacks with Scorch.
- Curse of Recklessness is assigned before Shadow on a boss; Curse of Agony capped at two warlocks.
- Shadow priests no longer try Holy spells (Power Word: Shield, Inner Fire, Fear Ward, Desperate Prayer) in Shadowform; healer priests rank Fear Ward and Inner Fire above Renew.
- Healer paladins only seal within judgement reach.

## Optimisation pass (2026-10-08 22:xx, on top of 98cf2624)

Done (vanilla rule is clear):

- Judgements: in a raid the bot paladins that are not tanks, ordered by guid, seal Wisdom, Light, Crusader (melee only; a healer in the third slot keeps its damage seal). The existing loop judges as soon as Judgement is ready (cooldown 10 s = debuff length), so each debuff stays up. Seals Wisdom 20166 / Crusader 21082 count as a seal now. Outside raids the damage seal (Command / Righteousness) is unchanged.
- Expose Armor (8647): rogue, 5 combo points, boss only, only when no Sunder Armor with 3+ stacks and no Expose with 5 s left is on the target (any caster), and only the lowest-guid rogue casts it.
- Multi-Shot counts enemies around the victim (it counted around the hunter, so it fired at a lone target 30 yards away that stood near the hunter's pet or never fired in a pack).
- Fire Blast: only while moving, or as a filler for a fire build above 50% mana; frost/arcane stand still and cast the main spell.
- Enhancement off-hand imbue: Flametongue (Rockbiter before it is learned) on a dual-wielded off-hand weapon; main hand stays Windfury.
- Balance in Moonkin Form casts Wrath, Starfire, Moonfire, Insect Swarm and Faerie Fire (BalanceReady blocks only Cat/Bear/Aquatic/Travel). Heals and dispels still leave the form first. If the 70205 data refuses them in the form, the [cast_fail] log shows NOT_SHAPESHIFT for classic_balance rules; the old behaviour was a Moonkin that never cast.
- Shadowburn keeps at least two shards back.
- [cast_rank] already logs, per bot and spell once a minute, the requested and the cast id (rank) for every bot cast, so DPS ranks are visible: `grep "\[cast_rank\]"`.

Held back (the vanilla rule is unsure, or the view lacks the data):

- Aimed Shot clipping Auto Shot: needs the Auto Shot swing timer, which the snapshot does not carry; casting only when not moving is what exists. Not guessed.
- Fury priority queue (Bloodthirst > Whirlwind > Heroic Strike queue above ~50 rage, Execute under 20%): Bloodthirst/Whirlwind order is already in place from the last pass; Heroic Strike queue threshold left alone, the exact threshold depends on weapon speed.
- Rogue energy pooling and Slice and Dice refresh timing beyond the 3 s recast: left as is.
- Warlock Life Tap threshold (mana under 25%, health over 60%): reasonable in a raid with healers; not changed.
- Mage Ice Barrier, Fade on threat, Concussion Blow interrupt plan, Holy Shock range, Prayer of Healing / Power Infusion claims, Flametongue Totem, Shadow Protection only when shadow damage is expected, Greater Blessing order for hunters/ret/feral, BlessingAt duplicates with 4+ paladins: still open, each needs a design decision or data the snapshot does not carry.

## Open (not done)

- Balance druid in Moonkin Form never casts (CasterReady fails in any form): UNSURE whether vanilla Moonkin may cast Balance spells in this data.
- Judgement of Wisdom/Light/Crusader: paladins only seal Command/Righteousness; plan one paladin per judgement.
- Holy Shock picks a patient at 40 yd but reaches 20 yd.
- Prayer of Healing, Holy Shock, Swift Healing Wave, Nature's Swiftness Healing Touch and Swiftmend bypass the in-flight claims.
- Power Infusion has no cross-priest claim.
- `BlessingAt` falls forward past unlearned blessings, so 4+ paladins can duplicate one.
- Greater Blessings for Ret/Enhancement/Feral use the caster order (Wisdom); hunters get Wisdom.
- Shadow Protection is cast in every raid.
- Cure Poison/Disease overlaps with a cleansing totem; shaman/paladin party index shifts when one dies.
- Enhancement off-hand imbue missing; Flametongue Totem never chosen.
- Rogue: no Expose Armor logic; hunter Multi-Shot tests enemies around the bot, not the victim; Aimed Shot clips Auto Shot (no Auto Shot timer in the view); Shadowburn has no shard reserve; Life Tap at 60% hp in a raid.
- Fade fires on own hp with any attacker, not on threat; Fire Blast on cooldown for every mage spec; Ice Barrier has no gate; Concussion Blow ignores the interrupt plan.
- Curse of the Elements (1490) and Curse of Shadow (17862) do not exist in the 70205 client spell data (no name, misc, effect or level rows); only Recklessness (704) does. PlanCurses already skips a curse nobody knows, so Recklessness (bosses) and Agony are what the warlocks get. For the advocaite list: Elements/Shadow missing from client data.
