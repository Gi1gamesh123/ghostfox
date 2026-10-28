#pragma once

#include "application/engine_port.h"
#include "application/dto.h"

namespace ruyijs::application {

class ContextLifecycleAppService {
public:
    explicit ContextLifecycleAppService(JsEnginePort& engine)
        : engine_(engine) {}

    /// Create and initialize a new context
    std::unique_ptr<domain::runtime_context::RuntimeContextAggregate> create_context(
        const dto::ContextConfigDto& config_dto)
    {
        // Build domain config from DTO
        domain::runtime_context::RuntimeContextAggregate::Config domain_config;
        domain_config.mode = (config_dto.mode == "debug")
            ? domain::runtime_context::ContextMode::Debug
            : domain::runtime_context::ContextMode::Prod;
        domain_config.time_mode = (config_dto.time_mode == "system")
            ? domain::runtime_context::TimeMode::System
            : domain::runtime_context::TimeMode::Logical;
        domain_config.js_api_name = config_dto.js_api;

        // Generate unique ID
        auto id = next_context_id_++;

        // Create aggregate
        auto context = std::make_unique<domain::runtime_context::RuntimeContextAggregate>(
            id, domain_config);

        // Initialize engine
        engine_.initialize(id, domain_config);

        // Setup browser environment
        auto env = std::make_unique<domain::browser_environment::BrowserEnvironmentAggregate>(id);
        if (!config_dto.environment.entries.empty()) {
            domain::browser_environment::FingerprintConfig overrides;
            for (const auto& [path, value] : config_dto.environment.entries) {
                overrides.set(path, value);
            }
            env->apply_overrides(overrides);
        }
        engine_.setup_globals(*env);

        // Mark ready
        context->mark_ready();

        return context;
    }

    /// Execute JavaScript in a context
    domain::runtime_context::EvalResult eval(
        domain::runtime_context::RuntimeContextAggregate& context,
        const std::string& source,
        const domain::runtime_context::EvalOptions& options = {})
    {
        // Domain guard: ensure context is evaluable
        context.ensure_evaluable();

        // Delegate to engine
        auto result = engine_.eval(source, options);

        // Update statistics
        context.increment_eval_count();

        return result;
    }

    /// Close a context and release resources
    void close_context(domain::runtime_context::RuntimeContextAggregate& context,
                       const std::string& gc_str = "none")
    {
        auto gc_policy = domain::runtime_context::ContextLifecyclePolicy::parse_gc_policy(gc_str);
        engine_.shutdown(gc_policy);
        context.close();
    }

private:
    JsEnginePort& engine_;
    uint64_t next_context_id_ = 1;
};

} // namespace ruyijs::application
