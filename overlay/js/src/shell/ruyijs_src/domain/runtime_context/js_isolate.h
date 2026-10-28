#pragma once

#include "domain/shared/entity.h"

#include <cstdint>
#include <string>

namespace ruyijs::domain::runtime_context {

/// Unique identifier for an isolate instance
using IsolateId = uint64_t;

/// Entity: Represents a V8 Isolate at the domain level.
/// Each RuntimeContext owns exactly one Isolate (1:1 relationship).
/// The domain only tracks identity and state — actual V8 isolate
/// management is in the infrastructure layer.
class JsIsolate : public shared::Entity<IsolateId> {
public:
    enum class State {
        Created,    // Isolate allocated but not yet entered
        Active,     // Isolate is entered and usable
        Disposed,   // Isolate has been disposed
    };

    explicit JsIsolate(IsolateId id)
        : Entity<IsolateId>(id), state_(State::Created) {}

    State state() const noexcept { return state_; }
    bool is_active() const noexcept { return state_ == State::Active; }
    bool is_disposed() const noexcept { return state_ == State::Disposed; }

    void mark_active() { state_ = State::Active; }
    void mark_disposed() { state_ = State::Disposed; }

    /// Heap statistics (updated by infrastructure layer)
    struct HeapStats {
        size_t total_heap_size = 0;
        size_t used_heap_size = 0;
        size_t external_memory = 0;
    };

    const HeapStats& heap_stats() const noexcept { return heap_stats_; }
    void update_heap_stats(HeapStats stats) { heap_stats_ = stats; }

private:
    State state_;
    HeapStats heap_stats_;
};

} // namespace ruyijs::domain::runtime_context
