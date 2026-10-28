#pragma once

#include "domain/runtime_context/js_value.h"

#include <optional>
#include <string>

namespace ruyijs::domain::runtime_context {

/// Value Object: Result of a JavaScript evaluation.
/// Either a successful value or an error (exception).
class EvalResult {
public:
    /// Create a successful evaluation result
    static EvalResult success(JsValuePtr value) {
        EvalResult r;
        r.value_ = std::move(value);
        r.has_error_ = false;
        return r;
    }

    /// Create a failed evaluation result (JS exception)
    static EvalResult error(std::string message, std::string stack_trace = "") {
        EvalResult r;
        r.error_message_ = std::move(message);
        r.stack_trace_ = std::move(stack_trace);
        r.has_error_ = true;
        return r;
    }

    bool is_success() const noexcept { return !has_error_; }
    bool is_error() const noexcept { return has_error_; }

    /// The resulting JS value (valid only if is_success())
    const JsValuePtr& value() const noexcept { return value_; }

    /// Error message (valid only if is_error())
    const std::string& error_message() const noexcept { return error_message_; }

    /// Stack trace of the error (may be empty)
    const std::string& stack_trace() const noexcept { return stack_trace_; }

    /// Full error description including stack trace
    std::string full_error() const {
        if (stack_trace_.empty()) return error_message_;
        return error_message_ + "\n" + stack_trace_;
    }

private:
    EvalResult() = default;

    JsValuePtr value_;
    std::string error_message_;
    std::string stack_trace_;
    bool has_error_ = false;
};

} // namespace ruyijs::domain::runtime_context
