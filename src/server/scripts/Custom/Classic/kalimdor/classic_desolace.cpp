/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// Classic 1.60 port of VMaNGOS src/scripts/kalimdor/desolace/desolace.cpp (ScriptDev2 lineage, GPL-2)
// Ported: go_hand_of_iruxos_crystal (5381), npc_melizza_brimbuzzle (6132), npc_dalinda_malem (1440),
//         npc_cork_gizelton (Gizelton Caravan, 5821 / 5943)

#include "ScriptMgr.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "Group.h"
#include "Log.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "QuestDef.h"
#include "ScriptedCreature.h"
#include "ScriptedEscortAI.h"
#include "TemporarySummon.h"
#include "classic_script_text.h"

namespace
{
struct ClassicEscortPoint
{
    float X, Y, Z, O;
    uint32 WaitMs;
};

// VMaNGOS npc_escortAI reads its path from world.script_waypoint (by creature entry). TC EscortAI needs the path
// supplied by the script, so the VMaNGOS rows are embedded here (node ids are 0-based and contiguous, as TC requires).
template <std::size_t N, typename RunPredicate>
void LoadClassicEscortPath(EscortAI* ai, ClassicEscortPoint const (&points)[N], RunPredicate isRunning)
{
    ai->ResetPath();
    for (uint32 i = 0; i < N; ++i)
    {
        Optional<Milliseconds> waitTime;
        if (points[i].WaitMs)
            waitTime = Milliseconds(points[i].WaitMs);
        ai->AddWaypoint(i, points[i].X, points[i].Y, points[i].Z, points[i].O, waitTime, isRunning(i));
    }
}

// VMaNGOS script_waypoint entry 12277 (pointid 1..24, stored 0-based here)
ClassicEscortPoint const EscortPath12277[] =
{
    { -1154.87f, 2708.16f, 111.123f, 2.5964f, 1000 },  // 1: SAY_MELIZZA_START
    { -1162.62f, 2712.86f, 111.549f, 2.5964f, 0 },  // 2
    { -1183.37f, 2709.45f, 111.601f, 3.3045f, 0 },  // 3
    { -1245.09f, 2676.43f, 111.572f, 3.6328f, 0 },  // 4
    { -1260.54f, 2672.48f, 111.55f, 3.3919f, 0 },  // 5
    { -1272.71f, 2666.38f, 111.555f, 3.6062f, 0 },  // 6
    { -1342.95f, 2580.82f, 111.557f, 4.025f, 0 },  // 7
    { -1362.24f, 2561.74f, 110.848f, 3.9215f, 0 },  // 8
    { -1376.56f, 2514.06f, 95.6146f, 4.4206f, 0 },  // 9
    { -1379.06f, 2510.88f, 93.3256f, 4.0461f, 0 },  // 10
    { -1383.14f, 2489.17f, 89.009f, 4.5266f, 0 },  // 11
    { -1395.34f, 2426.15f, 88.6607f, 4.5212f, 0 },  // 12: SAY_MELIZZA_FINISH
    { -1366.23f, 2317.17f, 91.8086f, 4.9734f, 0 },  // 13
    { -1353.81f, 2213.52f, 90.726f, 4.8316f, 0 },  // 14
    { -1354.19f, 2208.28f, 88.7386f, 4.64f, 0 },  // 15
    { -1354.59f, 2193.77f, 77.6702f, 4.6848f, 0 },  // 16
    { -1367.62f, 2160.64f, 67.1482f, 4.3377f, 0 },  // 17
    { -1379.44f, 2132.77f, 64.1326f, 4.3113f, 0 },  // 18
    { -1404.81f, 2088.68f, 61.8162f, 4.1902f, 0 },  // 19: SAY_MELIZZA_1
    { -1417.15f, 2082.65f, 62.4112f, 3.5961f, 0 },  // 20
    { -1423.28f, 2074.19f, 62.2046f, 4.0854f, 0 },  // 21
    { -1432.99f, 2070.56f, 61.7811f, 3.4993f, 0 },  // 22
    { -1469.27f, 2078.68f, 63.1141f, 2.9214f, 0 },  // 23
    { -1507.21f, 2115.12f, 62.3578f, 2.3764f, 0 },  // 24
};
// VMaNGOS script_waypoint entry 5644 (pointid 1..19, stored 0-based here)
ClassicEscortPoint const EscortPath5644[] =
{
    { -339.679f, 1752.04f, 139.482f, 5.2727f, 0 },  // 1
    { -328.957f, 1734.95f, 139.327f, 5.2727f, 0 },  // 2
    { -338.29f, 1731.36f, 139.327f, 3.5088f, 0 },  // 3
    { -350.747f, 1731.12f, 139.338f, 3.1609f, 0 },  // 4
    { -365.064f, 1739.04f, 139.376f, 2.6363f, 0 },  // 5
    { -371.105f, 1746.03f, 139.374f, 2.2835f, 0 },  // 6
    { -383.141f, 1738.62f, 138.93f, 3.6934f, 0 },  // 7
    { -390.445f, 1733.98f, 136.353f, 3.7075f, 0 },  // 8
    { -401.368f, 1726.77f, 131.071f, 3.725f, 0 },  // 9
    { -416.016f, 1721.19f, 129.807f, 3.5056f, 0 },  // 10
    { -437.139f, 1709.82f, 126.342f, 3.6354f, 0 },  // 11
    { -455.83f, 1695.61f, 119.305f, 3.7916f, 0 },  // 12
    { -459.862f, 1687.92f, 116.059f, 4.2295f, 0 },  // 13
    { -463.565f, 1679.1f, 111.653f, 4.3149f, 0 },  // 14
    { -461.485f, 1670.94f, 109.033f, 4.962f, 0 },  // 15
    { -471.786f, 1647.34f, 102.862f, 4.3008f, 0 },  // 16
    { -477.146f, 1625.69f, 98.342f, 4.4697f, 0 },  // 17
    { -475.815f, 1615.81f, 97.07f, 4.8463f, 0 },  // 18
    { -474.329f, 1590.01f, 94.4982f, 4.7699f, 0 },  // 19
};
// VMaNGOS script_waypoint entry 11625 (pointid 0..281, stored 0-based here)
ClassicEscortPoint const EscortPath11625[] =
{
    { -1888.72f, 2458.89f, 59.8224f, 4.4645f, 0 },  // 0
    { -1895.4f, 2432.5f, 59.8224f, 4.4645f, 0 },  // 1
    { -1899.03f, 2418.98f, 59.8224f, 4.4501f, 0 },  // 2
    { -1899.68f, 2393.35f, 59.8224f, 4.687f, 0 },  // 3
    { -1892.03f, 2375.61f, 59.9178f, 5.1195f, 0 },  // 4
    { -1885.29f, 2346.94f, 59.8216f, 4.9433f, 0 },  // 5
    { -1881.12f, 2339.05f, 59.8216f, 5.1986f, 0 },  // 6
    { -1864.4f, 2316.59f, 59.8226f, 5.3523f, 0 },  // 7
    { -1852.79f, 2300.12f, 59.8226f, 5.3264f, 0 },  // 8
    { -1847.08f, 2287.99f, 59.8226f, 5.1524f, 0 },  // 9
    { -1844.34f, 2264.2f, 59.8226f, 4.8271f, 0 },  // 10
    { -1842.38f, 2243.22f, 59.8226f, 4.8055f, 0 },  // 11
    { -1840.11f, 2237.75f, 59.8226f, 5.1058f, 0 },  // 12
    { -1830.64f, 2226.2f, 59.8226f, 5.3992f, 0 },  // 13
    { -1810.22f, 2209.43f, 59.8226f, 5.5956f, 0 },  // 14
    { -1802.8f, 2194.02f, 59.8226f, 5.1611f, 0 },  // 15
    { -1800.41f, 2180.08f, 59.8226f, 4.8822f, 0 },  // 16
    { -1800.26f, 2166.08f, 60.1822f, 4.7231f, 0 },  // 17
    { -1801.62f, 2148.85f, 62.344f, 4.6336f, 0 },  // 18
    { -1801.15f, 2134.86f, 63.1766f, 4.746f, 0 },  // 19
    { -1802.26f, 2110.11f, 63.6737f, 4.6676f, 0 },  // 20
    { -1805.87f, 2096.6f, 63.1784f, 4.4513f, 0 },  // 21
    { -1809.25f, 2083.01f, 63.0772f, 4.4686f, 0 },  // 22
    { -1812.63f, 2069.43f, 63.043f, 4.4685f, 0 },  // 23
    { -1816.72f, 2056.04f, 61.8496f, 4.4159f, 0 },  // 24
    { -1822.89f, 2032.23f, 60.6524f, 4.4588f, 0 },  // 25
    { -1822.76f, 2027.39f, 60.3783f, 4.7392f, 0 },  // 26
    { -1815.56f, 2003.46f, 59.4022f, 5.0047f, 0 },  // 27
    { -1814.41f, 1983.18f, 58.9549f, 4.769f, 0 },  // 28
    { -1811.8f, 1967.01f, 59.4735f, 4.8724f, 0 },  // 29
    { -1803.12f, 1951.78f, 60.7154f, 5.2304f, 0 },  // 30
    { -1793.24f, 1941.87f, 60.8439f, 5.4963f, 0 },  // 31
    { -1775.92f, 1926.82f, 59.3033f, 5.5678f, 0 },  // 32
    { -1759.93f, 1918.92f, 58.9613f, 5.8243f, 0 },  // 33
    { -1751.9f, 1917.2f, 59.0003f, 6.0722f, 0 },  // 34
    { -1737.91f, 1917.04f, 59.0673f, 6.2717f, 0 },  // 35
    { -1712.18f, 1914.85f, 60.4394f, 6.1983f, 0 },  // 36
    { -1701.72f, 1911.02f, 61.0949f, 5.9322f, 0 },  // 37
    { -1694.06f, 1904.03f, 61.03f, 5.5435f, 0 },  // 38
    { -1687.1f, 1886.34f, 59.7501f, 5.0872f, 0 },  // 39
    { -1684.12f, 1872.66f, 59.0354f, 4.9269f, 0 },  // 40
    { -1673.14f, 1845.28f, 58.9273f, 5.0938f, 0 },  // 41
    { -1657.63f, 1821.97f, 58.9273f, 5.2995f, 0 },  // 42
    { -1649.83f, 1810.34f, 58.9273f, 5.3032f, 0 },  // 43
    { -1634.24f, 1787.08f, 58.9252f, 5.3029f, 0 },  // 44
    { -1626.45f, 1775.45f, 58.9252f, 5.3026f, 0 },  // 45
    { -1605.77f, 1750.66f, 58.9256f, 5.4076f, 0 },  // 46
    { -1594.91f, 1741.83f, 58.9256f, 5.6005f, 0 },  // 47
    { -1573.31f, 1724.02f, 58.9256f, 5.5937f, 0 },  // 48
    { -1553.4f, 1704.35f, 58.9256f, 5.5039f, 0 },  // 49
    { -1543.67f, 1694.29f, 58.9256f, 5.4811f, 0 },  // 50
    { -1523.39f, 1674.99f, 58.9256f, 5.5225f, 0 },  // 51
    { -1505.1f, 1659.98f, 58.9256f, 5.596f, 0 },  // 52
    { -1489.89f, 1652.47f, 58.9256f, 5.8245f, 0 },  // 53
    { -1460.15f, 1634.27f, 58.9256f, 5.734f, 0 },  // 54
    { -1453.16f, 1621.35f, 58.9256f, 5.2083f, 0 },  // 55
    { -1446.87f, 1598.31f, 58.9256f, 4.9789f, 0 },  // 56
    { -1440.81f, 1573.28f, 58.9256f, 4.9499f, 0 },  // 57
    { -1445.9f, 1553.99f, 58.9256f, 4.4544f, 0 },  // 58
    { -1451.91f, 1541.35f, 58.9256f, 4.2686f, 0 },  // 59
    { -1458.46f, 1528.97f, 58.9256f, 4.2257f, 0 },  // 60
    { -1471.62f, 1504.26f, 58.9256f, 4.223f, 0 },  // 61
    { -1478.08f, 1491.84f, 58.9256f, 4.2328f, 0 },  // 62
    { -1490.08f, 1466.54f, 58.9256f, 4.2695f, 0 },  // 63
    { -1491.71f, 1455.14f, 58.9291f, 4.5704f, 0 },  // 64
    { -1488.22f, 1427.36f, 58.9348f, 4.8374f, 0 },  // 65
    { -1486.41f, 1413.48f, 58.9418f, 4.8421f, 0 },  // 66
    { -1487.62f, 1388.44f, 58.9251f, 4.6641f, 0 },  // 67
    { -1491.84f, 1375.08f, 58.9301f, 4.4064f, 0 },  // 68
    { -1502.72f, 1349.31f, 58.9416f, 4.3129f, 0 },  // 69
    { -1508.49f, 1336.58f, 59.525f, 4.2868f, 0 },  // 70
    { -1511.68f, 1327.41f, 60.3754f, 4.3776f, 0 },  // 71
    { -1514.03f, 1314.22f, 62.0185f, 4.5361f, 0 },  // 72
    { -1514.79f, 1300.27f, 64.5471f, 4.658f, 0 },  // 73
    { -1516.1f, 1286.34f, 68.0841f, 4.6186f, 0 },  // 74
    { -1518.52f, 1272.55f, 72.0932f, 4.5387f, 0 },  // 75
    { -1523.17f, 1245.16f, 82.7876f, 4.5442f, 0 },  // 76
    { -1522.5f, 1234.75f, 87.008f, 4.7767f, 0 },  // 77
    { -1517.95f, 1221.51f, 91.5343f, 5.0434f, 0 },  // 78
    { -1511.76f, 1208.2f, 96.7403f, 5.1477f, 0 },  // 79
    { -1501.33f, 1196.53f, 102.475f, 5.4417f, 0 },  // 80
    { -1490.76f, 1188.95f, 106.376f, 5.6611f, 0 },  // 81
    { -1475.92f, 1185.48f, 109.181f, 6.0535f, 0 },  // 82
    { -1452.6f, 1187.95f, 111.422f, 0.1055f, 0 },  // 83
    { -1433.28f, 1193.58f, 111.857f, 0.2836f, 0 },  // 84
    { -1414.55f, 1203.63f, 111.886f, 0.4925f, 0 },  // 85
    { -1388.31f, 1213.37f, 111.599f, 0.3554f, 0 },  // 86
    { -1375.11f, 1218.03f, 111.465f, 0.3394f, 0 },  // 87
    { -1348.49f, 1226.69f, 111.175f, 0.3145f, 0 },  // 88
    { -1319.41f, 1232.27f, 110.201f, 0.1896f, 0 },  // 89
    { -1290.31f, 1231.38f, 109.237f, 6.2526f, 0 },  // 90
    { -1277.59f, 1225.54f, 108.85f, 5.8528f, 0 },  // 91
    { -1264.86f, 1219.71f, 108.452f, 5.8537f, 0 },  // 92
    { -1230.04f, 1204.31f, 104.374f, 5.8668f, 0 },  // 93
    { -1216.17f, 1206.24f, 101.889f, 0.1383f, 0 },  // 94
    { -1202.32f, 1208.28f, 99.7026f, 0.1462f, 0 },  // 95
    { -1188.03f, 1207.66f, 97.2208f, 6.2398f, 0 },  // 96
    { -1170.99f, 1195.93f, 94.5615f, 5.6803f, 0 },  // 97
    { -1155.46f, 1192.16f, 92.4374f, 6.045f, 0 },  // 98
    { -1127.52f, 1190.39f, 89.8358f, 6.2199f, 0 },  // 99
    { -1113.58f, 1189.12f, 89.7403f, 6.1923f, 0 },  // 100
    { -1070.8f, 1186.15f, 89.7403f, 6.2139f, 0 },  // 101
    { -1037.27f, 1183.2f, 89.8006f, 6.1954f, 0 },  // 102
    { -995.58f, 1177.92f, 89.7409f, 6.1572f, 0 },  // 103
    { -981.817f, 1180.48f, 89.8152f, 0.1839f, 0 },  // 104
    { -952.606f, 1181.99f, 89.7313f, 0.0516f, 0 },  // 105
    { -935.445f, 1182.25f, 91.2113f, 0.0151f, 0 },  // 106
    { -921.448f, 1182.53f, 93.1746f, 0.02f, 0 },  // 107
    { -879.467f, 1183.72f, 97.6043f, 0.0283f, 0 },  // 108
    { -858.976f, 1184.22f, 99.0322f, 0.0244f, 0 },  // 109
    { -828.316f, 1180.2f, 99.6657f, 6.1528f, 0 },  // 110
    { -799.811f, 1176.0f, 99.3364f, 6.1369f, 0 },  // 111
    { -757.106f, 1191.47f, 96.9164f, 0.3475f, 0 },  // 112
    { -731.879f, 1208.14f, 92.6956f, 0.5839f, 0 },  // 113
    { -719.12f, 1213.91f, 91.3297f, 0.4247f, 0 },  // 114
    { -706.36f, 1219.67f, 90.2856f, 0.424f, 0 },  // 115
    { -689.935f, 1228.43f, 89.4426f, 0.49f, 0 },  // 116
    { -679.121f, 1237.31f, 89.17f, 0.6875f, 0 },  // 117
    { -661.434f, 1247.28f, 89.17f, 0.5133f, 0 },  // 118
    { -635.655f, 1258.2f, 89.2063f, 0.4007f, 0 },  // 119
    { -614.489f, 1269.64f, 89.1686f, 0.4955f, 0 },  // 120
    { -600.078f, 1274.85f, 89.1238f, 0.3469f, 0 },  // 121
    { -586.268f, 1277.15f, 89.145f, 0.165f, 0 },  // 122
    { -546.297f, 1287.15f, 89.1597f, 0.2451f, 0 },  // 123
    { -541.257f, 1300.21f, 89.1602f, 1.2025f, 0 },  // 124
    { -536.026f, 1313.2f, 89.1314f, 1.188f, 0 },  // 125
    { -525.098f, 1338.97f, 89.1005f, 1.1697f, 0 },  // 126
    { -518.852f, 1356.12f, 89.0827f, 1.2215f, 0 },  // 127
    { -516.879f, 1395.56f, 89.0827f, 1.5208f, 0 },  // 128
    { -518.905f, 1436.25f, 89.0696f, 1.6205f, 0 },  // 129
    { -525.605f, 1446.54f, 88.4907f, 2.148f, 0 },  // 130
    { -543.459f, 1462.9f, 88.3752f, 2.3998f, 0 },  // 131
    { -557.591f, 1471.17f, 88.9477f, 2.6121f, 0 },  // 132
    { -584.698f, 1478.14f, 88.3754f, 2.8899f, 0 },  // 133
    { -598.459f, 1480.72f, 88.3754f, 2.9563f, 0 },  // 134
    { -632.084f, 1491.03f, 88.3754f, 2.8441f, 0 },  // 135
    { -644.249f, 1497.95f, 88.3754f, 2.6244f, 0 },  // 136
    { -660.456f, 1507.56f, 88.3874f, 2.6064f, 0 },  // 137
    { -666.507f, 1504.7f, 89.0746f, 3.5831f, 0 },  // 138
    { -673.795f, 1499.48f, 90.3922f, 3.7631f, 0 },  // 139
    { -692.154f, 1480.43f, 90.5302f, 3.9455f, 0 },  // 140
    { -710.764f, 1470.11f, 91.3034f, 3.6479f, 0 },  // 141
    { -697.048f, 1484.75f, 91.0929f, 0.818f, 0 },  // 142
    { -676.552f, 1497.37f, 90.6505f, 0.5519f, 0 },  // 143
    { -665.209f, 1505.76f, 88.8321f, 0.6369f, 0 },  // 144
    { -657.898f, 1510.94f, 88.3752f, 0.6164f, 0 },  // 145
    { -647.387f, 1501.69f, 88.3752f, 5.5615f, 0 },  // 146
    { -630.597f, 1491.17f, 88.3752f, 5.7235f, 0 },  // 147
    { -603.756f, 1483.26f, 88.3752f, 5.9966f, 0 },  // 148
    { -576.573f, 1476.54f, 88.3752f, 6.0408f, 0 },  // 149
    { -556.868f, 1470.92f, 88.8685f, 6.0054f, 0 },  // 150
    { -547.471f, 1464.89f, 88.3747f, 5.7127f, 0 },  // 151
    { -529.316f, 1449.97f, 88.402f, 5.5953f, 0 },  // 152
    { -517.699f, 1433.75f, 89.0816f, 5.3339f, 0 },  // 153
    { -518.09f, 1405.76f, 89.0816f, 4.6984f, 0 },  // 154
    { -518.149f, 1377.76f, 89.0816f, 4.7103f, 0 },  // 155
    { -521.289f, 1350.76f, 89.0816f, 4.5966f, 0 },  // 156
    { -531.625f, 1324.74f, 89.1339f, 4.3343f, 0 },  // 157
    { -537.391f, 1311.99f, 89.1594f, 4.2877f, 0 },  // 158
    { -551.845f, 1284.12f, 89.1594f, 4.234f, 0 },  // 159
    { -578.054f, 1278.57f, 89.1685f, 3.3503f, 0 },  // 160
    { -591.957f, 1276.92f, 89.1634f, 3.2597f, 0 },  // 161
    { -611.806f, 1271.05f, 89.1694f, 3.4291f, 0 },  // 162
    { -623.928f, 1264.06f, 89.1694f, 3.6647f, 0 },  // 163
    { -653.384f, 1249.74f, 89.1694f, 3.5941f, 0 },  // 164
    { -666.372f, 1244.51f, 89.1694f, 3.5244f, 0 },  // 165
    { -684.6f, 1232.06f, 89.2134f, 3.7408f, 0 },  // 166
    { -694.027f, 1225.67f, 89.6627f, 3.7373f, 0 },  // 167
    { -706.605f, 1219.58f, 90.2981f, 3.5925f, 0 },  // 168
    { -732.184f, 1208.23f, 92.7376f, 3.5592f, 0 },  // 169
    { -738.514f, 1204.75f, 93.8662f, 3.6443f, 0 },  // 170
    { -754.159f, 1193.91f, 96.6195f, 3.7475f, 0 },  // 171
    { -766.62f, 1187.59f, 97.8394f, 3.611f, 0 },  // 172
    { -792.515f, 1177.07f, 98.8327f, 3.5275f, 0 },  // 173
    { -802.533f, 1175.57f, 99.4435f, 3.2902f, 0 },  // 174
    { -821.772f, 1178.84f, 99.6542f, 2.9732f, 0 },  // 175
    { -835.435f, 1181.9f, 99.6662f, 2.9213f, 0 },  // 176
    { -848.98f, 1184.67f, 99.5782f, 2.9399f, 0 },  // 177
    { -861.251f, 1185.3f, 98.8033f, 3.0903f, 0 },  // 178
    { -889.179f, 1183.34f, 96.6117f, 3.2117f, 0 },  // 179
    { -903.158f, 1182.57f, 95.2033f, 3.1966f, 0 },  // 180
    { -931.15f, 1182.17f, 91.8346f, 3.1559f, 0 },  // 181
    { -945.149f, 1182.01f, 89.8612f, 3.153f, 0 },  // 182
    { -959.149f, 1181.92f, 89.7397f, 3.148f, 0 },  // 183
    { -973.149f, 1181.97f, 89.7397f, 3.138f, 0 },  // 184
    { -1001.65f, 1178.06f, 89.7398f, 3.2779f, 0 },  // 185
    { -1011.8f, 1177.4f, 89.7398f, 3.2065f, 0 },  // 186
    { -1033.08f, 1182.29f, 89.7629f, 2.9157f, 0 },  // 187
    { -1073.62f, 1186.33f, 89.7398f, 3.0423f, 0 },  // 188
    { -1101.59f, 1187.56f, 89.7398f, 3.0976f, 0 },  // 189
    { -1129.48f, 1190.01f, 89.8855f, 3.054f, 0 },  // 190
    { -1143.44f, 1191.11f, 91.0344f, 3.063f, 0 },  // 191
    { -1166.85f, 1194.28f, 93.9649f, 3.007f, 0 },  // 192
    { -1184.71f, 1203.56f, 96.6406f, 2.6624f, 0 },  // 193
    { -1201.45f, 1208.2f, 99.5698f, 2.8712f, 0 },  // 194
    { -1225.42f, 1204.68f, 103.502f, 3.2874f, 0 },  // 195
    { -1235.55f, 1206.75f, 105.129f, 2.94f, 0 },  // 196
    { -1261.05f, 1218.25f, 108.207f, 2.7179f, 0 },  // 197
    { -1286.64f, 1229.58f, 109.112f, 2.7248f, 0 },  // 198
    { -1306.84f, 1233.21f, 109.771f, 2.9638f, 0 },  // 199
    { -1331.25f, 1233.54f, 110.674f, 3.1281f, 0 },  // 200
    { -1350.02f, 1227.22f, 111.201f, 3.4664f, 0 },  // 201
    { -1389.37f, 1212.53f, 111.587f, 3.4989f, 0 },  // 202
    { -1415.8f, 1202.23f, 111.948f, 3.5132f, 0 },  // 203
    { -1424.26f, 1196.81f, 112.038f, 3.7114f, 0 },  // 204
    { -1449.27f, 1188.13f, 111.53f, 3.4756f, 0 },  // 205
    { -1474.53f, 1186.42f, 109.366f, 3.2092f, 0 },  // 206
    { -1491.78f, 1189.4f, 106.114f, 2.9705f, 0 },  // 207
    { -1502.95f, 1198.12f, 101.757f, 2.4788f, 0 },  // 208
    { -1512.09f, 1209.44f, 96.2469f, 2.25f, 0 },  // 209
    { -1520.19f, 1226.67f, 89.7861f, 2.0102f, 0 },  // 210
    { -1522.75f, 1243.63f, 83.3864f, 1.7206f, 0 },  // 211
    { -1520.9f, 1257.51f, 77.7027f, 1.4383f, 0 },  // 212
    { -1518.48f, 1273.17f, 71.8991f, 1.4175f, 0 },  // 213
    { -1516.17f, 1290.94f, 66.8473f, 1.4415f, 0 },  // 214
    { -1514.74f, 1306.19f, 63.4211f, 1.4773f, 0 },  // 215
    { -1511.54f, 1328.73f, 60.2051f, 1.4298f, 0 },  // 216
    { -1505.75f, 1341.47f, 59.2142f, 1.1442f, 0 },  // 217
    { -1494.42f, 1367.08f, 58.9254f, 1.1543f, 0 },  // 218
    { -1485.84f, 1393.02f, 58.9251f, 1.2514f, 0 },  // 219
    { -1485.47f, 1407.0f, 58.9469f, 1.5443f, 0 },  // 220
    { -1487.57f, 1434.84f, 58.9347f, 1.6461f, 0 },  // 221
    { -1489.45f, 1448.71f, 58.9302f, 1.7055f, 0 },  // 222
    { -1489.77f, 1469.49f, 58.9251f, 1.5862f, 0 },  // 223
    { -1483.8f, 1482.15f, 58.9251f, 1.1302f, 0 },  // 224
    { -1471.32f, 1507.22f, 58.9251f, 1.1089f, 0 },  // 225
    { -1464.94f, 1519.68f, 58.9251f, 1.0976f, 0 },  // 226
    { -1452.27f, 1544.64f, 58.9251f, 1.1011f, 0 },  // 227
    { -1442.94f, 1584.75f, 58.9255f, 1.3423f, 0 },  // 228
    { -1452.7f, 1610.98f, 58.9255f, 1.927f, 0 },  // 229
    { -1464.1f, 1641.7f, 58.9255f, 1.9261f, 0 },  // 230
    { -1474.86f, 1647.49f, 58.9255f, 2.6479f, 0 },  // 231
    { -1500.84f, 1657.91f, 58.9255f, 2.7602f, 0 },  // 232
    { -1521.76f, 1671.96f, 58.9255f, 2.5502f, 0 },  // 233
    { -1541.37f, 1691.94f, 58.9255f, 2.3468f, 0 },  // 234
    { -1551.2f, 1701.92f, 58.9255f, 2.3486f, 0 },  // 235
    { -1571.41f, 1721.29f, 58.9255f, 2.3774f, 0 },  // 236
    { -1592.97f, 1739.14f, 58.9255f, 2.4501f, 0 },  // 237
    { -1613.28f, 1758.07f, 58.9255f, 2.3913f, 0 },  // 238
    { -1630.32f, 1780.27f, 58.9255f, 2.2254f, 0 },  // 239
    { -1645.92f, 1803.52f, 58.9296f, 2.1618f, 0 },  // 240
    { -1661.7f, 1826.65f, 58.9271f, 2.1695f, 0 },  // 241
    { -1681.42f, 1858.29f, 58.9271f, 2.1281f, 0 },  // 242
    { -1686.38f, 1877.21f, 59.2059f, 1.8272f, 0 },  // 243
    { -1692.06f, 1899.02f, 60.7504f, 1.8256f, 0 },  // 244
    { -1699.45f, 1908.31f, 61.1412f, 2.2428f, 0 },  // 245
    { -1717.0f, 1915.93f, 60.0908f, 2.732f, 0 },  // 246
    { -1738.04f, 1917.48f, 59.0673f, 3.0681f, 0 },  // 247
    { -1757.16f, 1918.92f, 58.9757f, 3.0664f, 0 },  // 248
    { -1772.71f, 1926.58f, 59.1537f, 2.6839f, 0 },  // 249
    { -1791.81f, 1939.62f, 60.7298f, 2.5426f, 0 },  // 250
    { -1802.7f, 1951.89f, 60.7237f, 2.2967f, 0 },  // 251
    { -1809.79f, 1963.96f, 59.7477f, 2.1019f, 0 },  // 252
    { -1815.2f, 1976.74f, 59.0006f, 1.9712f, 0 },  // 253
    { -1817.51f, 2008.7f, 59.5336f, 1.6429f, 0 },  // 254
    { -1823.0f, 2032.7f, 60.6767f, 1.7957f, 0 },  // 255
    { -1821.87f, 2042.21f, 60.944f, 1.4525f, 0 },  // 256
    { -1813.14f, 2068.68f, 63.0096f, 1.2522f, 0 },  // 257
    { -1810.52f, 2082.43f, 63.114f, 1.3825f, 0 },  // 258
    { -1806.9f, 2095.9f, 63.1144f, 1.3083f, 0 },  // 259
    { -1802.03f, 2111.56f, 63.6862f, 1.2693f, 0 },  // 260
    { -1802.03f, 2111.56f, 63.6862f, 1.2693f, 0 },  // 261
    { -1801.53f, 2141.07f, 63.006f, 1.5539f, 0 },  // 262
    { -1801.53f, 2141.07f, 63.006f, 1.5539f, 0 },  // 263
    { -1802.25f, 2155.05f, 61.5195f, 1.6223f, 0 },  // 264
    { -1803.02f, 2183.03f, 59.8215f, 1.5983f, 0 },  // 265
    { -1808.36f, 2207.2f, 59.8215f, 1.7882f, 0 },  // 266
    { -1822.66f, 2219.86f, 59.8215f, 2.417f, 0 },  // 267
    { -1836.76f, 2232.87f, 59.8215f, 2.3964f, 0 },  // 268
    { -1843.44f, 2245.11f, 59.8215f, 2.0704f, 0 },  // 269
    { -1844.75f, 2266.9f, 59.8215f, 1.6308f, 0 },  // 270
    { -1846.34f, 2280.81f, 59.8215f, 1.6846f, 0 },  // 271
    { -1849.89f, 2294.13f, 59.8215f, 1.8313f, 0 },  // 272
    { -1864.58f, 2316.31f, 59.8215f, 2.1558f, 0 },  // 273
    { -1872.72f, 2327.7f, 59.8224f, 2.1913f, 0 },  // 274
    { -1884.98f, 2346.82f, 59.8224f, 2.141f, 0 },  // 275
    { -1887.2f, 2354.3f, 59.8696f, 1.8593f, 0 },  // 276
    { -1893.87f, 2379.11f, 59.9196f, 1.8334f, 0 },  // 277
    { -1900.07f, 2391.67f, 59.8224f, 2.0293f, 0 },  // 278
    { -1904.0f, 2405.04f, 59.8224f, 1.8567f, 0 },  // 279
    { -1904.0f, 2405.04f, 59.8224f, 1.8567f, 0 },  // 280
    { -1895.71f, 2431.78f, 59.8224f, 1.2702f, 0 },  // 281
};
}

