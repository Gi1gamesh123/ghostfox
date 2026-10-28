#pragma once

#include "application/domain.h"
#include "domain/runtime_context/runtime_context.h"
#include "domain/runtime_context/context_lifecycle_policy.h"
#include "domain/browser_environment/browser_environment.h"
#include "domain/execution_scheduling/task.h"

namespace ruyijs::application {

class JsEnginePort {
public:
    virtual ~JsEnginePort() = default;

    /// Initialize the engine (V8 platform, isolate, context)
    virtual bool initialize(domain::runtime_context::ContextId id,
                            const domain::runtime_context::RuntimeContextAggregate::Config& config) = 0;

    /// Execute JavaScript code and return result
    virtual domain::runtime_context::EvalResult eval(
        const std::string& source,
        const domain::runtime_context::EvalOptions& options) = 0;

    /// Shutdown and release resources
    virtual void shutdown(domain::runtime_context::GcPolicy gc_policy) = 0;

    /// Inject browser environment globals into the V8 context
    virtual void setup_globals(const domain::browser_environment::BrowserEnvironmentAggregate& env) = 0;

    /// Execute a task callback by ID (for event loop)
    virtual void execute_task_callback(domain::execution_scheduling::TaskId task_id) = 0;
};

} // namespace ruyijs::application
