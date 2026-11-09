#include "tc_catch2.h"
#include "RequestRateLimit.h"

using Trinity::Net::RequestRateLimit;

TEST_CASE("A connection gets a burst of ten seconds' worth of requests, then no more", "[RequestRateLimit]")
{
    RequestRateLimit limit(10, 1000);
    for (int i = 0; i < 100; ++i)
        REQUIRE(limit.Allow(1000));
    CHECK_FALSE(limit.Allow(1000));
}

TEST_CASE("The allowance comes back at the configured rate", "[RequestRateLimit]")
{
    RequestRateLimit limit(10, 0);
    for (int i = 0; i < 100; ++i)
        REQUIRE(limit.Allow(0));
    CHECK_FALSE(limit.Allow(50));    // 50 ms: half a request back
    CHECK(limit.Allow(100));         // 100 ms: one request back
    CHECK_FALSE(limit.Allow(100));
    CHECK(limit.Allow(1100));        // one second: ten more
    for (int i = 0; i < 9; ++i)
        REQUIRE(limit.Allow(1100));
    CHECK_FALSE(limit.Allow(1100));
}

TEST_CASE("A quiet connection never banks more than the burst", "[RequestRateLimit]")
{
    RequestRateLimit limit(5, 0);
    int allowed = 0;
    while (limit.Allow(3600000) && allowed < 1000)
        ++allowed;
    CHECK(allowed == 50);
}

TEST_CASE("A normal login and keep-alives stay well inside the default", "[RequestRateLimit]")
{
    RequestRateLimit limit(10, 0);
    for (int i = 0; i < 40; ++i)            // the login burst
        REQUIRE(limit.Allow(i * 10));
    for (int64 t = 1000; t < 600000; t += 2000)   // a realm list refresh every 2 s for ten minutes
        REQUIRE(limit.Allow(t));
}

TEST_CASE("A limit of 0 never refuses", "[RequestRateLimit]")
{
    RequestRateLimit limit(0, 0);
    for (int i = 0; i < 10000; ++i)
        REQUIRE(limit.Allow(0));
}

TEST_CASE("A clock that steps back does not refill", "[RequestRateLimit]")
{
    RequestRateLimit limit(1, 10000);
    for (int i = 0; i < 10; ++i)
        REQUIRE(limit.Allow(10000));
    CHECK_FALSE(limit.Allow(5000));
    CHECK_FALSE(limit.Allow(10000));
}