enum DesolaceFactions
{
    FACTION_ESCORT_A_NEUTRAL_PASSIVE    = 10,
    FACTION_ESCORT_N_NEUTRAL_PASSIVE    = 113,
    FACTION_ESCORT_N_FRIEND_ACTIVE      = 495
};

/*######
## go_hand_of_iruxos_crystal
######*/

enum HandOfIruxos
{
    DEMON_SPIRIT = 11876
};

struct classic_go_hand_of_iruxos_crystal : public GameObjectAI
{
    classic_go_hand_of_iruxos_crystal(GameObject* go) : GameObjectAI(go) { }

    bool OnGossipHello(Player* player) override
    {
        if (me->GetGoType() == GAMEOBJECT_TYPE_GOOBER)
        {
            if (TempSummon* spirit = player->SummonCreature(DEMON_SPIRIT, -346.84f, 1765.13f, 138.39f, 5.91f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 15s))
                spirit->AI()->AttackStart(player);
        }

        return true;
    }
};

/*######
## npc_melizza_brimbuzzle
######*/

enum MelizzaBrimbuzzle
{
    QUEST_GET_ME_OUT_OF_HERE    = 6132,

    GO_MELIZZAS_CAGE            = 177706,

    SAY_MELIZZA_START           = 7540,
    SAY_MELIZZA_FINISH          = 7544,
    SAY_MELIZZA_1               = 7550,
    SAY_MELIZZA_2               = 7551,
    SAY_MELIZZA_3               = 7552,

