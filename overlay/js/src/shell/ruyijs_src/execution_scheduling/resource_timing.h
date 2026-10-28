#pragma once
#include "domain/execution_scheduling/resource_timing.h"
namespace ruyijs::execution_scheduling {
// Transient values supplied by the runtime coordinator; retained state belongs
// to the original Performance timeline and its realm-owned native entry.
using ResourceTimingRecord=ruyijs::domain::execution_scheduling::ResourceTimingRecord;
}
