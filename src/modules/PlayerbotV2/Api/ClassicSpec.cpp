#include "ClassicSpec.h"
#include "ClassicTalentBuilds.h"
#include "DB2Stores.h"
#include "Player.h"
#include "SharedDefines.h"
#include "DBCEnums.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TraitMgr.h"
#include "TraitPacketsCommon.h"
#include "World.h"
#include "Log.h"
#include <array>
#include <cctype>
#include <cmath>
#include <set>
#include <map>
#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Playerbot::Classic {

namespace {

struct ClassSpecs
{
    uint8 cls;
    std::array<uint32, 4> specs;   // retail ChrSpecialization ids, 0 = unused
};

constexpr std::array<ClassSpecs, 13> kRetailSpecs = {{
    { CLASS_WARRIOR,      { 71, 72, 73, 0 } },
    { CLASS_PALADIN,      { 65, 66, 70, 0 } },
    { CLASS_HUNTER,       { 253, 254, 255, 0 } },
    { CLASS_ROGUE,        { 259, 260, 261, 0 } },
    { CLASS_PRIEST,       { 256, 257, 258, 0 } },
    { CLASS_DEATH_KNIGHT, { 250, 251, 252, 0 } },
    { CLASS_SHAMAN,       { 262, 263, 264, 0 } },
    { CLASS_MAGE,         { 62, 63, 64, 0 } },
    { CLASS_WARLOCK,      { 265, 266, 267, 0 } },
    { CLASS_MONK,         { 268, 269, 270, 0 } },
    { CLASS_DRUID,        { 102, 103, 104, 105 } },
    { CLASS_DEMON_HUNTER, { 577, 581, 1480, 0 } },
    { CLASS_EVOKER,       { 1467, 1468, 1473, 0 } },
}};

// Vanilla talent spells (rank 1 ids) that identify a talent build, checked in
// order: the first one the player knows decides. Deep talents of the less
// ambiguous tree come first (a Shadow priest also has Inner Focus).
struct Signature
{
    uint32 spell;
    uint32 spec;
};

struct ClassRules
{
    uint8 cls;
    uint32 fallback;
    std::array<Signature, 8> signatures;   // spell 0 = unused
};

constexpr std::array<ClassRules, 9> kRules = {{
    { CLASS_WARRIOR, 72, {{
        { 23922, 73 },   // Shield Slam
        { 12809, 73 },   // Concussion Blow
        { 23881, 72 },   // Bloodthirst
        { 12294, 71 },   // Mortal Strike
    }}},
    { CLASS_PALADIN, 70, {{
        { 20473, 65 },   // Holy Shock
        { 20925, 66 },   // Holy Shield
        { 20066, 70 },   // Repentance
        { 20216, 65 },   // Divine Favor
        { 20375, 70 },   // Seal of Command
    }}},
    { CLASS_HUNTER, 254, {{
        { 19574, 253 },  // Bestial Wrath
        // Vanilla Survival is still a ranged spec; retail 255 is melee, so
        // Survival talents map to Marksmanship (the fallback).
    }}},
    { CLASS_ROGUE, 260, {{
        { 14177, 259 },  // Cold Blood
        { 13750, 260 },  // Adrenaline Rush
        { 14185, 261 },  // Preparation
        { 16511, 261 },  // Hemorrhage
        { 13877, 260 },  // Blade Flurry
    }}},
    { CLASS_PRIEST, 257, {{
        { 15473, 258 },  // Shadowform
        { 15286, 258 },  // Vampiric Embrace
        { 15407, 258 },  // Mind Flay
        { 10060, 256 },  // Power Infusion
        { 20711, 257 },  // Spirit of Redemption
        { 724,   257 },  // Lightwell
    }}},
    { CLASS_SHAMAN, 263, {{
        { 16190, 264 },  // Mana Tide Totem
        { 16188, 264 },  // Nature's Swiftness
        { 17364, 263 },  // Stormstrike
        { 16166, 262 },  // Elemental Mastery
    }}},
    { CLASS_MAGE, 63, {{   // fallback: fire, the highest-damage raid spec (parity 184: fire 743, frost 523 buffed)
        { 12042, 62 },   // Arcane Power
        { 11129, 63 },   // Combustion
        { 11426, 64 },   // Ice Barrier
        { 11366, 63 },   // Pyroblast
        { 12043, 62 },   // Presence of Mind
    }}},
    { CLASS_WARLOCK, 267, {{
        { 18220, 265 },  // Dark Pact
        { 18265, 265 },  // Siphon Life
        { 19028, 266 },  // Soul Link
        { 18788, 266 },  // Demonic Sacrifice
        { 17962, 267 },  // Conflagrate
        { 17877, 267 },  // Shadowburn
    }}},
    { CLASS_DRUID, 103, {{
        { 24858, 102 },  // Moonkin Form
        { 18562, 105 },  // Swiftmend
        { 17116, 105 },  // Nature's Swiftness
        { 17007, 104 },  // Leader of the Pack (vanilla ferals tank in raids)
        { 16979, 104 },  // Feral Charge
    }}},
}};

std::shared_mutex g_overrideLock;
std::unordered_map<ObjectGuid, uint32> g_overrides;

// Talent name <-> trait node entry, per class. The class trees never change
// after load, so each class is resolved once.
struct ClassTalentIndex
{
    std::unordered_map<std::string, TalentNode> byName;              // TalentKey -> node
    std::map<std::pair<int32, int32>, std::string> nameByEntry;     // node -> TalentKey of its first name
};

// Lookup key of a talent name: lower case letters and digits only, so the
// build's "Two-Handed Weapon Specialization" matches "Two Handed Weapon
// Specialization" or a different capitalisation in the client data.
std::string TalentKey(std::string_view name)
{
    std::string key;
    key.reserve(name.size());
    for (char c : name)
        if (std::isalnum(static_cast<unsigned char>(c)))
            key.push_back(char(std::tolower(static_cast<unsigned char>(c))));
    return key;
}

// A build pick may name alternatives, "A|B": the node of the first one the client data has. The client builds differ in a few nodes
// (70450 has Call of Thunder and Improved Fire Nova where 70532 has Elemental Mastery and Trial by Fire), and one build serves both.
template <typename Fn>
auto FirstTalentAlternative(std::string_view name, Fn&& lookup) -> decltype(lookup(std::string()))
{
    for (std::size_t from = 0;;)
    {
        std::size_t const bar = name.find('|', from);
        if (auto found = lookup(TalentKey(name.substr(from, bar == std::string_view::npos ? std::string_view::npos : bar - from))))
            return found;
        if (bar == std::string_view::npos)
            return {};
        from = bar + 1;
    }
}

std::mutex g_indexLock;
std::unordered_map<uint8, ClassTalentIndex> g_indexes;

// Talent tabs per class (GetTalentTabs) and the owners' talent codes per bot (SetTalentCode).
std::mutex g_tabsLock;
std::unordered_map<uint8, std::vector<std::vector<TalentSlot>>> g_tabs;
std::shared_mutex g_codeLock;
std::unordered_map<ObjectGuid, std::string> g_codes;


// English names of the class talents (spell id, 1.60 name, 1.12 name where it differs): the talent builds use them, the
// realm's DBC locale may not be enUS.
struct TalentEnglishName { uint32 spell; char const* name; char const* oldName; };
constexpr TalentEnglishName kTalentEnglishNames[] = {
    { 5570, "Insect Swarm", nullptr },
    { 5923, "Holy Power", nullptr },
    { 6057, "Wand Specialization", nullptr },
    { 9452, "Vindication", nullptr },
    { 9453, "Unyielding Faith", nullptr },
    { 9799, "Eye for an Eye", nullptr },
    { 10060, "Power Infusion", nullptr },
    { 11069, "Accelerant", "Improved Fireball" },
    { 11070, "Improved Frostbolt", nullptr },
    { 11071, "Frostbite", nullptr },
    { 11078, "Improved Fire Blast", nullptr },
    { 11083, "Burning Soul", nullptr },
    { 11094, "Improved Fire Ward", nullptr },
    { 11095, "Improved Scorch", nullptr },
    { 11100, "Flame Throwing", nullptr },
    { 11103, "Impact", nullptr },
    { 11108, "Improved Flamestrike", nullptr },
    { 11113, "Blast Wave", nullptr },
    { 11115, "Critical Mass", nullptr },
    { 11119, "Ignite", nullptr },
    { 11124, "Fire Power", nullptr },
    { 11129, "Combustion", nullptr },
    { 11151, "Piercing Ice", nullptr },
    { 11160, "Frost Channeling", nullptr },
    { 11165, "Improved Frost Nova", nullptr },
    { 11170, "Shatter", nullptr },
    { 11175, "Permafrost", nullptr },
    { 11180, "Winter's Chill", nullptr },
    { 11185, "Improved Blizzard", nullptr },
    { 11189, "Frost Warding", nullptr },
    { 11190, "Improved Cone of Cold", nullptr },
    { 11207, "Ice Shards", nullptr },
    { 11210, "Arcane Subtlety", nullptr },
    { 11213, "Arcane Concentration", nullptr },
    { 11222, "Arcane Focus", nullptr },
    { 11232, "Arcane Mind", nullptr },
    { 11237, "Improved Channeling", "Improved Arcane Missiles" },
    { 11242, "Arcane Impact", "Improved Arcane Explosion" },
    { 11247, "Arcane Geometry", "Magic Attunement" },
    { 11252, "Arcane Shielding", "Improved Mana Shield" },
    { 11255, "Improved Counterspell", nullptr },
    { 11366, "Pyroblast", nullptr },
    { 11426, "Ice Barrier", nullptr },
    { 11958, "Ice Block", nullptr },
    { 12042, "Arcane Power", nullptr },
    { 12043, "Presence of Mind", nullptr },
    { 12163, "Two-Handed Weapon Specialization", nullptr },
    { 12281, "Weaponmaster", "Sword Specialization" },
    { 12282, "Improved Heroic Strike", nullptr },
    { 12285, "Improved Charge", nullptr },
    { 12286, "Improved Rend", nullptr },
    { 12287, "Improved Thunder Clap", nullptr },
    { 12289, "Improved Hamstring", nullptr },
    { 12290, "Improved Overpower", nullptr },
    { 12292, "Sweeping Strikes", nullptr },
    { 12294, "Mortal Strike", nullptr },
    { 12295, "Improved Tactical Mastery", "Tactical Mastery" },
    { 12296, "Anger Management", nullptr },
    { 12297, "Anticipation", nullptr },
    { 12298, "Shield Specialization", nullptr },
    { 12299, "Toughness", nullptr },
    { 12301, "Improved Bloodrage", nullptr },
    { 12308, "Improved Sunder Armor", nullptr },
    { 12311, "Improved Shield Bash", nullptr },
    { 12312, "Improved Shield Wall", nullptr },
    { 12313, "Improved Disarm", nullptr },
    { 12317, "Enrage", nullptr },
    { 12319, "Flurry", nullptr },
    { 12320, "Cruelty", nullptr },
    { 12321, "Booming Voice", nullptr },
    { 12322, "Unbridled Wrath", nullptr },
    { 12323, "Piercing Howl", nullptr },
    { 12328, "Death Wish", nullptr },
    { 12329, "Improved Cleave", nullptr },
    { 12472, "Cold Snap", nullptr },
    { 12700, "Weaponmaster", "Axe Specialization" },
    { 12792, "Defiance", nullptr },
    { 12797, "Improved Revenge", nullptr },
    { 12809, "Concussion Blow", nullptr },
    { 12834, "Deep Wounds", nullptr },
    { 12862, "Improved Slam", nullptr },
    { 12962, "Iron Will", nullptr },
    { 12975, "Last Stand", nullptr },
    { 13705, "Precision", nullptr },
    { 13709, "Hack and Slash", "Mace Specialization" },
    { 13712, "Lightning Reflexes", nullptr },
    { 13713, "Deflection", nullptr },
    { 13715, "Dual Wield Specialization", nullptr },
    { 13732, "Efficient Strikes", "Improved Sinister Strike" },
    { 13741, "Improved Gouge", nullptr },
    { 13742, "Endurance", nullptr },
    { 13743, "Improved Sprint", nullptr },
    { 13750, "Adrenaline Rush", nullptr },
    { 13754, "Improved Kick", nullptr },
    { 13877, "Blade Flurry", nullptr },
    { 13958, "Master of Deception", nullptr },
    { 13960, "Hack and Slash", "Sword Specialization" },
    { 13975, "Camouflage", nullptr },
    { 13976, "Initiative", nullptr },
    { 13981, "Elusiveness", nullptr },
    { 13983, "Setup", nullptr },
    { 14057, "Opportunity", nullptr },
    { 14079, "Improved Ambush", nullptr },
    { 14082, "Dirty Deeds", nullptr },
    { 14113, "Improved Poisons", nullptr },
    { 14128, "Lethality", nullptr },
    { 14138, "Malice", nullptr },
    { 14144, "Remorseless Attacks", nullptr },
    { 14156, "Ruthlessness", nullptr },
    { 14162, "Coup de Grace", "Improved Eviscerate" },
    { 14165, "Improved Slice and Dice", nullptr },
    { 14168, "Improved Expose Armor", nullptr },
    { 14171, "Serrated Blades", nullptr },
    { 14174, "Improved Kidney Shot", nullptr },
    { 14177, "Cold Blood", nullptr },
    { 14179, "Relentless Strikes", nullptr },
    { 14183, "Premeditation", nullptr },
    { 14185, "Preparation", nullptr },
    { 14186, "Seal Fate", nullptr },
    { 14251, "Riposte", nullptr },
    { 14278, "Ghostly Strike", nullptr },
    { 14520, "Mental Agility", nullptr },
    { 14521, "Meditation", nullptr },
    { 14523, "Silent Resolve", nullptr },
    { 14524, "Wand Specialization", nullptr },
    { 14531, "Martyrdom", nullptr },
    { 14747, "Improved Inner Fire", nullptr },
    { 14748, "Improved Power Word: Shield", nullptr },
    { 14750, "Improved Mana Burn", nullptr },
    { 14751, "Inner Focus", nullptr },
    { 14889, "Holy Specialization", nullptr },
    { 14892, "Inspiration", nullptr },
    { 14898, "Spiritual Healing", nullptr },
    { 14901, "Spiritual Guidance", nullptr },
    { 14908, "Improved Renew", nullptr },
    { 14909, "Searing Light", nullptr },
    { 14912, "Improved Healing", nullptr },
    { 14913, "Twilight Focus", "Healing Focus" },
    { 14983, "Vigor", nullptr },
    { 15058, "Arcane Instability", nullptr },
    { 15237, "Holy Nova", nullptr },
    { 15257, "Shadow Weaving", nullptr },
    { 15259, "Darkness", nullptr },
    { 15260, "Shadow Focus", nullptr },
    { 15270, "Spirit Tap", nullptr },
    { 15273, "Improved Mind Blast", nullptr },
    { 15275, "Improved Shadow Word: Pain", nullptr },
    { 15286, "Vampiric Embrace", nullptr },
    { 15326, "Blackout", nullptr },
    { 15392, "Improved Psychic Scream", nullptr },
    { 15407, "Mind Flay", nullptr },
    { 15473, "Shadowform", nullptr },
    { 15487, "Silence", nullptr },
    { 16035, "Concussion", nullptr },
    { 16038, "Call of Flame", nullptr },
    { 16039, "Convection", nullptr },
    { 16040, "Reverberation", nullptr },
    { 16043, "Earth's Grasp", nullptr },
    { 16086, "Trial by Fire", "Improved Fire Totems" },
    { 16089, "Elemental Fury", nullptr },
    { 16120, "Call of Thunder", nullptr },
    { 16164, "Elemental Focus", nullptr },
    { 16166, "Lightning Mastery", "Elemental Mastery" },
    { 16173, "Totemic Focus", nullptr },
    { 16176, "Ancestral Healing", nullptr },
    { 16178, "Purification", nullptr },
    { 16179, "Tidal Focus", nullptr },
    { 16181, "Healing Focus", nullptr },
    { 16182, "Improved Healing Wave", nullptr },
    { 16184, "Improved Reincarnation", nullptr },
    { 16187, "Restorative Totems", nullptr },
    { 16188, "Nature's Swiftness", nullptr },
    { 16190, "Mana Tide Totem", nullptr },
    { 16194, "Tidal Mastery", nullptr },
    { 16252, "Toughness", nullptr },
    { 16254, "Anticipation", nullptr },
    { 16255, "Thundering Strikes", nullptr },
    { 16256, "Flurry", nullptr },
    { 16258, "Guardian Totems", nullptr },
    { 16261, "Improved Lightning Shield", nullptr },
    { 16262, "Improved Ghost Wolf", nullptr },
    { 16266, "Elemental Weapons", nullptr },
    { 16268, "Spirit Weapons", "Parry" },
    { 16462, "Deflection", nullptr },
    { 16487, "Blood Craze", nullptr },
    { 16493, "Impale", nullptr },
    { 16511, "Hemorrhage", nullptr },
    { 16513, "Vile Poisons", nullptr },
    { 16538, "Bastion", "One-Handed Weapon Specialization" },
    { 16578, "Elemental Alacrity", "Lightning Mastery" },
    { 16757, "Arctic Reach", nullptr },
    { 16814, "Improved Wrath", nullptr },
    { 16819, "Nature's Reach", nullptr },
    { 16821, "Improved Moonfire", nullptr },
    { 16833, "Natural Shapeshifter", nullptr },
    { 16845, "Moonglow", nullptr },
    { 16850, "Improved Starfire", nullptr },
    { 16880, "Nature's Grace", nullptr },
    { 16896, "Moonfury", nullptr },
    { 16909, "Vengeance", nullptr },
    { 16918, "Improved Entangling Roots", nullptr },
    { 16929, "Thick Hide", nullptr },
    { 16934, "Ferocity", nullptr },
    { 16940, "Brutal Impact", nullptr },
    { 16942, "Sharpened Claws", nullptr },
    { 16947, "Feral Instinct", nullptr },
    { 16958, "Blood Frenzy", "Primal Fury" },
    { 16966, "Shredding Attacks", "Improved Shred" },
    { 16972, "Predatory Strikes", nullptr },
    { 16979, "Feral Charge (Bear)", "Feral Charge" },
    { 16998, "Savage Fury", nullptr },
    { 17002, "Feral Swiftness", "Feline Swiftness" },
    { 17003, "Heart of the Wild", nullptr },
    { 17007, "Leader of the Pack", nullptr },
    { 17056, "Furor", nullptr },
    { 17063, "Nature's Focus", nullptr },
    { 17069, "Naturalist", "Improved Healing Touch" },
    { 17074, "Improved Regrowth", nullptr },
    { 17104, "Gift of Nature", nullptr },
    { 17106, "Reflection", nullptr },
    { 17111, "Improved Rejuvenation", nullptr },
    { 17116, "Nature's Swiftness", nullptr },
    { 17118, "Subtlety", nullptr },
    { 17123, "Improved Tranquility", nullptr },
    { 17245, "Overgrowth", "Improved Nature's Grasp" },
    { 17322, "Shadow Reach", nullptr },
    { 17364, "Stormstrike", nullptr },
    { 17485, "Ancestral Knowledge", nullptr },
    { 17778, "Cataclysm", nullptr },
    { 17783, "Fel Concentration", nullptr },
    { 17788, "Bane", nullptr },
    { 17793, "Improved Shadow Bolt", nullptr },
    { 17804, "Soul Siphon", "Improved Drain Life" },
    { 17810, "Improved Corruption", nullptr },
    { 17877, "Shadowburn", nullptr },
    { 17917, "Destructive Reach", nullptr },
    { 17927, "Agonizing Flames", "Improved Searing Pain" },
    { 17959, "Ruin", nullptr },
    { 18094, "Nightfall", nullptr },
    { 18119, "Aftermath", nullptr },
    { 18135, "Intensity", nullptr },
    { 18174, "Suppression", nullptr },
    { 18182, "Improved Life Tap", nullptr },
    { 18218, "Grim Reach", nullptr },
    { 18223, "Curse of Exhaustion", nullptr },
    { 18265, "Siphon Life", nullptr },
    { 18271, "Shadow Mastery", nullptr },
    { 18288, "Amplify Curse", nullptr },
    { 18427, "Aggression", nullptr },
    { 18459, "Incineration", "Incinerate" },
    { 18462, "Arcane Meditation", nullptr },
    { 18530, "Divine Fury", nullptr },
    { 18551, "Mental Strength", nullptr },
    { 18562, "Swiftmend", nullptr },
    { 18694, "Improved Imp", nullptr },
    { 18697, "Demonic Embrace", nullptr },
    { 18703, "Improved Health Funnel", nullptr },
    { 18705, "Improved Voidwalker", nullptr },
    { 18708, "Fel Domination", nullptr },
    { 18709, "Master Summoner", nullptr },
    { 18731, "Fel Vitality", "Fel Intellect" },
    { 18754, "Improved Sayaad", nullptr },
    { 18769, "Unholy Power", nullptr },
    { 18788, "Demonic Sacrifice", nullptr },
    { 18827, "Improved Bane of Agony", "Improved Curse of Agony" },
    { 19028, "Soul Link", nullptr },
    { 19159, "Savage Strikes", nullptr },
    { 19168, "Lightning Reflexes", nullptr },
    { 19184, "Entrapment", nullptr },
    { 19228, "Improved Wing Clip", nullptr },
    { 19239, "Clever Traps", nullptr },
    { 19255, "Survivalist", nullptr },
    { 19263, "Deterrence", nullptr },
    { 19290, "Surefooted", nullptr },
    { 19295, "Deflection", nullptr },
    { 19306, "Counterattack", nullptr },
    { 19376, "Survival Tactics", "Trap Mastery" },
    { 19407, "Improved Concussive Shot", nullptr },
    { 19416, "Efficiency", nullptr },
    { 19421, "Improved Hunter's Mark", nullptr },
    { 19426, "Lethal Attacks", "Lethal Shots" },
    { 19454, "Improved Arcane Shot", nullptr },
    { 19461, "Barrage", nullptr },
    { 19464, "Improved Serpent Sting", nullptr },
    { 19485, "Mortal Shots", nullptr },
    { 19498, "Hawk Eye", nullptr },
    { 19503, "Scatter Shot", nullptr },
    { 19507, "Ranged Weapon Specialization", nullptr },
    { 19549, "Improved Aspect of the Monkey", nullptr },
    { 19552, "Deadly Aspects", "Improved Aspect of the Hawk" },
    { 19559, "Pathfinding", nullptr },
    { 19572, "Improved Mend Pet", nullptr },
    { 19574, "Bestial Wrath", nullptr },
    { 19577, "Intimidation", nullptr },
    { 19578, "Spirit Bond", nullptr },
    { 19583, "Endurance Training", nullptr },
    { 19590, "Bestial Discipline", nullptr },
    { 19596, "Bestial Swiftness", nullptr },
    { 19598, "Ferocity", nullptr },
    { 19616, "Unleashed Fury", nullptr },
    { 19621, "Frenzy", nullptr },
    { 20049, "Vengeance", nullptr },
    { 20060, "Deflection", nullptr },
    { 20066, "Repentance", nullptr },
    { 20096, "Anticipation", nullptr },
    { 20101, "Benediction", nullptr },
    { 20111, "Two-Handed Weapon Specialization", nullptr },
    { 20117, "Conviction", nullptr },
    { 20127, "Redoubt", nullptr },
    { 20143, "Toughness", nullptr },
    { 20150, "Shield Specialization", nullptr },
    { 20174, "Guardian's Favor", nullptr },
    { 20177, "Reckoning", nullptr },
    { 20189, "Precision", nullptr },
    { 20196, "One-Handed Weapon Specialization", nullptr },
    { 20205, "Spiritual Focus", nullptr },
    { 20210, "Illumination", nullptr },
    { 20216, "Divine Favor", nullptr },
    { 20224, "Improved Seals", "Improved Seal of Righteousness" },
    { 20234, "Improved Lay on Hands", nullptr },
    { 20237, "Healing Light", nullptr },
    { 20254, "Improved Concentration Aura", nullptr },
    { 20257, "Divine Intellect", nullptr },
    { 20262, "Divine Strength", nullptr },
    { 20375, "Seal of Command", nullptr },
    { 20468, "Improved Righteous Fury", nullptr },
    { 20487, "Improved Hammer of Justice", nullptr },
    { 20500, "Improved Berserker Rage", nullptr },
    { 20502, "Improved Execute", nullptr },
    { 20504, "Improved Intercept", nullptr },
    { 20711, "Spirit of Redemption", nullptr },
    { 20925, "Holy Shield", nullptr },
    { 22571, "Mangle", nullptr },
    { 23584, "Dual Wield Specialization", nullptr },
    { 23785, "Master Demonologist", nullptr },
    { 23881, "Bloodthirst", nullptr },
    { 23922, "Shield Slam", nullptr },
    { 24293, "Improved Tracking", "Monster Slaying" },
    { 24443, "Improved Revive Pet", nullptr },
    { 24858, "Moonkin Form", nullptr },
    { 24968, "Tranquil Spirit", nullptr },
    { 25956, "Improved Judgement", nullptr },
    { 26022, "Pursuit of Justice", nullptr },
    { 27789, "Holy Reach", nullptr },
    { 27811, "Blessed Recovery", nullptr },
    { 27900, "Spell Warding", nullptr },
    { 28574, "Arcane Resilience", nullptr },
    { 28996, "Elemental Warding", nullptr },
    { 28999, "Elemental Reach", "Storm Reach" },
    { 29062, "Eye of the Storm", nullptr },
    { 29074, "Master of Elements", nullptr },
    { 29187, "Natural Grace", "Healing Grace" },
    { 29206, "Healing Way", nullptr },
    { 29438, "Precision", "Elemental Precision" },
    { 29441, "Magic Absorption", nullptr },
    { 30160, "Elemental Devastation", nullptr },
    { 30892, "Sleight of Hand", nullptr },
    { 30894, "Heightened Senses", nullptr },
    { 30902, "Deadliness", nullptr },
    { 30919, "Weapon Expertise", nullptr },
    { 399956, "Mutilate", nullptr },
    { 400574, "Arcane Blast", nullptr },
    { 401859, "Prayer of Mending", nullptr },
    { 402174, "Penance", nullptr },
    { 407669, "Avenger's Shield", nullptr },
    { 407995, "Primal Bite", "Mangle" },
    { 408120, "Wild Growth", nullptr },
    { 408510, "Water Shield", nullptr },
    { 408521, "Riptide", nullptr },
    { 412758, "Incinerate", nullptr },
    { 417141, "Berserk", nullptr },
    { 425336, "Rage of the Farseer", "Shamanistic Rage" },
    { 437032, "Soul Harvest", "Soul Harvesting" },
    { 1224716, "Puncturing Wounds", nullptr },
    { 1225228, "Bane of Havoc", nullptr },
    { 1238122, "Feral Charge", nullptr },
    { 1293241, "Summon Hawk", nullptr },
    { 1293817, "Conflagrate", nullptr },
    { 1299346, "Trueshot Aura", nullptr },
    { 1310222, "Spearing Strike", nullptr },
    { 1310687, "Sniper Shot", nullptr },
    { 1310711, "Flawless Execution", nullptr },
    { 1310735, "Unsealed Wrath", nullptr },
    { 1310911, "Light's Vigil", nullptr },
    { 1310994, "Swift Judgement", nullptr },
    { 1311015, "Templar's Bulwark", nullptr },
    { 1311074, "Sanctified Judgement", nullptr },
    { 1311606, "Holy Shock", nullptr },
    { 1312002, "Ice Lance", nullptr },
    { 1316697, "Wrack", nullptr },
    { 1316709, "Death Mark", nullptr },
    { 1317006, "Litany of Light", nullptr },
    { 1317257, "Strider Kick", nullptr },
    { 1322605, "Shifting Power", nullptr },
    { 1326079, "Arcane Wrath", nullptr },
    { 1328548, "Oath of Light", nullptr },
    { 1328617, "Lacerate", nullptr },
    { 1329328, "Venom", nullptr },
    { 1329555, "Reckless Swings", nullptr },
};

// old = the 1.12 spelling only (where it differs), else the 1.60 spelling
char const* EnglishTalentName(uint32 spellId, bool old)
{
    for (TalentEnglishName const& e : kTalentEnglishNames)
        if (e.spell == spellId)
            // spells of 1.60 runes and new talents (ids from 400000) never stood in a 1.12 build: no old pass for them, so a rune whose 1.12 name is a classic talent name (407995 Mangle) cannot take that key
            return old ? (spellId < 400000 ? (e.oldName ? e.oldName : e.name) : nullptr) : e.name;
    return nullptr;
}

std::vector<std::string> EnglishTalentNamesOf(int32 traitDefinitionId, bool old)
{
    std::vector<std::string> names;
    TraitDefinitionEntry const* def = sTraitDefinitionStore.LookupEntry(traitDefinitionId);
    if (!def)
        return names;
    for (int32 spellId : { def->SpellID, def->VisibleSpellID })
        if (char const* en = EnglishTalentName(uint32(spellId), old))
            names.emplace_back(en);
    return names;
}

// Every name a talent goes by: the definition's override name and the names
// of its spell and its visible spell (the tooltip shows the override name if
// set, else the visible spell's; a build may use either).
std::vector<std::string> TalentNamesOf(int32 traitDefinitionId)
{
    std::vector<std::string> names;
    TraitDefinitionEntry const* def = sTraitDefinitionStore.LookupEntry(traitDefinitionId);
    if (!def)
        return names;
    LocaleConstant const locale = sWorld->GetDefaultDbcLocale();
    if (char const* overrideName = def->OverrideName[locale]; overrideName && *overrideName)
        names.emplace_back(overrideName);
    for (int32 spellId : { def->SpellID, def->VisibleSpellID })
        if (SpellInfo const* info = sSpellMgr->GetSpellInfo(uint32(spellId), DIFFICULTY_NONE))
            if (char const* name = info->SpellName->Str[locale]; name && *name)
                names.emplace_back(name);
    return names;
}

ClassTalentIndex const* GetClassTalentIndex(Player const* player)
{
    uint8 const cls = player->GetClass();
    std::lock_guard lock(g_indexLock);
    if (auto itr = g_indexes.find(cls); itr != g_indexes.end())
        return &itr->second;

    UF::TraitConfig const* active = player->GetTraitConfig(player->m_activePlayerData->ActiveCombatTraitConfigID);
    if (!active)
        return nullptr;

    ClassTalentIndex& index = g_indexes[cls];
    auto const entries = TraitMgr::GetNodeEntriesForConfig(WorldPackets::Traits::TraitConfig(*active));
    auto add = [&](TraitMgr::NodeEntryInfo const& e, std::string const& name)
    {
        std::string key = TalentKey(name);
        if (key.empty())
            return;
        index.nameByEntry.try_emplace({ e.TraitNodeID, e.TraitNodeEntryID }, key);
        if (!index.byName.try_emplace(key, TalentNode{ e.TraitNodeID, e.TraitNodeEntryID, e.MaxRanks }).second)
            if (TalentNode const& held = index.byName[key]; held.TraitNodeEntryID != e.TraitNodeEntryID)
                TC_LOG_DEBUG("playerbot.v2", "[talents] class {}: name {} of entry {} already names entry {}", cls, name, e.TraitNodeEntryID, held.TraitNodeEntryID);
    };
    // The builds name talents in English, in the 1.12 spelling: those names first, so a 1.60 rename that reuses another
    // talent's old name (Lightning Mastery) cannot take it; then the realm locale's names and the 1.60 English names.
    for (TraitMgr::NodeEntryInfo const& e : entries)
        for (std::string const& name : EnglishTalentNamesOf(e.TraitDefinitionID, true))
            add(e, name);
    for (TraitMgr::NodeEntryInfo const& e : entries)
    {
        for (std::string const& name : TalentNamesOf(e.TraitDefinitionID))
            add(e, name);
        for (std::string const& name : EnglishTalentNamesOf(e.TraitDefinitionID, false))
            add(e, name);
    }
    return &index;
}

uint32 BestBuildFor(uint8 cls, std::unordered_map<std::string, int32> const& owned);

// Which build do the player's purchased talents fit best? Each rank counts
// for a spec whose build lists that talent (up to the build's ranks). 0 when
// the player has no purchased talents; ties go to the earlier build.
uint32 InferFromTalents(Player const* player)
{
    UF::TraitConfig const* active = player->GetTraitConfig(player->m_activePlayerData->ActiveCombatTraitConfigID);
    if (!active)
        return 0;
    ClassTalentIndex const* index = GetClassTalentIndex(player);
    if (!index)
        return 0;

    std::unordered_map<std::string, int32> owned;
    for (UF::TraitEntry const& e : active->Entries)
        if (e.Rank > 0)
            if (auto itr = index->nameByEntry.find({ e.TraitNodeID, e.TraitNodeEntryID }); itr != index->nameByEntry.end())
                owned[itr->second] += e.Rank;
    return BestBuildFor(player->GetClass(), owned);
}

// The spec whose build lists most of these ranks (by talent name key, up to the build's ranks); ties go to the earlier build.
uint32 BestBuildFor(uint8 cls, std::unordered_map<std::string, int32> const& owned)
{
    if (owned.empty())
        return 0;

    uint32 bestSpec = 0;
    int32 bestScore = 0;
    for (TalentBuild const& build : GetTalentBuilds())
    {
        if (build.cls != cls)
            continue;
        int32 score = 0;
        for (TalentPick const& pick : build.picks)
            if (int32 const held = FirstTalentAlternative(pick.name, [&](std::string const& key)
                { auto itr = owned.find(key); return itr != owned.end() ? itr->second : 0; }))
                score += std::min<int32>(held, pick.ranks);
        if (score > bestScore)
        {
            bestScore = score;
            bestSpec = build.spec;
        }
    }
    return bestSpec;
}

} // namespace