    NPC_MARAUDINE_MARAUDER      = 4659,
    NPC_MARAUDINE_BONEPAW       = 4660,
    NPC_MARAUDINE_WRANGLER      = 4655,
    NPC_HORNIZ_BRIMBUZZLE       = 6019,

    NPC_MELIZZA                 = 12277,

    MAX_MARAUDERS               = 2,
    MAX_WRANGLERS               = 3
};

static Position const MelizzaMarauderSpawn[] =
{
    { -1291.492f, 2644.650f, 111.556f },
    { -1306.730f, 2675.163f, 111.561f },
};

static Position const MelizzaWranglerSpawn = { -1393.194f, 2429.465f, 88.689f };

struct classic_npc_melizza_brimbuzzle : public EscortAI
{
    classic_npc_melizza_brimbuzzle(Creature* creature) : EscortAI(creature), _dialogueStep(20), _dialogueTimer(0) { }

    void Reset() override
    {
        if (!HasEscortState(STATE_ESCORT_ESCORTING))
        {
            _dialogueStep = 20;
            _dialogueTimer = 0;
        }
    }

    void JustAppeared() override
    {
        me->RestoreFaction(); // VMaNGOS TEMPFACTION_RESTORE_RESPAWN
        EscortAI::JustAppeared();
    }

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() != QUEST_GET_ME_OUT_OF_HERE)
            return;

        // VMaNGOS walks, then SetRun(true) when the dialogue at point 12 ends (point ids start at 1 -> node index = id - 1)
        LoadClassicEscortPath(this, EscortPath12277, [](uint32 i) { return i + 1 > 12; });
        Start(true, player->GetGUID(), quest);

        // VMaNGOS JustStartedEscort()
        _dialogueStep = 20;
        if (GameObject* cage = GetClosestGameObjectWithEntry(me, GO_MELIZZAS_CAGE, INTERACTION_DISTANCE))
            cage->UseDoorOrButton();
    }

    void WaypointReached(uint32 waypointId, uint32 /*pathId*/) override
    {
        // VMaNGOS script_waypoint ids for this entry start at 1
        switch (waypointId + 1)
        {
            case 1:
                if (Player* player = GetPlayerForEscort())
                    ClassicScriptText(SAY_MELIZZA_START, me, player);

                me->SetFaction(FACTION_ESCORT_N_NEUTRAL_PASSIVE);
                break;
            case 4:
                for (Position const& spawn : MelizzaMarauderSpawn)
                {
                    for (uint8 j = 0; j < MAX_MARAUDERS; ++j)
                    {
                        // Summon 2 Marauders on each point
                        Position pos = me->GetRandomPoint(spawn, 7.0f);
                        me->SummonCreature(NPC_MARAUDINE_MARAUDER, pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), 0.0f, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 25s);
                    }
                }
                break;
            case 9:
                for (uint8 i = 0; i < MAX_WRANGLERS; ++i)
                {
                    Position pos = me->GetRandomPoint(MelizzaWranglerSpawn, 10.0f);
                    me->SummonCreature(NPC_MARAUDINE_BONEPAW, pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), 0.0f, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 25s);

                    pos = me->GetRandomPoint(MelizzaWranglerSpawn, 10.0f);
                    me->SummonCreature(NPC_MARAUDINE_WRANGLER, pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), 0.0f, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 25s);
                }
                break;
            case 12:
                _dialogueStep = 6;
                SetEscortPaused(true);
                SetMaxPlayerDistance(100); // Let's not have her despawn so easily.
                if (Player* player = GetPlayerForEscort())
                    me->SetFacingToObject(player);
                _dialogueTimer = 200;
                break;
            case 19:
                _dialogueStep = 0;
                SetEscortPaused(true);
                break;
            default:
                break;
        }
    }

    void Dialogue(uint32 diff)
    {
        if (_dialogueStep > 6)
            return;

        if (_dialogueTimer < diff)
        {
            switch (_dialogueStep)
            {
                case 0:
                    if (Creature* horniz = me->FindNearestCreature(NPC_HORNIZ_BRIMBUZZLE, 30.0f))
                        me->SetFacingToObject(horniz);
                    ClassicScriptText(SAY_MELIZZA_1, me);
                    _dialogueTimer = 4000;
                    ++_dialogueStep;
                    break;
                case 1:
                    ClassicScriptText(SAY_MELIZZA_2, me);
                    _dialogueTimer = 5000;
                    ++_dialogueStep;
                    break;
                case 2:
                    ClassicScriptText(SAY_MELIZZA_3, me);
                    _dialogueTimer = 4000;
                    ++_dialogueStep;
                    break;
                case 3:
                    SetEscortPaused(false);
                    _dialogueTimer = 0;
                    ++_dialogueStep;
                    break;
                case 6:
                    if (Player* player = GetPlayerForEscort())
                    {
                        ClassicScriptText(SAY_MELIZZA_FINISH, me, player);
                        player->GroupEventHappens(QUEST_GET_ME_OUT_OF_HERE, me);
                    }
                    _dialogueTimer = 2000;
                    ++_dialogueStep;
                    me->RestoreFaction(); // ClearTemporaryFaction()
                    // SetRun(true): the remaining nodes are run nodes (see OnQuestAccept)
                    SetEscortPaused(false);
                    break;
                default:
                    break;
            }
        }
        else
            _dialogueTimer -= diff;
    }

    void UpdateAI(uint32 diff) override
    {
        Dialogue(diff);
        EscortAI::UpdateAI(diff);
    }

