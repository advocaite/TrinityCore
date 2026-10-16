// Warsong Gulch lanes, hand-recorded waypoint chains.
//
// Source: Karrz1/wow-1.12 branch raid10-revamp, commit bcc8b9fe70c42a8fa5d50fa83a472da9e9d19546, src/game/PlayerBots/playerbot/
// strategy/actions/BattleGroundTactics.cpp, BGTactics::wsgPaths() ("old wsg waypoints", the ike3 / cmangos playerbots lineage, GPL).
// Map 489 coordinates, the same map as BgWaypointData.inc. Only the points are taken; the choice among them is ours (per-bot lanes).
// Checked offline against the recorded graph (BgWaypointData.inc): all but the two south mid-field points lie within ~20 y of a
// recorded node and at the same height. The south crossing, (1270,1383) and (1270,1399), exists only in this data. Walkability
// of the legs on the navmesh is verified on the server (move_to paths each leg).
//
// East = a bot going toward larger x (Horde side to Alliance side), west = the reverse.

namespace WsgLaneData
{
    using P = LanePoint;
    // out of the Horde base, eastbound
    inline std::vector<P> const kExitTunnelE   = { {1006.59f, 1450.44f, 335.72f}, {1125.78f, 1460.06f, 316.00f} };
    inline std::vector<P> const kExitRidgeE    = { {985.94f, 1423.26f, 345.42f}, {1055.18f, 1396.97f, 339.36f}, {1076.78f, 1396.00f, 324.00f, true}, {1125.78f, 1396.00f, 316.57f} };
    inline std::vector<P> const kExitRampE     = { {985.94f, 1423.26f, 345.42f}, {1031.76f, 1454.52f, 343.34f}, {1051.30f, 1494.92f, 342.04f}, {1050.09f, 1538.05f, 332.46f}, {1098.72f, 1535.62f, 315.73f}, {1239.09f, 1541.41f, 306.49f} };
    // mid field, eastbound: after the tunnel / the north ramp the middle, after the ridge the south
    inline std::vector<P> const kMidMiddleE    = { {1227.45f, 1476.24f, 307.48f} };
    inline std::vector<P> const kMidSouthE     = { {1269.96f, 1382.66f, 308.55f} };
    // into the Alliance base, eastbound: the tunnel or the south ramp
    inline std::vector<P> const kEnterTunnelE  = { {1351.76f, 1462.37f, 323.67f}, {1449.57f, 1470.70f, 342.68f}, {1538.39f, 1480.90f, 352.58f} };
    inline std::vector<P> const kEnterRampE    = { {1360.09f, 1393.45f, 326.18f}, {1399.36f, 1405.11f, 341.48f}, {1417.10f, 1459.55f, 349.59f}, {1505.05f, 1493.79f, 352.02f}, {1538.39f, 1480.90f, 352.58f} };
    // out of the Alliance base, westbound
    inline std::vector<P> const kExitTunnelW   = { {1449.57f, 1470.70f, 342.68f}, {1443.76f, 1459.58f, 342.12f}, {1380.76f, 1457.58f, 329.12f} };
    inline std::vector<P> const kExitRidgeW    = { {1500.05f, 1493.79f, 352.02f}, {1459.49f, 1494.18f, 351.57f}, {1422.11f, 1529.85f, 342.00f}, {1398.70f, 1534.60f, 322.50f, true}, {1344.33f, 1514.92f, 319.08f} };
    inline std::vector<P> const kExitRampW     = { {1500.05f, 1493.79f, 352.02f}, {1459.49f, 1494.18f, 351.57f}, {1417.10f, 1459.55f, 349.59f}, {1399.36f, 1405.11f, 341.48f}, {1356.09f, 1393.45f, 326.18f}, {1269.96f, 1398.66f, 309.95f} };
    inline std::vector<P> const kMidMiddleW    = { {1227.45f, 1476.24f, 307.48f} };
    inline std::vector<P> const kMidNorthW     = { {1239.09f, 1541.41f, 306.49f} };
    // into the Horde base, westbound: the tunnel or the north ramp
    inline std::vector<P> const kEnterTunnelW  = { {1127.78f, 1462.06f, 315.70f}, {1006.59f, 1450.44f, 335.72f}, {919.16f, 1433.87f, 345.90f} };
    inline std::vector<P> const kEnterRampW    = { {1096.72f, 1535.62f, 315.73f}, {1070.09f, 1538.05f, 332.46f}, {1050.09f, 1538.05f, 332.46f}, {1031.76f, 1454.52f, 343.34f}, {985.94f, 1423.26f, 345.42f}, {919.16f, 1433.87f, 345.90f} };