TalentNode const* FindTalent(Player const* player, std::string const& name)
{
    ClassTalentIndex const* index = player ? GetClassTalentIndex(player) : nullptr;
    if (!index)
        return nullptr;
    return FirstTalentAlternative(name, [&](std::string const& key) -> TalentNode const*
    {
        auto itr = index->byName.find(key);
        return itr != index->byName.end() ? &itr->second : nullptr;
    });
}

std::vector<std::string> ListTalentNames(Player const* player)
{
    std::vector<std::string> names;
    ClassTalentIndex const* index = player ? GetClassTalentIndex(player) : nullptr;
    if (!index)
        return names;
    names.reserve(index->byName.size());
    for (auto const& [name, node] : index->byName)
        names.push_back(name);
    std::ranges::sort(names);
    return names;
}

std::vector<std::vector<TalentSlot>> const* GetTalentTabs(Player const* player)
{
    if (!player)
        return nullptr;
    uint8 const cls = player->GetClass();
    {
        std::lock_guard lock(g_tabsLock);
        if (auto itr = g_tabs.find(cls); itr != g_tabs.end())
            return itr->second.empty() ? nullptr : &itr->second;
    }
    UF::TraitConfig const* active = player->GetTraitConfig(player->m_activePlayerData->ActiveCombatTraitConfigID);
    if (!active)
        return nullptr;   // not cached: a bot without a config yet says nothing about its class
    ClassTalentIndex const* index = GetClassTalentIndex(player);

    // The client data draws a class's three tabs side by side in one trait tree: rows 600 apart from the top row (a row's nodes
    // differ by a few units), talents 600 apart in a row, and a wide gap between tabs. Nodes far below the seventh row or off on
    // their own are hidden ones the data keeps (a hunter and a priest node on 70862); they are not in the calculator.
    struct Placed { TraitMgr::NodeEntryInfo info; int32 x; int32 row; };
    std::vector<Placed> placed;
    std::vector<TraitMgr::NodeEntryInfo> const entries = TraitMgr::GetNodeEntriesForConfig(WorldPackets::Traits::TraitConfig(*active));
    // The top row: the highest y that at least three nodes share (one hidden node above the tabs must not shift the rows).
    std::vector<int32> ys;
    for (TraitMgr::NodeEntryInfo const& e : entries)
        if (TraitNodeEntry const* node = sTraitNodeStore.LookupEntry(e.TraitNodeID))
            ys.push_back(node->PosY);
    std::ranges::sort(ys);
    int32 topY = ys.empty() ? 0 : ys.front();
    for (size_t i = 0; i < ys.size(); ++i)
        if (std::ranges::count_if(ys, [&](int32 y) { return std::abs(y - ys[i]) <= 100; }) >= 3)
        {
            topY = ys[i];
            break;
        }
    std::set<int32> seenNodes;
    for (TraitMgr::NodeEntryInfo const& e : entries)
    {
        TraitNodeEntry const* node = sTraitNodeStore.LookupEntry(e.TraitNodeID);
        if (!node || !seenNodes.insert(e.TraitNodeID).second)   // a choice node lists two entries: the calculator has one talent there
            continue;
        int32 const row = int32(std::lround(double(node->PosY - topY) / 600.0));
        if (row < 0 || row > 6)
            continue;
        placed.push_back({ e, node->PosX, row });
    }
    std::vector<std::vector<TalentSlot>> tabs;
    if (!placed.empty())
    {
        std::ranges::sort(placed, [](Placed const& a, Placed const& b) { return a.x < b.x; });
        std::vector<std::vector<Placed>> columns(1);
        for (size_t i = 0; i < placed.size(); ++i)
        {
            if (i && placed[i].x - placed[i - 1].x >= 1500)
                columns.emplace_back();
            columns.back().push_back(placed[i]);
        }
        for (std::vector<Placed>& tab : columns)
        {
            if (tab.size() < 8)   // a stray node, not a tab
                continue;
            std::ranges::sort(tab, [](Placed const& a, Placed const& b) { return a.row != b.row ? a.row < b.row : a.x < b.x; });
            std::vector<TalentSlot>& slots = tabs.emplace_back();
            for (Placed const& p : tab)
            {
                TalentSlot& slot = slots.emplace_back();
                slot.TraitNodeID = p.info.TraitNodeID;
                slot.TraitNodeEntryID = p.info.TraitNodeEntryID;
                slot.MaxRanks = p.info.MaxRanks;
                slot.row = uint8(p.row);
                if (index)
                    if (auto itr = index->nameByEntry.find({ p.info.TraitNodeID, p.info.TraitNodeEntryID }); itr != index->nameByEntry.end())
                        slot.name = itr->second;
            }
        }
    }
    if (tabs.size() != 3)
    {
        TC_LOG_ERROR("playerbot.v2", "[talent_code] class {}: the talent data lays out as {} tabs, not 3: talent codes are refused for it",
            uint32(cls), tabs.size());
        tabs.clear();
    }
    else
        TC_LOG_INFO("playerbot.v2", "[talent_code] class {}: tabs of {}/{}/{} talents", uint32(cls), tabs[0].size(), tabs[1].size(), tabs[2].size());
    std::lock_guard lock(g_tabsLock);
    auto const itr = g_tabs.try_emplace(cls, std::move(tabs)).first;
    return itr->second.empty() ? nullptr : &itr->second;
}

