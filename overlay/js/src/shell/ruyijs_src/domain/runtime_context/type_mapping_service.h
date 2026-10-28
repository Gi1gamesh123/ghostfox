#pragma once

#include "domain/runtime_context/js_value.h"

#include <string>

namespace ruyijs::domain::runtime_context {

/// Domain Service: Defines the rules for mapping between JS types and Python types.
/// This is a pure domain concept — the actual conversion is done by infrastructure.
///
/// Mapping rules (JS → Python):
///   undefined    → None
///   null         → None
///   boolean      → bool
///   int32/uint32 → int
///   double       → float (or int if value is integral and within int64 range)
///   string       → str
///   array        → list (recursive)
///   object       → dict (recursive, only if to_py=True)
///   function     → callable wrapper
///   bigint       → int
///
/// Mapping rules (Python → JS):
///   None         → null
///   bool         → boolean
///   int          → int32/uint32/double (depending on range)
///   float        → double
///   str          → string
///   list/tuple   → array
///   dict         → object
///   callable     → function
class TypeMappingService {
public:
    /// Determine the Python type name for a given JS value type
    static std::string python_type_for(JsValueType js_type) {
        switch (js_type) {
            case JsValueType::Undefined: return "NoneType";
            case JsValueType::Null:      return "NoneType";
            case JsValueType::Boolean:   return "bool";
            case JsValueType::Int32:     return "int";
            case JsValueType::Uint32:    return "int";
            case JsValueType::Double:    return "float";
            case JsValueType::String:    return "str";
            case JsValueType::Array:     return "list";
            case JsValueType::Object:    return "dict";
            case JsValueType::Function:  return "function";
            case JsValueType::Symbol:    return "symbol";
            case JsValueType::BigInt:    return "int";
        }
        return "unknown";
    }

    /// Determine if a JS double should be converted to Python int
    /// (when the value is integral and within safe integer range)
    static bool should_convert_double_to_int(double value) {
        // Check if value is integral
        if (value != static_cast<double>(static_cast<int64_t>(value))) {
            return false;
        }
        // Check safe integer range (2^53 - 1)
        constexpr double MAX_SAFE_INTEGER = 9007199254740991.0;
        return value >= -MAX_SAFE_INTEGER && value <= MAX_SAFE_INTEGER;
    }

    /// Determine the JS value type for a Python integer based on its range
    static JsValueType js_type_for_integer(int64_t value) {
        if (value >= INT32_MIN && value <= INT32_MAX) {
            return JsValueType::Int32;
        }
        if (value >= 0 && value <= UINT32_MAX) {
            return JsValueType::Uint32;
        }
        return JsValueType::Double;  // Falls back to double for large integers
    }

    /// Whether an object should be recursively converted (to_py semantics)
    static bool should_deep_convert(JsValueType type, bool to_py_flag) {
        if (!to_py_flag) return false;
        return type == JsValueType::Object || type == JsValueType::Array;
    }
};

} // namespace ruyijs::domain::runtime_context
