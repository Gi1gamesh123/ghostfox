#pragma once

#include <string>

namespace ruyijs::domain::runtime_context {

/// Value Object: Options for JavaScript code evaluation.
/// Immutable — all fields set at construction.
class EvalOptions {
public:
    EvalOptions() = default;

    EvalOptions(std::string name, int line, int col, bool to_py, bool devtools)
        : name_(std::move(name)),
          line_(line),
          col_(col),
          to_py_(to_py),
          devtools_(devtools) {}

    /// Script name for stack traces (default: empty = "<anonymous>")
    const std::string& name() const noexcept { return name_; }

    /// Starting line offset (-1 = no offset)
    int line() const noexcept { return line_; }

    /// Starting column offset (-1 = no offset)
    int col() const noexcept { return col_; }

    /// Whether to recursively convert result to Python types
    bool to_py() const noexcept { return to_py_; }

    /// Whether DevTools should track this eval
    bool devtools() const noexcept { return devtools_; }

    /// Effective script name (returns "<anonymous>" if empty)
    std::string effective_name() const {
        return name_.empty() ? "<anonymous>" : name_;
    }

    // Builder-style setters (return new instance)
    EvalOptions with_name(std::string name) const {
        return EvalOptions(std::move(name), line_, col_, to_py_, devtools_);
    }
    EvalOptions with_line(int line) const {
        return EvalOptions(name_, line, col_, to_py_, devtools_);
    }
    EvalOptions with_col(int col) const {
        return EvalOptions(name_, line_, col, to_py_, devtools_);
    }
    EvalOptions with_to_py(bool to_py) const {
        return EvalOptions(name_, line_, col_, to_py, devtools_);
    }
    EvalOptions with_devtools(bool devtools) const {
        return EvalOptions(name_, line_, col_, to_py_, devtools);
    }

    bool operator==(const EvalOptions& other) const {
        return name_ == other.name_ && line_ == other.line_ &&
               col_ == other.col_ && to_py_ == other.to_py_ &&
               devtools_ == other.devtools_;
    }
    bool operator!=(const EvalOptions& other) const { return !(*this == other); }

private:
    std::string name_;
    int line_ = -1;
    int col_ = -1;
    bool to_py_ = false;
    bool devtools_ = true;
};

} // namespace ruyijs::domain::runtime_context