uint32 InferSpecFromRanks(Player const* player, std::vector<std::tuple<int32, int32, int32>> const& ranks)
{
    if (!player)
        return 0;
    ClassTalentIndex const* index = GetClassTalentIndex(player);
    if (!index)
        return 0;
    std::unordered_map<std::string, int32> owned;
    for (auto const& [nodeId, entryId, rank] : ranks)
        if (rank > 0)
            if (auto itr = index->nameByEntry.find({ nodeId, entryId }); itr != index->nameByEntry.end())
                owned[itr->second] += rank;
    return BestBuildFor(player->GetClass(), owned);
}

void SetTalentCode(ObjectGuid guid, std::string code)
{
    std::unique_lock lock(g_codeLock);
    if (code.empty())
        g_codes.erase(guid);
    else
        g_codes[guid] = std::move(code);
}

std::string GetTalentCode(ObjectGuid guid)
{
    std::shared_lock lock(g_codeLock);
    auto itr = g_codes.find(guid);
    return itr != g_codes.end() ? itr->second : std::string();
}

bool IsRetailClassSpec(uint8 cls, uint32 spec)
{
    if (!spec)
        return false;
    for (ClassSpecs const& c : kRetailSpecs)
        if (c.cls == cls)
            for (uint32 s : c.specs)
                if (s == spec)
                    return true;
    return false;
}

