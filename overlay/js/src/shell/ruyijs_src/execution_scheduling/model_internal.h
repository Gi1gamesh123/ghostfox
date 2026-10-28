#ifndef RUYIJS_SCHEDULING_MODEL_INTERNAL_H
#define RUYIJS_SCHEDULING_MODEL_INTERNAL_H
#include "execution_scheduling/model.h"
#include "domain/execution_scheduling/event_loop.h"
#include <cmath>
#include <new>
namespace ruyijs::execution_scheduling {
namespace domain = ruyijs::domain::execution_scheduling;
struct Model::Impl { domain::EventLoopAggregate loop{1, domain::TimeMode::System}; };
inline Status OOM() { return {ErrorCode::OutOfMemory, "Scheduling allocation failed"}; }
inline Status Range(const char* text) { return {ErrorCode::Range, text}; }
}
#endif
