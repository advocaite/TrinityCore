#include "ClassicTalentBuilds.h"
#include "SharedDefines.h"
#include <array>

namespace Playerbot::Classic {

namespace {

// 1.12 talent names. Each list spends 51 points; ranks are bought in order,
// so early entries are what a levelling bot gets first. A rank that the tree
// refuses (tier not reached yet) is skipped and retried on the next apply.

// ---- Warrior ----
constexpr TalentPick kWarriorArms[] = {
    // A talent below another needs a parent node filled to its maximum, so the first rows are bought in full (Deflection 5, Improved
    // Tactical Mastery 5, Improved Rend 3, Improved Thunder Clap 3): 143 stopped Deflection at 4 and Tactical Mastery at 3 and never
    // got Deep Wounds, Anger Management or Impale.
    // 0.1.30 (client tree of 70755): Deflection (parry) and Improved Thunder Clap only filled rows; Improved Heroic Strike and Improved Rend
    // already open the second row, so their 8 points (and Improved Hamstring's 1) go to the Fury tab: Unbridled Wrath 5 makes 10 there, which
    // opens Furious Precision (+hit, row 3), then Booming Voice and Improved Execute. Mortal Strike still has its 30 Arms points.
    // Client 70790: every tier gate of the warrior tree counts the points spent in the whole tree (one trait currency, 3820, for all
    // three tabs), so a row is open by the points spent anywhere. Scorecard 0.1.30 (5 Nov, 12 measurements): arms 708-834 against fury's
    // 1076-1260, with 5 points in Booming Voice (shout area and cost only) and 2 in Improved Charge (no Charge in a boss fight), and 2
    // in Improved Thunder Clap from the fillers. Those 9 points, and the 4 that the 70790 tree took from Improved Tactical Mastery and
    // Two-Handed Weapon Specialization (now 3 ranks each), buy the fury damage talents that need no fury rotation: Death Wish (+20%
    // physical damage for 30 s, cast in boss fights by classic_war:death_wish), Flurry (attack speed after a crit; arms crits 40%+) and
    // Improved Whirlwind (a shorter Whirlwind cooldown, and arms fights in Berserker Stance in boss fights).
    { "Improved Heroic Strike", 3 }, { "Improved Rend", 3 }, { "Improved Tactical Mastery", 3 },
    { "Anger Management", 1 }, { "Deep Wounds", 3 },
    { "Two-Handed Weapon Specialization", 3 }, { "Impale", 2 }, { "Sweeping Strikes", 1 },
    { "Weaponmaster", 5 }, { "Mortal Strike", 1 },
    { "Cruelty", 5 }, { "Unbridled Wrath", 5 }, { "Furious Precision", 3 }, { "Improved Execute", 2 },
    { "Death Wish", 1 }, { "Flurry", 3 }, { "Improved Whirlwind", 2 },
    // Fillers, bought only with points left over (Forever's trees have no Tactical Mastery, Sword Specialization or Improved Battle Shout,
    // and the 70532 tree no Axe Specialization: "build talents not found" on every arms walk of 235).
    { "Improved Overpower", 2, true }, { "Improved Charge", 2, true }, { "Booming Voice", 5, true },
};
constexpr TalentPick kWarriorFury[] = {
    // The vanilla raid fury build, 17 Arms / 34 Fury (51 points). The Arms core goes first: the last build spent 46 points in Fury before it
    // reached them and Deep Wounds was refused (damage_parity 146). The 147 build (Deflection, Tactical Mastery, Charge first) left Deep Wounds refused with
    // LearnResult 2, so the parents it was bought under in the 71 build go first (Rend, Thunder Clap, Overpower, Anger Management). Impale needs more
    // Arms points than 17 allow.
    // 156: Deep Wounds was bought, Anger Management refused with nothing held (Improved Tactical Mastery, its parent, was not in the list and the
    // 17 points had no room for the parent fill). Rend and Thunder Clap are what Deep Wounds was bought under; Tactical Mastery opens Anger Management.
    { "Improved Rend", 3 }, { "Improved Thunder Clap", 3 }, { "Improved Tactical Mastery", 5 }, { "Anger Management", 1 }, { "Deep Wounds", 3 },
    { "Improved Heroic Strike", 2 },
    // Bloodthirst needs 30 points in the Fury tab (Flurry's row needs 20 before it).
    { "Cruelty", 5 }, { "Unbridled Wrath", 5 }, { "Dual Wield Specialization", 5 }, { "Enrage", 5 }, { "Improved Execute", 2 },
    { "Improved Berserker Rage", 2 }, { "Death Wish", 1 }, { "Flurry", 5 }, { "Bloodthirst", 1 },
    { "Booming Voice", 3 },
    { "Blood Craze", 3, true },   // filler, see Arms
};
constexpr TalentPick kWarriorProtection[] = {
    // Forever's Protection tree has no Toughness, Improved Shield Block, Improved Taunt or One-Handed Weapon Specialization (readiness audit
    // group 4); it has Iron Will, Improved Shield Bash, Improved Shield Wall, Master of Defense, Vanguard and Bastion in their place.
    // A rank above a node's maximum is clamped by the walk.
    { "Shield Specialization", 5 }, { "Anticipation", 5 }, { "Iron Will", 5 }, { "Last Stand", 1 },
    { "Improved Shield Wall", 2 }, { "Defiance", 5 }, { "Improved Sunder Armor", 3 }, { "Improved Shield Bash", 2 },
    { "Concussion Blow", 1 }, { "Master of Defense", 5 }, { "Shield Slam", 1 },
    { "Improved Revenge", 2 }, { "Vanguard", 3 }, { "Bastion", 3 },
    { "Improved Heroic Strike", 3 }, { "Deflection", 2 }, { "Improved Tactical Mastery", 3 },
    { "Cruelty", 5 },
    { "Improved Bloodrage", 2, true }, { "Booming Voice", 5, true },   // fillers, see Arms
};

// ---- Paladin ----
constexpr TalentPick kPaladinHoly[] = {
    // Forever's Holy nodes (talent coverage dump): Consecration and Blessing of Kings are spells there, and there is no Improved Lay on Hands,
    // Improved Blessing of Wisdom or Improved Concentration Aura node under those names. Sanctified Light and Divinity are spells, not nodes
    // (the client's node list of class 2 has neither), and Illumination hangs under Reverence (node 110871, 3 ranks): without it Illumination
    // was refused and the holy paladin of the support scorecard (230) healed with no Illumination at all. Light's Vigil (row y5730) needs 30
    // points in the Holy tab, and Holy Conduit (node 105704) is a Retribution node: bought before it, the tab held 29 and Light's Vigil was
    // refused (233, LearnResult 2). Voice of Truth first makes the 30; Improved Hammer of Justice at 1 keeps the list at 51.
    // Forever launch data (hotfixes 2026_11_03_41): Voice of Truth's node (definition 134825) is Oath of Light now, and Reverence stops at 2.
    { "Divine Intellect", 5 }, { "Spiritual Focus", 2 }, { "Healing Light", 3 }, { "Reverence", 3 }, { "Divine Favor", 1 },
    { "Holy Power", 5 }, { "Holy Shock", 1 }, { "Illumination", 5 }, { "Purifying Power", 2 }, { "Infusion of Light", 2 },
    { "Oath of Light|Voice of Truth", 1 }, { "Light's Vigil", 1 }, { "Holy Conduit", 2 },
    { "Redoubt", 5 }, { "Guardian's Favor", 2 }, { "Toughness", 5 }, { "Anticipation", 5 }, { "Improved Hammer of Justice", 1 },
};
constexpr TalentPick kPaladinProtection[] = {
    // Forever's Protection tab (client node list of class 2, 168 log) has no Blessing of Sanctuary, Improved Seal of Righteousness or
    // Improved Lay on Hands, and Precision, Shield Specialization and One-Handed Weapon Specialization stop at 3 ranks. Shield
    // Specialization hangs under Redoubt, Holy Shield (row y5730) under Templar's Bulwark (y4530); tier order, parents first.
    { "Redoubt", 5 }, { "Toughness", 5 }, { "Precision", 3 }, { "Guardian's Favor", 2 }, { "Anticipation", 5 },
    { "Improved Righteous Fury", 3 }, { "Shield Specialization", 3 }, { "Sacred Duty", 2 },
    { "Improved Hammer of Justice", 3 }, { "One-Handed Weapon Specialization", 3 },
    { "Templar's Bulwark", 1 }, { "Reckoning", 5 }, { "Holy Shield", 1 },
    { "Divine Strength", 5 }, { "Divine Intellect", 5 }, { "Unyielding Faith", 2 },
};
constexpr TalentPick kPaladinRetribution[] = {
    // Forever's Retribution tab (class tree dump, 168 log): no Crusade, Sanctity Aura, Improved Blessing of Might or Improved Seal of the
    // Crusader. Vengeance (row y4530) hangs under Sanctified Judgement (node 105701, 3 ranks), which is why it was refused with the tab
    // full of other points. Tier order, parents first; the leftover pass spends the rest down the Retribution tab.
    { "Improved Judgement", 2 }, { "Conviction", 5 }, { "Benediction", 5 }, { "Deflection", 5 },
    { "Vindication", 3 }, { "Sanctified Judgement", 3 }, { "Seal of Command", 1 }, { "Pursuit of Justice", 2 },
    { "Eye for an Eye", 2 }, { "Sacred Arbiter", 1 }, { "Vengeance", 3 }, { "Two-Handed Weapon Specialization", 3 },
    // Forever launch data: Improved Seal of Fury's node (definition 142627) is Shieldsworn, and Twist of Light (spell 1310735) is Unsealed Wrath.
    { "Repentance", 1 }, { "Shieldsworn|Improved Seal of Fury", 1 }, { "Swift Judgement", 1 }, { "Instrument of Law", 2 },
    { "Unsealed Wrath|Twist of Light", 1 },
};

// ---- Hunter ----
constexpr TalentPick kHunterBeastMastery[] = {
    // Forever names (talent coverage dump of the client data): Deadly Aspects is the Improved Aspect of the Hawk, Lethal Attacks the Lethal Shots;
    // Aimed Shot is a trainer spell there, not a talent.
    // Bestial Wrath (node 104961, row 7) needs 30 points in the tab and Intimidation, which hangs under Bestial Swiftness (client tree of
    // 70755). 0.1.29 spent 28 there and never got it (scorecard: "refused Bestial Wrath has parents: node 104964", "Bestial Wrath 0/1"):
    // Bestial Swiftness and Frenzy 5 make the 30; the 3 points come off Efficiency.
    { "Deadly Aspects", 5 }, { "Endurance Training", 5 }, { "Unleashed Fury", 5 }, { "Ferocity", 5 }, { "Bestial Swiftness", 1 },
    { "Intimidation", 1 }, { "Bestial Discipline", 2 }, { "Spirit Bond", 1 }, { "Frenzy", 5 },
    { "Bestial Wrath", 1 },
    // Mortal Shots (node 105002) hangs under Careful Aim (node 105008) in full: without it Mortal Shots was refused in every scorecard
    // (236: "refused Mortal Shots has parents: node 105008 edge type 2 0/5"), and its row needs 15 points in Marksmanship.
    { "Lethal Attacks", 5 }, { "Careful Aim", 5 },
    { "Improved Arcane Shot", 5 }, { "Mortal Shots", 5 },
};
constexpr TalentPick kHunterMarksmanship[] = {
    // Names from the talent dump of the client data (147/156 logs): Forever's Marksmanship has no Lethal Shots, Aimed Shot (a trainer
    // spell there), Improved Hunter's Mark, Monster / Humanoid Slaying or Trap Mastery. Mortal Shots was refused with the tab full of
    // other points, so the rows that open its tier are bought before it, then the leftover pass spends what remains in the tab.
    // 0.1.30 (client tree of 70755): Lethal Attacks (+5% crit) and Lone Wolf (row 3, +20% damage with no pet out; the bot sends the pet
    // away in a raid) replace Improved Concussive Shot, Scatter Shot and Hawk Eye, which add no damage; Deadly Aspects goes to 5 (Quick Shots).
    { "Lethal Attacks", 5 }, { "Efficiency", 5 }, { "Careful Aim", 5 }, { "Improved Arcane Shot", 5 }, { "Lone Wolf", 1 },
    { "Improved Stings", 3 }, { "Rapid Killing", 2 }, { "Mortal Shots", 5 }, { "Trueshot Aura", 1 }, { "Improved Serpent Sting", 5 },
    { "Barrage", 3 }, { "Ranged Weapon Specialization", 5 }, { "Deadly Aspects", 5 }, { "Hawk Eye", 1 },
};
constexpr TalentPick kHunterSurvival[] = {
    // Forever's Survival nodes (client tree of 70755): no Monster / Humanoid Slaying, Trap Mastery or Killer Instinct, and Wyvern Sting is a
    // spell. Lightning Reflexes (row 6, +15% agility) is there now and needs 25 points in the tab: the rows before it take the talents a bot
    // that also levels and fights in melee uses. The rest goes to Marksmanship: Lethal Attacks, Careful Aim, Improved Arcane Shot, Lone Wolf
    // (+20% damage with no pet out; the bot sends the pet away in a raid) and Mortal Shots. Entrapment, Clever Traps and Improved Wing Clip
    // (traps and snares) are left out; 0.1.29 survival was the lowest hunter (547-616 dps).
    { "Deflection", 5 }, { "Survivalist", 5 }, { "Savage Strikes", 2 }, { "Surefooted", 3 }, { "Deterrence", 1 },
    { "Survival Tactics", 2 }, { "Counterattack", 1 }, { "Predator's Edge", 5 }, { "Resourcefulness", 1 }, { "Lightning Reflexes", 5 },
    { "Lethal Attacks", 5 }, { "Careful Aim", 5 },   // Mortal Shots needs Careful Aim in full, see Beast Mastery
    { "Improved Arcane Shot", 5 }, { "Lone Wolf", 1 }, { "Mortal Shots", 5 },
};

// ---- Rogue ----
// Client 70790 renamed Improved Sinister Strike (13732) to Efficient Strikes and Improved Eviscerate (14162) to Coup de Grace.
constexpr TalentPick kRogueAssassination[] = {
    { "Malice", 5 }, { "Murder", 2 }, { "Ruthlessness", 3 }, { "Improved Slice and Dice", 3 },
    { "Relentless Strikes", 1 }, { "Lethality", 5 }, { "Vile Poisons", 1 }, { "Cold Blood", 1 },
    { "Improved Poisons", 4 }, { "Seal Fate", 5 }, { "Vigor", 1 },
    { "Improved Sinister Strike|Efficient Strikes", 2 }, { "Lightning Reflexes", 3 }, { "Precision", 5 }, { "Endurance", 2 },
    { "Improved Eviscerate|Coup de Grace", 1 }, { "Puncturing Wounds", 3 }, { "Dual Wield Specialization", 5 },
};
constexpr TalentPick kRogueCombat[] = {
    { "Improved Sinister Strike|Efficient Strikes", 2 }, { "Lightning Reflexes", 3 }, { "Precision", 5 }, { "Deflection", 5 }, { "Riposte", 1 },
    { "Endurance", 2 }, { "Improved Sprint", 2 }, { "Dual Wield Specialization", 5 }, { "Blade Flurry", 1 },
    { "Hack and Slash", 5 }, { "Weapon Expertise", 2 }, { "Aggression", 2 }, { "Adrenaline Rush", 1 },
    { "Malice", 5 }, { "Murder", 2 }, { "Ruthlessness", 3 }, { "Improved Slice and Dice", 3 },
    { "Relentless Strikes", 1 }, { "Lethality", 1 },
    { "Opportunity", 5 },
};
constexpr TalentPick kRogueSubtlety[] = {
    { "Opportunity", 5 }, { "Master of Deception", 5 }, { "Camouflage", 3 }, { "Initiative", 3 },
    { "Improved Ambush", 3 }, { "Ghostly Strike", 1 }, { "Serrated Blades", 3 }, { "Hemorrhage", 1 },
    { "Preparation", 1 }, { "Dirty Deeds", 2 }, { "Setup", 1 }, { "Heightened Senses", 2 }, { "Premeditation", 1 },
    { "Improved Sinister Strike|Efficient Strikes", 2 }, { "Lightning Reflexes", 3 }, { "Precision", 5 }, { "Endurance", 2 },
    { "Improved Eviscerate|Coup de Grace", 1 }, { "Puncturing Wounds", 3 }, { "Dual Wield Specialization", 5 },
};

// ---- Priest ----
constexpr TalentPick kPriestDiscipline[] = {
    // Forever's Discipline tab (client node list of class 5, support scorecard 230) has no Divine Spirit or Force of Will; it has Twin Disciplines,
    // Soul Warding, Renewed Hope, Penance and Divine Aegis, the shield and crit-heal talents a discipline healer is built round.
    { "Twin Disciplines", 5 }, { "Improved Power Word: Shield", 3 }, { "Silent Resolve", 3 }, { "Martyrdom", 1 },
    { "Inner Focus", 1 }, { "Meditation", 3 }, { "Mental Agility", 3 }, { "Soul Warding", 1 }, { "Mental Strength", 5 },
    { "Renewed Hope", 5 }, { "Penance", 1 }, { "Divine Aegis", 3 }, { "Power Infusion", 1 },
    // 51 points: with Martyrdom at 2 the list held 52 and Inspiration, last, stopped at 2 of 3 (support scorecard 233).
    { "Improved Renew", 3 }, { "Holy Specialization", 5 }, { "Divine Fury", 5 }, { "Inspiration", 3 },
};
constexpr TalentPick kPriestHoly[] = {
    { "Improved Renew", 3 }, { "Holy Specialization", 5 }, { "Divine Fury", 5 }, { "Inspiration", 3 },
    { "Holy Nova", 1 }, { "Litany of Light", 2 }, { "Improved Healing", 3 }, { "Holy Reach", 2 },
    { "Spirit of Redemption", 1 }, { "Spiritual Guidance", 5 }, { "Spiritual Healing", 5 },
    { "Silent Resolve", 3 }, { "Improved Power Word: Shield", 3 },
    { "Meditation", 3 }, { "Inner Focus", 1 }, { "Martyrdom", 2 }, { "Mental Agility", 3 },   // no Divine Spirit node in Forever
};
constexpr TalentPick kPriestShadow[] = {
    { "Spirit Tap", 5 }, { "Improved Shadow Word: Pain", 2 }, { "Shadow Focus", 5 }, { "Mind Flay", 1 },
    { "Improved Mind Blast", 5 }, { "Shadow Weaving", 5 }, { "Vampiric Embrace", 1 }, { "Silence", 1 },
    { "Shadow Reach", 3 }, { "Darkness", 5 }, { "Shadowform", 1 },
    { "Wand Specialization", 5 }, { "Improved Power Word: Shield", 3 },
    { "Inner Focus", 1 }, { "Meditation", 3 }, { "Martyrdom", 2 }, { "Mental Agility", 5 },
};

// ---- Shaman ----
constexpr TalentPick kShamanElemental[] = {
    // Forever's Elemental tab (193 log: Improved Fire Totems, Storm Reach, Lightning Mastery, Elemental Mastery, Nature's Guidance, Totemic Mastery and
    // Healing Grace are not in the client data, so the bot had no cast-time or crit talents and Call of Thunder / Elemental Fury were refused for
    // their parents). These are the client's names; a rank above a node's maximum is clamped. The 70532 client data has no Call of Thunder or
    // Improved Fire Nova (236: "build talents not found: Improved Fire Nova", Call of Thunder not held, 6 points spent outside the tab); it has
    // Elemental Mastery (row y5730, under Lava Burst) and Trial by Fire (node 104764: the Fire Nova talent, spell 16086) in their place.
    { "Convection", 5 }, { "Concussion", 5 }, { "Call of Flame", 3 }, { "Elemental Devastation", 3 }, { "Reverberation", 5 },
    { "Elemental Warding", 3 }, { "Elemental Focus", 1 }, { "Elemental Alacrity", 3 }, { "Eye of the Storm", 3 },
    { "Elemental Reach", 2 }, { "Lightning Overload", 3 }, { "Elemental Fury", 5 }, { "Lava Burst", 1 },
    { "Call of Thunder|Elemental Mastery", 1 }, { "Improved Fire Nova|Trial by Fire", 2 },
    { "Earth's Grasp", 2 }, { "Earthbound", 1 },
};
constexpr TalentPick kShamanEnhancement[] = {
    // 147 bought 19 Enhancement points: Forever's Enhancement tab has no Two-Handed Axes and Maces, Improved Weapon Totems or Weapon Mastery,
    // and the Tidal Focus / Totemic Focus / Restorative Totems picks that followed took 15 points in Restoration, so Anticipation stopped at 3
    // and Flurry was refused. These are names from the client data; a rank above a node's maximum is clamped.
    { "Ancestral Knowledge", 5 }, { "Thundering Strikes", 5 }, { "Anticipation", 5 }, { "Toughness", 5 }, { "Elemental Weapons", 3 },
    { "Flurry", 5 }, { "Spirit Weapons", 1 }, { "Shamanistic Focus", 1 }, { "Stormstrike", 1 }, { "Improved Stormstrike", 5 },
    { "Mental Dexterity", 5 }, { "Mental Quickness", 5 }, { "Guardian Totems", 2 }, { "Improved Lightning Shield", 3 }, { "Improved Ghost Wolf", 2 },
};
constexpr TalentPick kShamanRestoration[] = {
    // Forever's Restoration tab (client node list of class 7, support scorecard 230) has no Nature's Guidance, Totemic Mastery or Healing Grace.
    // Natural Grace is its spell-threat talent (Healing Grace's job), and it has Water Shield and Riptide.
    { "Improved Healing Wave", 5 }, { "Tidal Focus", 5 }, { "Natural Grace", 3 }, { "Water Shield", 1 },
    { "Healing Focus", 5 }, { "Tidal Mastery", 5 }, { "Nature's Swiftness", 1 }, { "Healing Way", 3 },
    { "Restorative Totems", 1 }, { "Purification", 5 }, { "Mana Tide Totem", 1 }, { "Riptide", 1 },
    { "Restorative Totems", 5 }, { "Ancestral Healing", 3 }, { "Totemic Focus", 5 },
    { "Healing Focus", 5 }, { "Improved Reincarnation", 2 },
};

// ---- Mage ----
// Forever launch data (hotfixes 2026_10_30_21 and 2026_11_03_41): Elemental Precision is Precision (3 ranks), Improved Fireball is
// Accelerant, Critical Mass stops at 3 and Incineration goes to 5, and Arcane Resilience and Missile Barrage swapped nodes (Missile
// Barrage takes the Arcane point Arcane Resilience had, so the tab keeps its shape).
constexpr TalentPick kMageArcane[] = {
    { "Arcane Subtlety", 2 }, { "Arcane Focus", 3 }, { "Arcane Concentration", 5 },
    { "Arcane Impact", 3 }, { "Missile Barrage", 1 }, { "Magic Absorption", 1 },
    { "Arcane Meditation", 3 }, { "Improved Channeling", 2 }, { "Presence of Mind", 1 },
    { "Arcane Mind", 5 }, { "Arcane Instability", 3 }, { "Arcane Power", 1 },
    { "Improved Frostbolt", 5 }, { "Ice Shards", 5 }, { "Piercing Ice", 3 }, { "Frostbite", 2 },
    { "Frost Channeling", 3 }, { "Precision|Elemental Precision", 2 },
};
constexpr TalentPick kMageFire[] = {
    // Every Fire talent of the client list at its full rank (parity 200: the fire mage fell from 743 to 575 when the leftover points that used to
    // finish the Fire tab went to the Arcane tail), the damage ones first; the last 7 points are Arcane Subtlety and Arcane Concentration.
    // The 70532 client data gives Critical Mass 5 ranks, and Combustion hangs under it in full (236: Combustion refused under node 105784 at 3/5);
    // it has no Wake of Fire, and Improved Fire Blast (2 ranks) in its place. A rank above a node's maximum is clamped (Critical Mass 3 on 70450).
    { "Accelerant|Improved Fireball", 5 }, { "Ignite", 5 }, { "Fire Power", 5 }, { "Critical Mass", 3 }, { "Improved Scorch", 3 },
    { "Master of Elements", 3 }, { "Incineration", 5 }, { "Pyroblast", 1 }, { "Combustion", 1 }, { "Impact", 3 },
    { "Improved Flamestrike", 3 }, { "Blast Wave", 1 }, { "Burning Soul", 3 }, { "Wake of Fire|Improved Fire Blast", 2 }, { "Heating Up", 1 },
    { "Flamethrowing", 2 },
    { "Arcane Subtlety", 2 }, { "Arcane Concentration", 5 },
};
constexpr TalentPick kMageFrost[] = {
    // Cold Snap sits a tier down that needs 20 points in the tree: bought third it was refused (LearnResult 1, 147), so the rows above it go first.
    { "Improved Frostbolt", 5 }, { "Ice Shards", 5 }, { "Piercing Ice", 3 },
    { "Improved Blizzard", 1 }, { "Frost Channeling", 3 }, { "Shatter", 2 }, { "Ice Block", 1 },
    { "Precision|Elemental Precision", 3 }, { "Cold Snap", 1 }, { "Frostbite", 1 }, { "Winter's Chill", 5 }, { "Ice Barrier", 1 },
    { "Arcane Subtlety", 2 }, { "Arcane Focus", 3 }, { "Arcane Concentration", 5 },
    { "Arcane Impact", 3 }, { "Magic Absorption", 2 }, { "Arcane Meditation", 3 },
    { "Improved Channeling", 2 },
};

// ---- Warlock ----
// Forever's warlock tree has no Fel Intellect, Fel Stamina, Improved Healthstone, Devastation, Improved Firebolt, Improved Drain Soul, Grim Reach,
// Improved Curse of Weakness, Improved Immolate or Emberstorm (147 log: "build talents not found"); its names are Improved Bane of Agony, Improved Drains,
// Improved Health Funnel, Improved Imp, Fel Vitality, Aftermath, Pyroclasm, Intensity and the others of the client list. Filler picks (third field)
// are bought only with points left over, so an unknown rank cap never starves the core.
constexpr TalentPick kWarlockAffliction[] = {
    // The 70532 client data has no Malediction (236: "build talents not found: Malediction"); its row holds Malevolence (node 110876, spell
    // 1310949: critical chance of Shadow spells, 5 ranks), which takes the same 3 points there.
    { "Improved Corruption", 5 }, { "Improved Life Tap", 2 }, { "Improved Drains", 1 },
    { "Improved Bane of Agony", 3 }, { "Amplify Curse", 1 },
    { "Fel Concentration", 3 }, { "Nightfall", 2 }, { "Suppression", 5 }, { "Siphon Life", 1 },
    { "Malediction|Malevolence", 3 }, { "Shadow Mastery", 5 }, { "Pandemic", 1 },
    { "Demonic Embrace", 5 }, { "Fel Domination", 1 }, { "Master Summoner", 2 }, { "Unholy Power", 2 },
    { "Improved Shadow Bolt", 5, true }, { "Bane", 5, true },
};
constexpr TalentPick kWarlockDemonology[] = {
    // Destruction/Ruin with a sacrificed Imp: the Demonology points that reach Demonic Sacrifice, then Destruction down to Ruin.
    { "Demonic Embrace", 5 }, { "Improved Imp", 3 }, { "Improved Health Funnel", 2 }, { "Fel Domination", 1 },
    { "Master Summoner", 2 }, { "Unholy Power", 5 }, { "Demonic Sacrifice", 1 },
    { "Improved Shadow Bolt", 5 }, { "Bane", 5 }, { "Destructive Reach", 2 }, { "Cataclysm", 1 }, { "Ruin", 1 },
    { "Aftermath", 5 }, { "Pyroclasm", 3 }, { "Intensity", 2 },
    { "Fel Vitality", 3, true }, { "Master Demonologist", 5, true },
};
constexpr TalentPick kWarlockDestruction[] = {
    { "Improved Shadow Bolt", 5 }, { "Bane", 5 }, { "Destructive Reach", 2 }, { "Cataclysm", 1 }, { "Intensity", 2 },
    { "Aftermath", 5 }, { "Ruin", 1 }, { "Pyroclasm", 3 }, { "Conflagrate", 1 }, { "Shadowburn", 1 }, { "Decimation", 3 },
    { "Demonic Embrace", 5 }, { "Fel Domination", 1 }, { "Master Summoner", 2 }, { "Unholy Power", 2 },
    { "Improved Imp", 3, true }, { "Improved Health Funnel", 2, true }, { "Fel Vitality", 3, true },
};

// ---- Druid ----
constexpr TalentPick kDruidBalance[] = {
    // Forever's druid trees have no Omen of Clarity, Natural Weapons or Improved Mark of the Wild (damage_parity 147); Insect Swarm is a talent
    // there and the rotation casts it.
    { "Improved Wrath", 5 }, { "Improved Moonfire", 5 }, { "Nature's Reach", 2 }, { "Vengeance", 5 },
    { "Nature's Grace", 1 }, { "Moonglow", 3 }, { "Improved Starfire", 5 }, { "Moonfury", 5 },
    { "Moonkin Form", 1 }, { "Insect Swarm", 1 },
    { "Heart of the Wild", 5 }, { "Nature's Focus", 5 }, { "Subtlety", 3 },
    { "Improved Rejuvenation", 3 }, { "Tranquil Spirit", 2 },
};
// Forever's feral tree names (damage_parity 147 talent list): no Feral Aggression, Improved Shred, Faerie Fire (Feral), Primal Fury or Improved Enrage.
// Forever launch data (hotfixes 2026_11_03_41) swaps spells between nodes: the Feral node that was Heart of the Wild (definition 134400)
// is Furor and the Restoration node that was Furor (134419) is Heart of the Wild; Reflection (134378) and Subtlety (134381) swap the same
// way; Berserk's node (134417) is Primal Bite, Primal Bite's (134410) is Lacerate. The builds name the new spells so that every node
// keeps the points it had, and the tiers below them still open.
constexpr TalentPick kDruidFeralCat[] = {
    { "Ferocity", 5 }, { "Feral Swiftness", 2 }, { "Sharpened Claws", 3 }, { "Shredding Attacks", 2 },
    { "Predatory Strikes", 3 }, { "Blood Frenzy", 2 }, { "Savage Fury", 2 }, { "Furor", 5 },
    { "Leader of the Pack", 1 }, { "Heart of the Wild", 5 }, { "Rend and Tear", 5 }, { "Brutal Impact", 2 },
    { "Feral Instinct", 5 }, { "Natural Shapeshifter", 3 }, { "Berserk|Primal Bite", 1 }, { "Reflection", 2 }, { "Subtlety", 3 },
};
constexpr TalentPick kDruidFeralBear[] = {
    { "Ferocity", 5 }, { "Thick Hide", 5 }, { "Feral Charge", 1 }, { "Sharpened Claws", 3 },
    { "Feral Swiftness", 2 }, { "Predatory Strikes", 3 }, { "Savage Fury", 2 }, { "Blood Frenzy", 2 },
    { "Furor", 5 }, { "Leader of the Pack", 1 }, { "Heart of the Wild", 5 }, { "Brutal Impact", 2 },
    { "Feral Instinct", 5 }, { "Natural Shapeshifter", 3 }, { "Lacerate", 1 }, { "Reflection", 2 }, { "Subtlety", 3 },
};
constexpr TalentPick kDruidRestoration[] = {
    // Nature's Swiftness (node 104921) hangs under Naturalist (node 104922, edge type 2: the parent full), so with Naturalist at 2 of 5 it was
    // refused in every support scorecard (233: "refused: Nature's Swiftness=2", readiness "Nature's Swiftness 0/1"). Naturalist in full; the three
    // points come off Improved Moonfire.
    // Node swaps of the launch data: see the feral builds.
    { "Heart of the Wild", 5 }, { "Genesis", 5 }, { "Subtlety", 3 }, { "Naturalist", 5 },
    { "Tranquil Spirit", 5 }, { "Improved Rejuvenation", 3 },
    { "Gift of Nature", 5 }, { "Improved Regrowth", 5 }, { "Nature's Swiftness", 1 }, { "Swiftmend", 1 }, { "Reflection", 5 },
    { "Improved Wrath", 5 }, { "Improved Moonfire", 1 },
};

// Class default first: it wins ties when a bot's talents fit several builds.
constexpr std::array<TalentBuild, 28> kBuilds = {{
    { CLASS_WARRIOR, 71,  kWarriorArms },
    { CLASS_WARRIOR, 72,  kWarriorFury },
    { CLASS_WARRIOR, 73,  kWarriorProtection },
    { CLASS_PALADIN, 70,  kPaladinRetribution },
    { CLASS_PALADIN, 65,  kPaladinHoly },
    { CLASS_PALADIN, 66,  kPaladinProtection },
    { CLASS_HUNTER,  254, kHunterMarksmanship },
    { CLASS_HUNTER,  253, kHunterBeastMastery },
    { CLASS_HUNTER,  255, kHunterSurvival },
    { CLASS_ROGUE,   260, kRogueCombat },
    { CLASS_ROGUE,   259, kRogueAssassination },
    { CLASS_ROGUE,   261, kRogueSubtlety },
    { CLASS_PRIEST,  257, kPriestHoly },
    { CLASS_PRIEST,  256, kPriestDiscipline },
    { CLASS_PRIEST,  258, kPriestShadow },
    { CLASS_SHAMAN,  263, kShamanEnhancement },
    { CLASS_SHAMAN,  262, kShamanElemental },
    { CLASS_SHAMAN,  264, kShamanRestoration },
    { CLASS_MAGE,    64,  kMageFrost },
    { CLASS_MAGE,    63,  kMageFire },
    { CLASS_MAGE,    62,  kMageArcane },
    { CLASS_WARLOCK, 265, kWarlockAffliction },
    { CLASS_WARLOCK, 266, kWarlockDemonology },
    { CLASS_WARLOCK, 267, kWarlockDestruction },
    { CLASS_DRUID,   102, kDruidBalance },
    { CLASS_DRUID,   103, kDruidFeralCat },
    { CLASS_DRUID,   104, kDruidFeralBear },
    { CLASS_DRUID,   105, kDruidRestoration },
}};

} // namespace

std::span<TalentBuild const> GetTalentBuilds()
{
    return kBuilds;
}

std::span<TalentPick const> GetTalentBuild(uint8 cls, uint32 spec)
{
    for (TalentBuild const& b : kBuilds)
        if (b.cls == cls && b.spec == spec)
            return b.picks;
    return {};
}

} // namespace Playerbot::Classic
