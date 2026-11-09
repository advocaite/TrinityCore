// SpecWords - the words an owner may whisper to `setspec`: a spec name of the bot's class ("holy", "arms", "prot",
// "beast mastery"), a role ("tank", "heal", "dps") or a ChrSpecialization id. Pure; tested in tests/game/ClassicSpecWords.cpp.
#pragma once

#include "SharedDefines.h"

#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>

namespace Playerbot::V2::SpecWords {

enum class Kind : std::uint8_t { None, Spec, Tank, Healer, Dps };

struct Word
{
    Kind kind = Kind::None;
    std::uint32_t spec = 0;   // Kind::Spec only; a number is passed through for the caller to check against the class
};

namespace Detail {

struct Name { std::uint8_t cls; char const* word; std::uint32_t spec; };

// Lower case, no spaces ("beast mastery" is looked up as "beastmastery").
inline constexpr Name kNames[] =
{
    { CLASS_WARRIOR, "arms", 71 }, { CLASS_WARRIOR, "fury", 72 }, { CLASS_WARRIOR, "prot", 73 }, { CLASS_WARRIOR, "protection", 73 },
    { CLASS_PALADIN, "holy", 65 }, { CLASS_PALADIN, "prot", 66 }, { CLASS_PALADIN, "protection", 66 },
    { CLASS_PALADIN, "ret", 70 }, { CLASS_PALADIN, "retri", 70 }, { CLASS_PALADIN, "retribution", 70 },
    { CLASS_HUNTER, "bm", 253 }, { CLASS_HUNTER, "beast", 253 }, { CLASS_HUNTER, "beastmastery", 253 },
    { CLASS_HUNTER, "mm", 254 }, { CLASS_HUNTER, "marks", 254 }, { CLASS_HUNTER, "marksman", 254 }, { CLASS_HUNTER, "marksmanship", 254 },
    { CLASS_HUNTER, "surv", 255 }, { CLASS_HUNTER, "survival", 255 },
    { CLASS_ROGUE, "assa", 259 }, { CLASS_ROGUE, "assassination", 259 }, { CLASS_ROGUE, "combat", 260 }, { CLASS_ROGUE, "outlaw", 260 },
    { CLASS_ROGUE, "sub", 261 }, { CLASS_ROGUE, "subtlety", 261 },
    { CLASS_PRIEST, "disc", 256 }, { CLASS_PRIEST, "discipline", 256 }, { CLASS_PRIEST, "holy", 257 }, { CLASS_PRIEST, "shadow", 258 },
    { CLASS_SHAMAN, "ele", 262 }, { CLASS_SHAMAN, "elemental", 262 }, { CLASS_SHAMAN, "enh", 263 }, { CLASS_SHAMAN, "enhance", 263 },
    { CLASS_SHAMAN, "enhancement", 263 }, { CLASS_SHAMAN, "resto", 264 }, { CLASS_SHAMAN, "restoration", 264 },
    { CLASS_MAGE, "arcane", 62 }, { CLASS_MAGE, "fire", 63 }, { CLASS_MAGE, "frost", 64 },
    { CLASS_WARLOCK, "affli", 265 }, { CLASS_WARLOCK, "aff", 265 }, { CLASS_WARLOCK, "affliction", 265 },
    { CLASS_WARLOCK, "demo", 266 }, { CLASS_WARLOCK, "demonology", 266 }, { CLASS_WARLOCK, "destro", 267 }, { CLASS_WARLOCK, "destruction", 267 },
    { CLASS_DRUID, "balance", 102 }, { CLASS_DRUID, "boomkin", 102 }, { CLASS_DRUID, "moonkin", 102 }, { CLASS_DRUID, "feral", 103 },
    { CLASS_DRUID, "cat", 103 }, { CLASS_DRUID, "guardian", 104 }, { CLASS_DRUID, "bear", 104 }, { CLASS_DRUID, "resto", 105 },
    { CLASS_DRUID, "restoration", 105 },
};

} // namespace Detail

// `text` is what follows "setspec", any case. Kind::None: nothing the class knows by that word.
inline Word Parse(std::uint8_t cls, std::string_view text)
{
    std::string w;
    for (char c : text)
        if (c != ' ' && c != '\t')
            w.push_back(char(std::tolower(static_cast<unsigned char>(c))));
    if (w.empty())
        return {};
    bool digits = true;
    for (char c : w)
        digits = digits && c >= '0' && c <= '9';
    if (digits)
        return w.size() <= 5 ? Word{ Kind::Spec, std::uint32_t(std::stoul(w)) } : Word{};
    if (w == "tank")
        return { Kind::Tank, 0 };
    if (w == "heal" || w == "healer" || w == "healing" || w == "heals")
        return { Kind::Healer, 0 };
    if (w == "dps" || w == "damage")
        return { Kind::Dps, 0 };
    for (Detail::Name const& n : Detail::kNames)
        if (n.cls == cls && w == n.word)
            return { Kind::Spec, n.spec };
    return {};
}

// The name a player knows a Classic spec by ("Fury", "Holy"); nullptr for an id that is not a Classic class's spec. The 70532
// client data has no ChrSpecialization names, so the DB2 lookup answered "spec 72" (Karrz 2026-10-25).
inline char const* Label(std::uint32_t spec)
{
    switch (spec)
    {
        case 71:  return "Arms";          case 72:  return "Fury";          case 73:  return "Protection";
        case 65:  return "Holy";          case 66:  return "Protection";    case 70:  return "Retribution";
        case 253: return "Beast Mastery"; case 254: return "Marksmanship";  case 255: return "Survival";
        case 259: return "Assassination"; case 260: return "Combat";        case 261: return "Subtlety";
        case 256: return "Discipline";    case 257: return "Holy";          case 258: return "Shadow";
        case 262: return "Elemental";     case 263: return "Enhancement";   case 264: return "Restoration";
        case 62:  return "Arcane";        case 63:  return "Fire";          case 64:  return "Frost";
        case 265: return "Affliction";    case 266: return "Demonology";    case 267: return "Destruction";
        case 102: return "Balance";       case 103: return "Feral";         case 104: return "Guardian";
        case 105: return "Restoration";
        default:  return nullptr;
    }
}

} // namespace Playerbot::V2::SpecWords