private:
    uint16 _dialogueStep;
    uint32 _dialogueTimer;
};

/*######
## npc_dalinda_malem
######*/

enum DalindaMalem
{
    QUEST_RETURN_TO_VAHLARRIEL  = 1440
};

struct classic_npc_dalinda_malem : public EscortAI
{
    classic_npc_dalinda_malem(Creature* creature) : EscortAI(creature) { }

    void Reset() override { }

    void JustAppeared() override
    {
        me->SetImmuneToNPC(true);
        me->RestoreFaction(); // VMaNGOS TEMPFACTION_RESTORE_RESPAWN
        EscortAI::JustAppeared();
    }

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() != QUEST_RETURN_TO_VAHLARRIEL)
            return;

        me->SetFaction(FACTION_ESCORT_A_NEUTRAL_PASSIVE);
        me->SetImmuneToNPC(false);
        LoadClassicEscortPath(this, EscortPath5644, [](uint32) { return false; });
        Start(true, player->GetGUID(), quest);

        // VMaNGOS JustStartedEscort()
        me->SetStandState(UNIT_STAND_STATE_STAND);
    }

    void WaypointReached(uint32 waypointId, uint32 /*pathId*/) override
    {
        // VMaNGOS script_waypoint ids for this entry start at 1
        if (waypointId + 1 == 18)
        {
            if (Player* player = GetPlayerForEscort())
                player->GroupEventHappens(QUEST_RETURN_TO_VAHLARRIEL, me);
        }
    }
};

