#pragma once
#include "domain/execution_scheduling/time_point.h"

namespace ruyijs::domain::execution_scheduling {
// Value state owned by EventLoopAggregate, never an independent clock owner.
struct ClockState {
    TimeMode mode = TimeMode::Logical;
    TimePoint elapsed;
    double epoch_ms = 0;
    unsigned running_nesting = 0;
    double frame_timestamp = -1;
    uint64_t frame_cutoff = 0;
    double idle_end = 0;
    uint64_t idle_cutoff = 0;
    bool idle_running = false;
};
}
