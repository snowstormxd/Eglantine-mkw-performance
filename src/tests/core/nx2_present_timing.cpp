// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>
#include "core/hle/service/nx2_present_timing.h"

using namespace std::chrono_literals;

TEST_CASE("Native presentation uses the game's refresh rate and swap interval", "[nx2_timing]") {
    using Service::Nx2::PresentInterval;
    CHECK(PresentInterval(0x1003c, 100).count() == 16'666'666);
    CHECK(PresentInterval(0x2003c, 100).count() == 33'333'333);
    CHECK(PresentInterval(0x10078, 100).count() == 8'333'333);
    CHECK(PresentInterval(0x1003c, 200).count() == 8'333'333);
    CHECK(PresentInterval(0x1003c, 50).count() == 33'333'333);
    CHECK(PresentInterval(0, 100) == PresentInterval(0x1003c, 100));
    CHECK(PresentInterval(0xffffffff, 100) == PresentInterval(0x4003c, 100));
    CHECK(PresentInterval(0x2003c, 100, 1) == PresentInterval(0x1003c, 100));
    CHECK(PresentInterval(0x1003c, 100, 0xffffffff) == PresentInterval(0x4003c, 100));
    CHECK(PresentInterval(0x1003c, 0) == 0ns);
}

TEST_CASE("Native presentation recovers from slow frames without banking lost time",
          "[nx2_timing]") {
    Service::Nx2::PresentPacer pacer;
    const auto start = Service::Nx2::PresentPacer::Clock::time_point{} + 1s;
    CHECK(pacer.Schedule(start, 16ms) == start);
    CHECK(pacer.Schedule(start + 1ms, 16ms) == start + 16ms);
    CHECK(pacer.Schedule(start + 20ms, 16ms) == start + 32ms);
    CHECK(pacer.Schedule(start + 250ms, 16ms) == start + 250ms);
    CHECK(pacer.Schedule(start + 251ms, 16ms) == start + 266ms);
    CHECK(pacer.Schedule(start + 252ms, 0ns) == start + 252ms);
    CHECK(pacer.Schedule(start + 253ms, 16ms) == start + 253ms);
}