/*
 * Gizelton Caravan, Bodyguard For Hire support
 */

struct GizeltonStruct
{
    uint32 onLeave, onAnnounce, onAmbush0, onAmbush1, onAmbush2, onComplete;
};

static GizeltonStruct const CaravanTalk[] =
{
    { 7506, 7475, 7330, 7331, 7332, 7333 },
    { 7505, 7474, 7310, 7311, 7312, 7334 }
};

enum GizeltonCaravan
{
    NPC_RIGGER_GIZELTON     = 11626,
    NPC_CORK_GIZELTON       = 11625,
    NPC_SUPER_SELLER_680    = 12246,
    NPC_VENDOR_TRON_1000    = 12245,
    NPC_CARAVAN_KODO        = 11564,
    NPC_DOOMWARDER          = 4677,
    NPC_NETHER_SORCERESS    = 4684,
    NPC_LESSER_INFERNAL     = 4676,
    NPC_KOLKAR_AMBUSHER     = 12977,
    NPC_KOLKAR_WAYLAYER     = 12976,

    POINT_BOT_CAMP          = 279,
    POINT_BOT_ANNOUNCE      = 14,
    POINT_BOT_AMBUSH_0      = 28,
    POINT_BOT_AMBUSH_1      = 34,
    POINT_BOT_AMBUSH_2      = 40,
    POINT_BOT_COMPLETE      = 42,

