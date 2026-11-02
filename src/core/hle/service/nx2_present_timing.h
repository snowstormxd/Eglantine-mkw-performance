// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <chrono>
#include "common/common_types.h"

namespace Service::Nx2 {

inline std::chrono::nanoseconds PresentInterval(u32 timing, u32 speed_percent,
                                                u32 forced_interval = 0) {
    if (speed_percent == 0) {
        return {};
    }
    const u32 swap_interval =
        std::clamp(forced_interval != 0 ? forced_interval : timing >> 16, 1U, 4U);
    const u32 rate = timing & 0xffff;
    const u32 refresh_rate = rate >= 30 && rate <= 240 ? rate : 60;
    return std::chrono::nanoseconds{100'000'000'000LL * swap_interval / refresh_rate /
                                    speed_percent};
}

class PresentPacer {
public:
    using Clock = std::chrono::steady_clock;

    Clock::time_point Schedule(Clock::time_point now, std::chrono::nanoseconds interval) {
        if (interval <= std::chrono::nanoseconds::zero()) {
            next = {};
            return now;
        }
        if (now > next + interval) {
            next = now;
        }
        const auto deadline = std::max(now, next);
        next += interval;
        return deadline;
    }

private:
    Clock::time_point next{};
};

} // namespace Service::Nx2
