#pragma once

#include "domain/shared/aggregate_root.h"
#include "domain/shared/domain_exception.h"
#include "domain/runtime_context/eval_options.h"
#include "domain/runtime_context/eval_result.h"
#include "domain/runtime_context/js_isolate.h"
#include "domain/runtime_context/js_value.h"

#include <cstdint>
#include <memory>
#include <string>

namespace ruyijs::domain::runtime_context {

/// Unique identifier for a runtime context
using ContextId = uint64_t;

/// Running mode of the context
enum class ContextMode {
    Prod,   // Zero monitoring overhead
    Debug,  // API call tracing + DevTools enabled
};

/// Time mode for the event loop
enum class TimeMode {
    Logical, // Pure logical time advancement (default)
    System,  // System clock anchored
};

/// Aggregate Root: RuntimeContext
/// Manages the lifecycle of a JavaScript execution environment.
/// Each RuntimeContext owns exactly one V8 Isolate and provides
/// eval/close operations. This is the main entry point aggregate.
class RuntimeContextAggregate : public shared::AggregateRoot<ContextId> {
public:
    /// Configuration for creating a new context
    struct Config {
        ContextMode mode = ContextMode::Prod;
        TimeMode time_mode = TimeMode::Logical;
        std::string js_api_name = "__ruyijs__";  // JS-side tool object mount name
    };

    explicit RuntimeContextAggregate(ContextId id, Config config = {})
        : AggregateRoot<ContextId>(id),
          config_(std::move(config)),
          isolate_(std::make_unique<JsIsolate>(id)),
          state_(State::Initializing) {}

    ~RuntimeContextAggregate() override = default;

    // --- State management ---

    enum class State {
        Initializing, // Being set up
        Ready,        // Ready for eval
        Closed,       // Resources released
    };

    State state() const noexcept { return state_; }
    bool is_ready() const noexcept { return state_ == State::Ready; }
    bool is_closed() const noexcept { return state_ == State::Closed; }

    /// Transition to Ready state (called after infrastructure setup completes)
    void mark_ready() {
        if (state_ != State::Initializing) {
            throw shared::InvalidStateException(
                "Context can only transition to Ready from Initializing");
        }
        state_ = State::Ready;
        isolate_->mark_active();
    }

    /// Close the context and release resources
    void close() {
        if (state_ == State::Closed) return;  // Idempotent
        state_ = State::Closed;
        isolate_->mark_disposed();
    }

    // --- Configuration accessors ---

    const Config& config() const noexcept { return config_; }
    ContextMode mode() const noexcept { return config_.mode; }
    TimeMode time_mode() const noexcept { return config_.time_mode; }
    const std::string& js_api_name() const noexcept { return config_.js_api_name; }

    // --- Isolate access ---

    JsIsolate& isolate() noexcept { return *isolate_; }
    const JsIsolate& isolate() const noexcept { return *isolate_; }

    // --- Eval guard ---

    /// Validate that the context is in a state where eval is allowed
    void ensure_evaluable() const {
        if (state_ != State::Ready) {
            throw shared::InvalidStateException(
                "Cannot eval: context is not in Ready state");
        }
    }

    // --- Statistics ---

    uint64_t eval_count() const noexcept { return eval_count_; }
    void increment_eval_count() noexcept { ++eval_count_; }

private:
    Config config_;
    std::unique_ptr<JsIsolate> isolate_;
    State state_;
    uint64_t eval_count_ = 0;
};

} // namespace ruyijs::domain::runtime_context