    POINT_TOP_CAMP          = 141,
    POINT_TOP_ANNOUNCE      = 164,
    POINT_TOP_AMBUSH_0      = 173,
    POINT_TOP_AMBUSH_1      = 181,
    POINT_TOP_AMBUSH_2      = 188,
    POINT_TOP_COMPLETE      = 195,
    POINT_END               = 281,

    QUEST_BOTTOM            = 5943,
    QUEST_TOP               = 5821,

    // SetGUID id: classic_npc_rigger_gizelton calls cork->AI()->SetGUID(player->GetGUID(), DATA_RIGGER_QUEST_ACCEPTED)
    DATA_RIGGER_QUEST_ACCEPTED = 1
};

struct GizeltonCoords
{
    uint32 entry;
    float x, y, z, o;
};

struct GizeltonFormation
{
    float distance, angle;
};

struct GizeltonCaravanMember
{
    GizeltonCoords coords;
    GizeltonFormation form;
};

static GizeltonCoords const Ambusher[] =
{
    { NPC_DOOMWARDER,       -1814.41f, 1983.18f, 58.9549f, 0.0f },
    { NPC_NETHER_SORCERESS, -1814.41f, 1983.18f, 58.9549f, 0.0f },
    { NPC_LESSER_INFERNAL,  -1814.41f, 1983.18f, 58.9549f, 0.0f },

    { NPC_DOOMWARDER,       -1751.9f, 1917.2f, 59.0003f, 0.0f },
    { NPC_NETHER_SORCERESS, -1751.9f, 1917.2f, 59.0003f, 0.0f },
    { NPC_LESSER_INFERNAL,  -1751.9f, 1917.2f, 59.0003f, 0.0f },

    { NPC_DOOMWARDER,       -1684.12f, 1872.66f, 59.0354f, 0.0f },
    { NPC_NETHER_SORCERESS, -1684.12f, 1872.66f, 59.0354f, 0.0f },
    { NPC_LESSER_INFERNAL,  -1684.12f, 1872.66f, 59.0354f, 0.0f },

    { NPC_KOLKAR_AMBUSHER,  -792.515f, 1177.07f, 98.8327f, 0.0f },
    { NPC_KOLKAR_WAYLAYER,  -792.515f, 1177.07f, 98.8327f, 0.0f },

    { NPC_KOLKAR_AMBUSHER,  -931.15f, 1182.17f, 91.8346f, 0.0f },
    { NPC_KOLKAR_WAYLAYER,  -931.15f, 1182.17f, 91.8346f, 0.0f },

    { NPC_KOLKAR_AMBUSHER,  -1073.62f, 1186.33f, 89.7398f, 0.0f },
    { NPC_KOLKAR_WAYLAYER,  -1073.62f, 1186.33f, 89.7398f, 0.0f }
};

