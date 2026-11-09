// ZonePicker - Level-bracket → destination zone for distribution-spawned
// bots. Distributes bots ACROSS the world instead of clustering them
// in faction capitals. Each pick returns:
//   - map id + coord (where to land)
//   - zone name (logging)
//   - starter quest id (0 if no expansion gate)
//
// The starter quest field is the expansion-gate quest: accepting it
// unblocks the rest of the chain (e.g. Hellfire Peninsula's "Through
// the Dark Portal" gate). Without acceptance, bots arrive at the
// expansion hub but the local quest givers' first quests are hidden
// behind the chain prerequisite.

#pragma once

#include "Bot/BotTypes.h"

#include <string>

namespace Playerbot::V2::World {

struct DestinationPick
{
    uint32 map_id      = 0;
    float  x           = 0.0f;
    float  y           = 0.0f;
    float  z           = 0.0f;
    float  o           = 0.0f;
    char const* zone   = "";
    // Expansion-gate quest. 0 = vanilla zone (no gate). Otherwise the
    // quest id the pipeline should accept (Player::AddQuestAndCheckCompletion)
    // before the bot arrives so local-hub quests aren't filtered out by
    // the prerequisite-chain check.
    uint32 starter_quest = 0;
};

// Picks a level-appropriate destination for the given bot. `seed` is
// the bot's character GUID — used for deterministic round-robin over
// zone candidates so we don't dump 200 bots into the same hub.
// Returns a destination with map_id == 0 ONLY if level < 10 (caller
// should keep the bot at its starter zone).
DestinationPick PickDestination(uint8 level, bool alliance, uint64 seed);

// ---- Classic 1.60 (vanilla realm) level pacing ----
// 1.12 level range of a vanilla leveling zone (AreaTable zone id, as
// Player::GetZoneId returns it). False for a zone without a leveling range
// (capitals, instances, battlegrounds, unknown).
bool VanillaZoneLevelRange(uint32 zone_id, uint8& lo, uint8& hi);

// Where a player of `level` goes on a 1.12 realm, in words for the language model, so its lines and answers name real places for
// that level ("Westfall" was told to a level 60, "Blackfathom Deeps at 60", PC 2026-11-09): the leveling zones whose range holds the
// level (no starter zones above 10), the dungeons of the usual party range (Bot/ChannelTalk.h kDungeons), and at 58+ what people do
// at the level cap. World thread or any thread (DB2 reads only).
std::string VanillaLevelPlaces(uint8 level);

// True for the six vanilla faction capitals (Stormwind, Ironforge, Darnassus,
// Orgrimmar, Thunder Bluff, Undercity): no leveling content of their own.
bool IsVanillaCapitalZone(uint32 zone_id);

// The vanilla town (flight master ground point, see kVanillaTowns) a bot of
// this level and faction should level at next: towns whose 1.12 range holds
// the level, preferring ones on `current_map` (no boat / zeppelin trip when
// the bot's own continent has a fitting zone), round-robin by `seed`.
// map_id == 0 && zone == "" when no town fits (level < 10, or no node
// resolved). Vanilla realm only.
DestinationPick PickVanillaLevelingTown(uint8 level, bool alliance, uint64 seed, uint32 current_map);

// Moves a destination off the town's single flight master point onto a quest hub of the same zone that suits the bot, with a
// per-bot offset (SpreadSpot.h). Needs the destination map to be loaded to read the ground; otherwise (the first bot of a zone)
// the destination stays as it is. Vanilla realm placement only.
DestinationPick SpreadWithinZone(DestinationPick d, uint8 level, bool alliance, uint64 seed, bool allow_grind = false);

// A place on `map` within `max_yards` of (x, y) where the hostile ordinary mobs average within two levels of `level` (GrindSpot.h). The spot is
// the bot's own: chosen by a hash of `seed` over the suitable spots in reach, so it does not change as the bot moves and a crowd spreads
// over the spots. False when none is in reach.
bool FindGrindSpotNear(bool alliance, uint32 map, float x, float y, uint32 level, uint64 seed, float max_yards, float& ox, float& oy, float& oz);

} // namespace Playerbot::V2::World