uint32 InferClassicSpec(Player const* player)
{
    if (!player)
        return 0;
    uint8 const cls = player->GetClass();
    // Talents bought from our builds say it best; the signature spells below
    // cover characters without trait-config talents (or none at all yet).
    if (uint32 fromTalents = InferFromTalents(player))
        return fromTalents;
    for (ClassRules const& r : kRules)
    {
        if (r.cls != cls)
            continue;
        for (Signature const& sig : r.signatures)
            if (sig.spell && player->HasSpell(sig.spell))
                return sig.spec;
        return r.fallback;
    }
    return 0;
}

uint32 EffectiveSpecId(Player const* player)
{
    if (!player)
        return 0;
    uint32 const real = uint32(AsUnderlyingType(player->GetPrimarySpecialization()));
    if (IsRetailClassSpec(player->GetClass(), real))
        return real;
    if (uint32 forced = GetSpecOverride(player->GetGUID()))
        return forced;
    if (uint32 inferred = InferClassicSpec(player))
        return inferred;
    return real;
}

bool SetSpecOverride(Player const* player, uint32 spec)
{
    if (!player)
        return false;
    uint32 const real = uint32(AsUnderlyingType(player->GetPrimarySpecialization()));
    if (IsRetailClassSpec(player->GetClass(), real))
        return false;
    std::unique_lock lock(g_overrideLock);
    if (!spec)
    {
        g_overrides.erase(player->GetGUID());
        return true;
    }
    if (!IsRetailClassSpec(player->GetClass(), spec))
        return false;
    g_overrides[player->GetGUID()] = spec;
    return true;
}

uint32 GetSpecOverride(ObjectGuid guid)
{
    std::shared_lock lock(g_overrideLock);
    auto itr = g_overrides.find(guid);
    return itr != g_overrides.end() ? itr->second : 0;
}

bool IsSingleSpecClass(uint8 cls)
{
    return sDB2Manager.GetChrSpecializationByIndex(cls, 0) && !sDB2Manager.GetChrSpecializationByIndex(cls, 1);
}

} // namespace Playerbot::Classic
