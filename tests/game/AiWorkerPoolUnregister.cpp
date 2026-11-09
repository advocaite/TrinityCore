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

#ifdef TESTS_HAVE_PLAYERBOT_V2

#include "../../src/modules/PlayerbotV2/Threading/AiWorkerPool.h"
#include <chrono>

// A bot must not be unregistered (its BotAI destroyed) while a worker still holds it: wait_not_in_flight is the barrier
// TickScheduler::unregister_bot uses before the registry entry is erased.
TEST_CASE("AiWorkerPool::wait_not_in_flight returns at once for a stopped pool and an unknown bot", "[AiWorkerPool]")
{
    Playerbot::AiWorkerPool pool(1);
    auto const t0 = std::chrono::steady_clock::now();
    pool.wait_not_in_flight(42);   // never started: must not block
    REQUIRE(std::chrono::steady_clock::now() - t0 < std::chrono::seconds(1));
}

TEST_CASE("AiWorkerPool::wait_not_in_flight waits out a queued tick", "[AiWorkerPool]")
{
    Playerbot::AiWorkerPool pool(2);
    pool.start();
    pool.schedule_tick(7);          // services are not initialised here, so the worker returns immediately
    pool.wait_not_in_flight(7);
    // After the barrier the bot is no longer queued or running: a second wait returns at once.
    auto const t0 = std::chrono::steady_clock::now();
    pool.wait_not_in_flight(7);
    REQUIRE(std::chrono::steady_clock::now() - t0 < std::chrono::seconds(1));
    pool.stop();
}

#endif