static GizeltonCaravanMember const Caravan[] =
{
    { { NPC_CARAVAN_KODO,     -1887.26f, 2465.12f, 59.8224f, 4.48f }, { 26.0f, 3.14f } },
    { { NPC_RIGGER_GIZELTON,  -1883.63f, 2471.68f, 59.8224f, 4.48f }, { 18.0f, 3.14f } },
    { { NPC_CARAVAN_KODO,     -1882.11f, 2476.80f, 59.8224f, 4.48f }, { 8.0f,  3.14f } }
};

// VMaNGOS moves the caravan with a CreatureGroup formation (JoinCreatureGroup, OPTION_FORMATION_MOVE | OPTION_AGGRO_TOGETHER).
// TC formations are keyed by DB spawn ids and cannot hold summons, so the members follow Cork with MoveFollow at the same
// distance/angle. TODO(classic): OPTION_AGGRO_TOGETHER (members assisting each other) is not reproduced.
// VMaNGOS switches walk/run with SetWalk(); TC EscortAI takes the move type from the path nodes, so the run sections
// (after a quest completes until the next camp) are baked into the path (see GizeltonIsRunNode).
static bool GizeltonIsRunNode(uint32 node)
{
    return (node > POINT_BOT_COMPLETE && node <= POINT_TOP_CAMP) || (node > POINT_TOP_COMPLETE && node <= POINT_BOT_CAMP);
}

struct classic_npc_cork_gizelton : public EscortAI
{
    classic_npc_cork_gizelton(Creature* creature) : EscortAI(creature)
    {
        ResetCreature();
    }

    void Reset() override { }

    void ResetCreature()
    {
        _caravanGuids.clear();
        _riggerGuid.Clear();
        _playerGuid.Clear();
        _enemiesCount = 0;
        _announceCount = 0;
        _initDelayTimer = 2000;
        _campTimer = 10 * MINUTE * IN_MILLISECONDS;
        _departTimer = MINUTE * IN_MILLISECONDS;
        _announceTimer = 0;
        _init = false;
        _camp = false;
        _waitingForPlayer = false;
        _waitingForDepart = false;
        _rigger = true;
    }

    void JustAppeared() override
    {
        ResetCreature();
        EscortAI::JustAppeared();
    }

    void SummonCaravan()
    {
        _caravanGuids.push_back(me->GetGUID());

        for (GizeltonCaravanMember const& member : Caravan)
        {
            if (TempSummon* summon = me->SummonCreature(member.coords.entry, member.coords.x, member.coords.y, member.coords.z, member.coords.o, TEMPSUMMON_DEAD_DESPAWN, 30s))
            {
                summon->setActive(true);
                AddToFormation(summon, member.form);
            }
            else
            {
                TC_LOG_ERROR("scripts", "[Desolace.GizeltonCaravan] Failed to summon caravan. Self-despawn.");
                DespawnCaravan();
                return;
            }
        }
    }

    void AddToFormation(Creature* who, GizeltonFormation const& form) const
    {
        who->GetMotionMaster()->MoveFollow(me, form.distance, form.angle);
    }

    void JustDied(Unit* /*killer*/) override
    {
        FailEscort();
    }

    // VMaNGOS Player::GroupEventFailHappens
    void FailQuestForGroup(Player* player, uint32 questId)
    {
        if (Group* group = player->GetGroup())
        {
            for (GroupReference const& groupRef : group->GetMembers())
            {
                Player* member = groupRef.GetSource();
                if (member->IsInMap(player) && member->GetQuestStatus(questId) == QUEST_STATUS_INCOMPLETE)
                    member->FailQuest(questId);
            }
        }
        else if (player->GetQuestStatus(questId) == QUEST_STATUS_INCOMPLETE)
            player->FailQuest(questId);
    }

