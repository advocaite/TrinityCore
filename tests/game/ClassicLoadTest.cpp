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

#include "tc_catch2.h"

#include "../../src/modules/PlayerbotV2/Diagnostics/LoadTestRecorder.h"

using namespace Playerbot::Diagnostics;

TEST_CASE("TickHistogram percentiles", "[loadtest]")
{
    TickHistogram h;
    CHECK(h.Percentile(99) == 0);
    for (int i = 0; i < 99; ++i) h.Add(10);
    h.Add(300);
    CHECK(h.Count() == 100);
    CHECK(h.Percentile(50) == 10);
    CHECK(h.Percentile(99) == 10);       // 99 of 100 ticks took <= 10 ms
    CHECK(h.Percentile(100) == 300);
    CHECK(h.Max() == 300);
    CHECK(h.Avg() == Catch::Approx(12.9));
    h.Add(5000);                          // beyond the last bucket is counted in it
    CHECK(h.Percentile(100) == TickHistogram::kBuckets - 1);
    CHECK(h.Max() == 5000);
    h.Reset();
    CHECK(h.Count() == 0);
}

TEST_CASE("Load test verdict and CSV", "[loadtest]")
{
    LoadLimits limits;
    LoadRow row;
    row.unix_time = 1760000000; row.build = "abc,def"; row.label = "mc+wsg"; row.bots_awake = 120; row.bots_raid = 40; row.bots_bg = 20;
    row.bots_world = 60; row.bots_sleeping = 1500; row.ticks = 1200; row.tick_avg_ms = 21.5; row.tick_p50_ms = 18; row.tick_p99_ms = 60;
    row.tick_max_ms = 90; row.rss_mb = 5200; row.cpu_pct = 31.4;
    CHECK(Verdict(row, limits).empty());
    CHECK(CsvRow(row, limits).find(",PASS,") != std::string::npos);

    row.tick_p99_ms = 150; row.rss_mb = 9500; row.slowest_call = "RunHygiene"; row.slowest_call_ms = 400;
    std::string const v = Verdict(row, limits);
    CHECK(v == "p99>100;rss>9000;slow_call>250");
    std::string const csv = CsvRow(row, limits);
    CHECK(csv.find(",FAIL,p99>100;rss>9000;slow_call>250") != std::string::npos);
    CHECK(csv.find("abc def") != std::string::npos);   // comma in a field is replaced

    // header and row have the same number of columns
    auto cols = [](std::string const& s) { size_t n = 1; for (char c : s) n += (c == ','); return n; };
    CHECK(cols(CsvHeader()) == cols(csv));
}
