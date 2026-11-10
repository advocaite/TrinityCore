#include "tc_catch2.h"
#include "IpConnectionLimit.h"

using Trinity::Net::IpConnectionLimit;

TEST_CASE("An address gets no more than the limit of connections", "[IpConnectionLimit]")
{
    IpConnectionLimit limit;
    auto a = limit.TryAcquire("10.0.0.5", false, 2);
    auto b = limit.TryAcquire("10.0.0.5", false, 2);
    REQUIRE(a);
    REQUIRE(b);
    CHECK(!limit.TryAcquire("10.0.0.5", false, 2));
    CHECK(limit.GetCount("10.0.0.5") == 2);

    // another address has its own count
    CHECK(limit.TryAcquire("10.0.0.6", false, 2));
}

TEST_CASE("A closed connection frees its place", "[IpConnectionLimit]")
{
    IpConnectionLimit limit;
    auto a = limit.TryAcquire("10.0.0.5", false, 1);
    REQUIRE(a);
    CHECK(!limit.TryAcquire("10.0.0.5", false, 1));
    a.reset();
    CHECK(limit.GetCount("10.0.0.5") == 0);
    CHECK(limit.TryAcquire("10.0.0.5", false, 1));
}

TEST_CASE("Loopback and a limit of 0 are never limited or counted", "[IpConnectionLimit]")
{
    IpConnectionLimit limit;
    for (int i = 0; i < 50; ++i)
    {
        CHECK(limit.TryAcquire("127.0.0.1", true, 1));
        CHECK(limit.TryAcquire("10.0.0.5", false, 0));
    }
    CHECK(limit.GetCount("127.0.0.1") == 0);
    CHECK(limit.GetCount("10.0.0.5") == 0);
}
