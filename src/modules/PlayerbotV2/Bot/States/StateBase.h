// StateBase - Dispatch interface for top-level states. CONTRACTS.md §2.7.
//
// Each State_*.cpp exports one DispatchXxx free function with this signature.
// BotAI::tick selects which to call based on current BotState. State files
// are stateless — all state lives in BotAI / BotSnapshot / BotPersonality.
//
// Note 2026-05-21: BotEventInbox parameter removed. The inbox had zero
// consumers and the snapshot covers every actual rule need.

#pragma once

#include "Bot/BotTypes.h"
#include "ObjectGuid.h"

namespace Playerbot {

class BotAI;
class BotSnapshotView;
class GroupSnapshotView;
class BotIntentEmitter;

namespace States {

// Primary states (mutually exclusive)
void DispatchLoggingIn  (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchLoggingOut (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchIdle       (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchTravelling (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchQuesting   (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchInCombat   (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchLooting    (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchDead       (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchResurrecting(BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);

// Cross-cutting layers (compose over a primary state)
void DispatchInGroup    (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchInInstance (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchAtVendor   (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchAtMailbox  (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchAtAh       (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);
void DispatchDecorating (BotAI&, BotSnapshotView, GroupSnapshotView, BotIntentEmitter&);

// Group threat discipline (State_InCombat.cpp; port of Karrz's wow-1.12
// MCRaidHoldDamage). True while this NON-TANK grouped bot must not damage
// `target`: the tank has not built its opening lead yet (bounded wait) or
// the bot is at/over PlayerbotV2.Threat.MaxRatio of the tank's threat.
// Always false for tanks, solo bots, BGs, and mobs hitting a non-tank member.
// `g` (optional): the group, so the skull / cross raid mark counts as the
// owner's explicit target (the unpulled-mob hold then lapses after 4s).
bool ThreatHoldActive(BotAI& ai, BotSnapshotView const& s, ObjectGuid target,
                      GroupSnapshotView const* g = nullptr);

// The owner said Stay / Hold and no mob is on this bot: it keeps its spot. Every mover and target picker that would walk the bot
// (melee chase, skull pick, stack / melee-behind positioning) asks this first; a mob that attacks the bot lifts it.
bool HeldByOwner(BotAI const& ai, BotSnapshotView const& s);

} // namespace States
} // namespace Playerbot
