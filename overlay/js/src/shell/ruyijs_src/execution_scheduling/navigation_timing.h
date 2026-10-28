#pragma once
#include "domain/execution_scheduling/navigation_timing.h"
namespace ruyijs::execution_scheduling {
// Transient values supplied by the runtime load coordinator; the retained state
// belongs to the original Performance owner and its realm-owned native entry
// (docs/owners/NAVIGATION_TIMING.md).
using NavigationTiming = ruyijs::domain::execution_scheduling::NavigationTiming;
}