    inline std::vector<P> Join(std::initializer_list<std::vector<P> const*> parts)
    {
        std::vector<P> out;
        for (auto const* p : parts) out.insert(out.end(), p->begin(), p->end());
        return out;
    }
}

// The lanes of one direction. A lane's waypoint chain is built once (static storage, referenced by pointer).
inline std::vector<Lane> BuildWsgLanes(bool east, bool flank_only)
{
    using namespace WsgLaneData;
    static std::vector<LanePoint> const chains[2][6] = {
        { // westbound
            Join({&kExitTunnelW, &kMidMiddleW, &kEnterTunnelW}), Join({&kExitTunnelW, &kMidMiddleW, &kEnterRampW}),
            Join({&kExitRidgeW,  &kMidNorthW,  &kEnterTunnelW}), Join({&kExitRidgeW,  &kMidNorthW,  &kEnterRampW}),
            Join({&kExitRampW,   &kEnterTunnelW}),               Join({&kExitRampW,   &kEnterRampW}) },
        { // eastbound
            Join({&kExitTunnelE, &kMidMiddleE, &kEnterTunnelE}), Join({&kExitTunnelE, &kMidMiddleE, &kEnterRampE}),
            Join({&kExitRidgeE,  &kMidSouthE,  &kEnterTunnelE}), Join({&kExitRidgeE,  &kMidSouthE,  &kEnterRampE}),
            Join({&kExitRampE,   &kEnterTunnelE}),               Join({&kExitRampE,   &kEnterRampE}) },
    };
    auto const& c = chains[east ? 1 : 0];
    std::vector<Lane> lanes;
    // The ridge chains (2, 3) leave the graveyard ridge by a drop of 15..20 y that has no navmesh link. The recorded landing points
    // are marked `drop` in the data above: the bot walks to the ledge and then crosses with the committed one-way drop move
    // (API::move_to drop=true: a low jump to the landing, fall damage on arrival). The original bots cross it with a jump move too
    // (their own code marks that lane "BUGGED"; ours does not path it, so the NOPATH that made it unusable cannot occur).
    char const* const names[6] = { "tunnel>tunnel", "tunnel>ramp", "ridge>tunnel", "ridge>ramp", "ramp>tunnel", "ramp>ramp" };
    bool const flank[6]        = { false, false, true, true, true, true };      // a base tunnel is not a flank
    // Weights (playtest, WSG 2026-10-09 playtest: bots ran the long way round the ramp where players drop off the ridge): the ridge
    // lanes carry the drop and are the players' shortcut, so they weigh most. The owner, same playtest: "still running up and down the middle":
    // the tunnel lanes and the two plain middle lines are the middle, about a quarter of the picks now; the ramp-out lanes end on the side of
    // the field their ramp leads to and no longer cut back through the middle (eastbound they used to go on to the middle point).
    int const weight[6] = { 6, 6, 34, 34, 10, 10 };
    for (int i = 0; i < 6; ++i)
        if (!flank_only || flank[i])
            lanes.push_back({nullptr, &c[i], weight[i], flank[i], names[i]});
    if (!flank_only)
    {
        lanes.push_back({nullptr, nullptr, 4, false, "plain"});                                       // the shortest recorded route
        lanes.push_back({"HordeTunnel_to_AllianceTunnel_2", nullptr, 4, false, "mid2"});             // the second recording of the mid line
    }
    return lanes;
}