    void FailEscort()
    {
        DespawnCaravan();

        if (_playerGuid.IsEmpty())
            return;

        if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid))
            FailQuestForGroup(player, _rigger ? QUEST_BOTTOM : QUEST_TOP);
    }

    void DespawnCaravan()
    {
        for (ObjectGuid const& guid : _caravanGuids)
        {
            if (guid != me->GetGUID())
            {
                if (Creature* killMe = ObjectAccessor::GetCreature(*me, guid))
                    killMe->DespawnOrUnsummon();
            }
        }

        me->DespawnOrUnsummon();
    }

    void CaravanFaction(bool apply)
    {
        for (ObjectGuid const& guid : _caravanGuids)
        {
            if (guid != me->GetGUID())
            {
                if (Creature* member = ObjectAccessor::GetCreature(*me, guid))
                {
                    if (apply)
                    {
                        member->SetFaction(FACTION_ESCORT_N_FRIEND_ACTIVE);
                        member->SetImmuneToNPC(false);
                    }
                    else
                    {
                        member->RestoreFaction();
                        member->SetImmuneToNPC(true);
                    }
                }
            }
        }

        if (apply)
        {
            me->SetFaction(FACTION_ESCORT_N_FRIEND_ACTIVE);
            me->SetImmuneToNPC(false);
        }
        else
        {
            me->RestoreFaction();
            me->SetImmuneToNPC(true);
        }
    }

    void SummonAmbusher(uint8 index)
    {
        // VMaNGOS: random walkable position within 20 yards (Map::GetWalkRandomPosition)
        Position pos = me->GetRandomPoint(Position(Ambusher[index].x, Ambusher[index].y, Ambusher[index].z), 20.0f);
        me->SummonCreature(Ambusher[index].entry, pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), 0.0f, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 30s);
    }

    void Ambush(uint32 point)
    {
        switch (point)
        {
            case POINT_BOT_AMBUSH_0:
                SummonAmbusher(0);
                SummonAmbusher(1);
                SummonAmbusher(2);
                DoTalk(CaravanTalk[0].onAmbush0);
                break;
            case POINT_BOT_AMBUSH_1:
                SummonAmbusher(3);
                SummonAmbusher(4);
                SummonAmbusher(5);
                DoTalk(CaravanTalk[0].onAmbush1);
                break;
            case POINT_BOT_AMBUSH_2:
                SummonAmbusher(6);
                SummonAmbusher(7);
                SummonAmbusher(8);
                DoTalk(CaravanTalk[0].onAmbush2);
                break;
            case POINT_TOP_AMBUSH_0:
                SummonAmbusher(9);
                SummonAmbusher(10);
                SummonAmbusher(9);
                SummonAmbusher(10);
                DoTalk(CaravanTalk[1].onAmbush0);
                break;
            case POINT_TOP_AMBUSH_1:
                SummonAmbusher(11);
                SummonAmbusher(12);
                SummonAmbusher(11);
                SummonAmbusher(12);
                DoTalk(CaravanTalk[1].onAmbush1);
                break;
            case POINT_TOP_AMBUSH_2:
                SummonAmbusher(13);
                SummonAmbusher(14);
                SummonAmbusher(13);
                SummonAmbusher(14);
                DoTalk(CaravanTalk[1].onAmbush2);
                break;
            default:
                break;
        }
    }

    void JustSummoned(Creature* summoned) override
    {
        switch (summoned->GetEntry())
        {
            case NPC_RIGGER_GIZELTON:
                _riggerGuid = summoned->GetGUID();
                summoned->RemoveNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
                [[fallthrough]];
            case NPC_CARAVAN_KODO:
                _caravanGuids.push_back(summoned->GetGUID());
                break;
            default:
            {
                ++_enemiesCount;

                if (_caravanGuids.empty())
                    break;

                // pick random caravan target
                ObjectGuid targetGuid = _caravanGuids[urand(0, uint32(_caravanGuids.size() - 1))];
                if (Creature* target = ObjectAccessor::GetCreature(*me, targetGuid))
                    summoned->AI()->AttackStart(target);
                break;
            }
        }
    }

    void SummonedCreatureDies(Creature* summoned, Unit* /*killer*/) override
    {
        switch (summoned->GetEntry())
        {
            case NPC_RIGGER_GIZELTON:
            case NPC_CARAVAN_KODO:
                FailEscort();
                break;
            default:
                if (_enemiesCount)
                    --_enemiesCount;

                if (!_enemiesCount)
                    SetEscortPaused(false);
                break;
        }
    }

    void ResumePath(Player* player)
    {
        _waitingForPlayer = false;
        _waitingForDepart = true;
        _announceCount = 0;
        if (player)
            _playerGuid = player->GetGUID();
        GiveQuest(false);
    }

    Creature* GetTalker()
    {
        return _rigger ? ObjectAccessor::GetCreature(*me, _riggerGuid) : me;
    }

    void DoTalk(uint32 textId, bool yell = false)
    {
        if (Creature* talker = GetTalker())
        {
            if (yell)
                talker->Yell(textId); // TODO(classic): VMaNGOS MonsterYellToZone (zone-wide yell); TC Yell uses yell range
            else
                talker->Say(textId);
        }
    }

    void GiveQuest(bool give)
    {
        if (Creature* giver = GetTalker())
        {
            if (give)
                giver->SetNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
            else
                giver->RemoveNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
        }
    }

    void DoVendor(bool visible)
    {
        if (Creature* vendor = me->FindNearestCreature(_rigger ? NPC_SUPER_SELLER_680 : NPC_VENDOR_TRON_1000, 100.0f))
            vendor->SetVisible(visible);
    }

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() == QUEST_TOP)
            ResumePath(player);
    }

    // VMaNGOS QuestAccept_npc_rigger_gizelton (classic_npc_rigger_gizelton) calls ResumePath on Cork
    void SetGUID(ObjectGuid const& guid, int32 id) override
    {
        if (id == DATA_RIGGER_QUEST_ACCEPTED)
            ResumePath(ObjectAccessor::GetPlayer(*me, guid));
    }

    void WaypointReached(uint32 waypointId, uint32 /*pathId*/) override
    {
        switch (waypointId)
        {
            case POINT_BOT_CAMP:
            case POINT_TOP_CAMP:
                SetEscortPaused(true);
                me->SetWalk(true); // CaravanWalk(true)
                _campTimer = 10 * MINUTE * IN_MILLISECONDS;
                _camp = true;
                DoVendor(true);
                break;
            case POINT_BOT_ANNOUNCE:
            case POINT_TOP_ANNOUNCE:
                SetEscortPaused(true);
                GiveQuest(true);
                _announceTimer = 0;
                _departTimer = 10 * IN_MILLISECONDS;
                _waitingForPlayer = true;
                break;
            case POINT_BOT_AMBUSH_0:
            case POINT_BOT_AMBUSH_1:
            case POINT_BOT_AMBUSH_2:
            case POINT_TOP_AMBUSH_0:
            case POINT_TOP_AMBUSH_1:
            case POINT_TOP_AMBUSH_2:
                if (!_playerGuid.IsEmpty())
                {
                    SetEscortPaused(true);
                    Ambush(waypointId);
                }
                break;
            case POINT_BOT_COMPLETE:
            case POINT_TOP_COMPLETE:
                if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid))
                {
                    DoTalk(CaravanTalk[_rigger ? 0 : 1].onComplete);

                    if (player->IsInRange(me, 0.0f, 100.0f))
                        player->GroupEventHappens(_rigger ? QUEST_BOTTOM : QUEST_TOP, me);
                }

                _playerGuid.Clear();
                CaravanFaction(false);
                me->SetWalk(false); // CaravanWalk(false)
                _rigger = !_rigger;
                break;
            case POINT_END:
                DespawnCaravan();
                break;
            default:
                break;
        }
    }

    void UpdateEscortAI(uint32 diff) override
    {
        // just summoned, do init
        if (!_init)
        {
            if (_initDelayTimer < diff)
            {
                SummonCaravan();
                _init = true;
                LoadClassicEscortPath(this, EscortPath11625, GizeltonIsRunNode);
                Start(true);
            }
            else
                _initDelayTimer -= diff;

            return;
        }

        // caravan is at camp point, vendor is available
        if (_camp)
        {
            if (_campTimer < diff)
            {
                _camp = false;
                DoTalk(CaravanTalk[_rigger ? 0 : 1].onLeave);
                DoVendor(false);
                SetEscortPaused(false);
            }
            else
                _campTimer -= diff;

            return;
        }

        // caravan is at waiting point, announcing every 3 minutes
        if (_waitingForPlayer)
        {
            if (_announceTimer < diff)
            {
                ++_announceCount;

                // caravan stays for 15+ minutes waiting for help
                if (_announceCount > 5)
                {
                    ResumePath(nullptr);
                    return;
                }

                DoTalk(CaravanTalk[_rigger ? 0 : 1].onAnnounce, true);
                _announceTimer = 3 * MINUTE * IN_MILLISECONDS;
            }
            else
                _announceTimer -= diff;

            return;
        }

        // player is here, 10 seconds more and caravan goes
        if (_waitingForDepart)
        {
            if (_departTimer < diff)
            {
                _waitingForDepart = false;
                CaravanFaction(true);
                SetEscortPaused(false);
            }
            else
                _departTimer -= diff;

            return;
        }

        EscortAI::UpdateEscortAI(diff);
    }

private:
    std::vector<ObjectGuid> _caravanGuids;
    ObjectGuid _riggerGuid;
    ObjectGuid _playerGuid;
    uint8 _enemiesCount;
    uint8 _announceCount;
    uint32 _initDelayTimer;
    uint32 _campTimer;
    uint32 _announceTimer;
    uint32 _departTimer;
    bool _init;
    bool _camp;
    bool _waitingForPlayer;
    bool _waitingForDepart;
    bool _rigger;
};

// Rigger Gizelton: summoned by Cork with the caravan (no spawn of his own). VMaNGOS QuestAccept_npc_rigger_gizelton: taking
// Gizelton Caravan (5943) from him at the southern camp sets the caravan off.
struct classic_npc_rigger_gizelton : public ScriptedAI
{
    classic_npc_rigger_gizelton(Creature* creature) : ScriptedAI(creature) { }

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() != QUEST_BOTTOM)
            return;

        if (Creature* cork = me->FindNearestCreature(NPC_CORK_GIZELTON, 100.0f))
            cork->AI()->SetGUID(player->GetGUID(), DATA_RIGGER_QUEST_ACCEPTED);
    }
};

void AddSC_classic_desolace()
{
    RegisterGameObjectAI(classic_go_hand_of_iruxos_crystal);
    RegisterCreatureAI(classic_npc_melizza_brimbuzzle);
    RegisterCreatureAI(classic_npc_dalinda_malem);
    RegisterCreatureAI(classic_npc_cork_gizelton);
    RegisterCreatureAI(classic_npc_rigger_gizelton);
}
