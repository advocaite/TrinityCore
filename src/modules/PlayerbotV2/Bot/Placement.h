// Placement - the ForeverBots Positions tab: chosen bots stand in a shape at one spot and stay there (Karrz 2026-11-03: "select the
// formation, move specific bots, set them in a formation: close, circle, far apart, line, spear, arrow").
//
// Pure geometry, no server types, so tests/game/ClassicPlacement.cpp checks it without a realm. The caller sorts the bots (tanks
// first, then melee, ranged, healers; BotCommandParser::PlaceBots) and turns each local slot into a world spot.
//
// Local frame: `forward` is the way the anchor faces (the player's facing for "here", from the target toward the player for
// "target"), `left` is to its left. Every shape is centred on the anchor. Slot 0 is the front one (the tip of a spear or arrow),
// so the tanks lead.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>

namespace Playerbot::Placement {

enum class Shape : uint8_t
{
    Close = 0,   // a tight clump round the spot
    Circle,      // one ring round the spot
    Arc,         // the front half of a ring (round a boss, or ahead of you)
    Spread,      // far apart: a wide grid, for spread-out fights
    Line,        // side by side across the facing
    Column,      // single file along the facing
    Spear,       // a point, a head of three, then a shaft two wide
    Arrow,       // a V pointing forward
};

constexpr uint32_t kMaxBots = 40;

struct Slot
{
    float forward = 0.0f;
    float left = 0.0f;
};

// "close", "circle", "arc", "spread" ("far", "apart", "far apart"), "line", "column" ("file"), "spear", "arrow" ("wedge", "v").
inline bool ParseShape(std::string_view w, Shape& out)
{
    if (w == "close" || w == "clump" || w == "stack")             { out = Shape::Close;  return true; }
    if (w == "circle" || w == "ring")                             { out = Shape::Circle; return true; }
    if (w == "arc" || w == "half" || w == "halfcircle")           { out = Shape::Arc;    return true; }
    if (w == "spread" || w == "far" || w == "apart" || w == "farapart" || w == "grid") { out = Shape::Spread; return true; }
    if (w == "line" || w == "row")                                { out = Shape::Line;   return true; }
    if (w == "column" || w == "file" || w == "queue")             { out = Shape::Column; return true; }
    if (w == "spear")                                             { out = Shape::Spear;  return true; }
    if (w == "arrow" || w == "wedge" || w == "v")                 { out = Shape::Arrow;  return true; }
    return false;
}

inline char const* ShapeName(Shape s)
{
    switch (s)
    {
        case Shape::Close:  return "close";
        case Shape::Circle: return "circle";
        case Shape::Arc:    return "arc";
        case Shape::Spread: return "spread";
        case Shape::Line:   return "line";
        case Shape::Column: return "column";
        case Shape::Spear:  return "spear";
        case Shape::Arrow:  return "arrow";
    }
    return "?";
}

// Gap words: "tight" 2.5 y, "normal" ("close" as a gap word too) 4 y, "wide" 8 y. Empty = normal.
inline bool ParseGap(std::string_view w, float& yards)
{
    if (w.empty() || w == "normal" || w == "medium" || w == "close") { yards = 4.0f; return true; }
    if (w == "tight" || w == "small")                                { yards = 2.5f; return true; }
    if (w == "wide" || w == "big" || w == "spaced")                  { yards = 8.0f; return true; }
    return false;
}

namespace Detail {
constexpr float kPi = 3.14159265358979f;

// Move the slots so the middle of their front-to-back and side-to-side spread is the anchor.
inline void Centre(std::vector<Slot>& v)
{
    if (v.empty()) return;
    float fmin = v[0].forward, fmax = fmin, lmin = v[0].left, lmax = lmin;
    for (Slot const& s : v)
    {
        fmin = std::min(fmin, s.forward); fmax = std::max(fmax, s.forward);
        lmin = std::min(lmin, s.left);    lmax = std::max(lmax, s.left);
    }
    float const fc = (fmin + fmax) * 0.5f, lc = (lmin + lmax) * 0.5f;
    for (Slot& s : v) { s.forward -= fc; s.left -= lc; }
}

// Rows of up to `width` side by side, `gap` apart, the first row in front; each row centred.
inline void Rows(std::vector<Slot>& v, uint32_t n, uint32_t width, float gap, float row_gap)
{
    for (uint32_t i = 0; i < n; ++i)
    {
        uint32_t const row = i / width, start = row * width;
        uint32_t const in_row = std::min(width, n - start);
        Slot s;
        s.forward = -float(row) * row_gap;
        s.left = (float(i - start) - float(in_row - 1) * 0.5f) * gap;
        v.push_back(s);
    }
}
}  // namespace Detail

// The ring radius a shape uses for n bots `gap` apart: never under `min_r`, never over 35 y (40 bots on a wide gap squeeze in).
inline float RingRadius(uint32_t n, float gap, float span_rad, float min_r)
{
    float const r = float(n) * gap / span_rad;
    return std::clamp(r, min_r, 35.0f);
}

// n slots for `shape`, `gap` yards between neighbours. `min_radius`: the least ring radius for Circle / Arc (a boss's size when
// placing round a target). Front slot first.
inline std::vector<Slot> Slots(Shape shape, uint32_t n, float gap, float min_radius = 3.0f)
{
    using namespace Detail;
    std::vector<Slot> v;
    if (n == 0) return v;
    n = std::min(n, kMaxBots);
    gap = std::clamp(gap, 1.0f, 20.0f);
    v.reserve(n);
    switch (shape)
    {
        case Shape::Close:
        {
            // Rings of 1, 6, 12, 18... round the middle, a little more than half a gap apart.
            float const g = std::max(1.5f, gap * 0.6f);
            v.push_back(Slot{});
            for (uint32_t ring = 1; v.size() < n; ++ring)
            {
                uint32_t const cap = 6 * ring;
                uint32_t const take = std::min<uint32_t>(cap, n - uint32_t(v.size()));
                for (uint32_t k = 0; k < take; ++k)
                {
                    float const a = 2.0f * kPi * float(k) / float(take) + (ring % 2 ? 0.0f : kPi / float(cap));
                    v.push_back(Slot{ std::cos(a) * g * float(ring), std::sin(a) * g * float(ring) });
                }
            }
            return v;
        }
        case Shape::Circle:
        {
            if (n == 1) { v.push_back(Slot{ std::max(min_radius, 4.0f), 0.0f }); return v; }
            float const r = RingRadius(n, gap, 2.0f * kPi, std::max(min_radius, 3.0f));
            for (uint32_t k = 0; k < n; ++k)
            {
                float const a = 2.0f * kPi * float(k) / float(n);   // slot 0 straight ahead
                v.push_back(Slot{ std::cos(a) * r, std::sin(a) * r });
            }
            return v;
        }
        case Shape::Arc:
        {
            // From the left end round the front to the right end (half a circle), the front slot first.
            if (n == 1) { v.push_back(Slot{ std::max(min_radius, 4.0f), 0.0f }); return v; }
            float const r = RingRadius(n, gap, kPi, std::max(min_radius, 4.0f));
            std::vector<float> angles;
            for (uint32_t k = 0; k < n; ++k)
                angles.push_back(-kPi * 0.5f + kPi * float(k) / float(n - 1));
            // the ones nearest the front first, so the tanks get the middle of the arc
            std::stable_sort(angles.begin(), angles.end(), [](float a, float b) { return std::fabs(a) < std::fabs(b) - 1e-4f; });
            for (float a : angles)
                v.push_back(Slot{ std::cos(a) * r, std::sin(a) * r });
            return v;
        }
        case Shape::Spread:
        {
            // A square-ish grid, each bot 2.5 gaps from the next (10 y on a normal gap, 6 y tight, 12 y wide at most so 40 bots
            // still fit a boss room), so nobody shares a splash.
            uint32_t const width = std::max<uint32_t>(1u, uint32_t(std::ceil(std::sqrt(float(n)))));
            float const cell = std::clamp(gap * 2.5f, 6.0f, 12.0f);
            Rows(v, n, width, cell, cell);
            Centre(v);
            return v;
        }
        case Shape::Line:
        {
            // One row up to 10, then a second row behind it.
            Rows(v, n, 10u, gap, gap);
            Centre(v);
            return v;
        }
        case Shape::Column:
        {
            // Single file up to 10, then two abreast, three beyond 20.
            uint32_t const lanes = n <= 10 ? 1u : n <= 20 ? 2u : 3u;
            Rows(v, n, lanes, gap, gap);
            Centre(v);
            return v;
        }
        case Shape::Spear:
        {
            // The point, then the head (three wide), then a shaft behind it: two wide, three past 12 bots, four past 24.
            uint32_t const shaft = n <= 12 ? 2u : n <= 24 ? 3u : 4u;
            v.push_back(Slot{ 0.0f, 0.0f });
            for (uint32_t k = 0; k < 3 && v.size() < n; ++k)
                v.push_back(Slot{ -gap, (float(k) - 1.0f) * gap });
            for (uint32_t row = 2; v.size() < n; ++row)
                for (uint32_t k = 0; k < shaft && v.size() < n; ++k)
                    v.push_back(Slot{ -gap * float(row), (float(k) - float(shaft - 1) * 0.5f) * gap * 0.9f });
            Centre(v);
            return v;
        }
        case Shape::Arrow:
        {
            // The tip, then the two arms going back at about 45 degrees; past 9 bots a second V fills in behind the first.
            constexpr uint32_t kPerV = 9;
            for (uint32_t i = 0; i < n; ++i)
            {
                uint32_t const layer = i / kPerV, k = i % kPerV;
                float const back = float(layer) * gap * 1.6f;
                if (k == 0) { v.push_back(Slot{ -back, 0.0f }); continue; }
                float const j = float((k + 1) / 2);
                float const side = (k % 2) ? 1.0f : -1.0f;
                v.push_back(Slot{ -back - j * gap * 0.75f, side * j * gap * 0.75f });
            }
            Centre(v);
            return v;
        }
    }
    return v;
}

// Move the whole shape forward so its nearest slot is at least `clear` yards in front of the anchor (side offsets are kept): "Place
// here" must never put a bot on the player's own spot, and a shape at a target must not stand inside it (owner report 2026-11-04: with one
// bot every shape was its middle slot, his own spot, and the bot just ran onto him).
inline void PushAhead(std::vector<Slot>& v, float clear)
{
    if (v.empty()) return;
    float fmin = v[0].forward;
    for (Slot const& s : v) fmin = std::min(fmin, s.forward);
    if (fmin >= clear) return;
    float const d = clear - fmin;
    for (Slot& s : v) s.forward += d;
}

// The shapes that go round the anchor (a ring or a half ring) rather than in front of it.
inline bool RingShape(Shape s) { return s == Shape::Circle || s == Shape::Arc; }

// A local slot turned into a world spot round the anchor (ax, ay) facing `o` (radians).
inline void ToWorld(Slot const& s, float ax, float ay, float o, float& x, float& y)
{
    float const c = std::cos(o), sn = std::sin(o);
    x = ax + c * s.forward - sn * s.left;
    y = ay + sn * s.forward + c * s.left;
}

}  // namespace Playerbot::Placement
