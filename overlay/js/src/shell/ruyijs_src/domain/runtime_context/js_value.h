#pragma once

#include "domain/shared/value_object.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace ruyijs::domain::runtime_context {

/// Represents the type of a JavaScript value at the domain level.
/// This is a pure domain abstraction — no V8 dependency.
enum class JsValueType {
    Undefined,
    Null,
    Boolean,
    Int32,
    Uint32,
    Double,
    String,
    Array,
    Object,
    Function,
    Symbol,
    BigInt,
    Json,  // Raw JSON string, parsed by Python json.loads()
};

/// Forward declaration for recursive types
class JsValue;
using JsValuePtr = std::shared_ptr<JsValue>;
using JsArray = std::vector<JsValuePtr>;
using JsObject = std::map<std::string, JsValuePtr>;

/// Domain-level representation of a JavaScript value.
/// This decouples the domain from V8's v8::Value type.
/// Infrastructure layer converts between v8::Value <-> JsValue.
class JsValue {
public:
    // --- Factory methods ---

    static JsValuePtr make_undefined() {
        return JsValuePtr(new JsValue(UndefinedTag{}));
    }

    static JsValuePtr make_null() {
        return JsValuePtr(new JsValue(NullTag{}));
    }

    static JsValuePtr make_bool(bool v) {
        return JsValuePtr(new JsValue(v));
    }

    static JsValuePtr make_int32(int32_t v) {
        return JsValuePtr(new JsValue(v));
    }

    static JsValuePtr make_uint32(uint32_t v) {
        return JsValuePtr(new JsValue(v));
    }

    static JsValuePtr make_double(double v) {
        return JsValuePtr(new JsValue(v));
    }

    static JsValuePtr make_string(std::string v) {
        return JsValuePtr(new JsValue(std::move(v)));
    }

    static JsValuePtr make_array(JsArray v) {
        JsValuePtr ptr(new JsValue(ArrayTag{}));
        ptr->array_ = std::move(v);
        return ptr;
    }

    static JsValuePtr make_object(JsObject v) {
        JsValuePtr ptr(new JsValue(ObjectTag{}));
        ptr->object_ = std::move(v);
        return ptr;
    }

    static JsValuePtr make_function(std::string name = "") {
        JsValuePtr ptr(new JsValue(FunctionTag{}));
        ptr->function_name_ = std::move(name);
        return ptr;
    }

    static JsValuePtr make_bigint(int64_t v) {
        JsValuePtr ptr(new JsValue(BigIntTag{}));
        ptr->bigint_ = v;
        return ptr;
    }

    // --- Type queries ---

    JsValueType type() const noexcept { return type_; }

    bool is_undefined() const noexcept { return type_ == JsValueType::Undefined; }
    bool is_null() const noexcept { return type_ == JsValueType::Null; }
    bool is_null_or_undefined() const noexcept { return is_null() || is_undefined(); }
    bool is_bool() const noexcept { return type_ == JsValueType::Boolean; }
    bool is_int32() const noexcept { return type_ == JsValueType::Int32; }
    bool is_uint32() const noexcept { return type_ == JsValueType::Uint32; }
    bool is_double() const noexcept { return type_ == JsValueType::Double; }
    bool is_number() const noexcept {
        return is_int32() || is_uint32() || is_double();
    }
    bool is_string() const noexcept { return type_ == JsValueType::String; }
    bool is_array() const noexcept { return type_ == JsValueType::Array; }
    bool is_object() const noexcept { return type_ == JsValueType::Object; }
    bool is_function() const noexcept { return type_ == JsValueType::Function; }
    bool is_bigint() const noexcept { return type_ == JsValueType::BigInt; }

    // --- Value accessors ---

    bool as_bool() const { return bool_; }
    int32_t as_int32() const { return int32_; }
    uint32_t as_uint32() const { return uint32_; }
    double as_double() const { return double_; }
    const std::string& as_string() const { return string_; }
    const JsArray& as_array() const { return array_; }
    const JsObject& as_object() const { return object_; }
    const std::string& function_name() const { return function_name_; }
    int64_t as_bigint() const { return bigint_; }

    /// Convert numeric value to double regardless of numeric subtype
    double to_number() const {
        if (is_int32()) return static_cast<double>(int32_);
        if (is_uint32()) return static_cast<double>(uint32_);
        if (is_double()) return double_;
        return 0.0;
    }

private:
    // Tags for disambiguating constructors
    struct UndefinedTag {};
    struct NullTag {};
    struct ArrayTag {};
    struct ObjectTag {};
    struct FunctionTag {};
    struct BigIntTag {};

    explicit JsValue(UndefinedTag) : type_(JsValueType::Undefined) {}
    explicit JsValue(NullTag) : type_(JsValueType::Null) {}
    explicit JsValue(bool v) : type_(JsValueType::Boolean), bool_(v) {}
    explicit JsValue(int32_t v) : type_(JsValueType::Int32), int32_(v) {}
    explicit JsValue(uint32_t v) : type_(JsValueType::Uint32), uint32_(v) {}
    explicit JsValue(double v) : type_(JsValueType::Double), double_(v) {}
    explicit JsValue(std::string v) : type_(JsValueType::String), string_(std::move(v)) {}
    explicit JsValue(ArrayTag) : type_(JsValueType::Array) {}
    explicit JsValue(ObjectTag) : type_(JsValueType::Object) {}
    explicit JsValue(FunctionTag) : type_(JsValueType::Function) {}
    explicit JsValue(BigIntTag) : type_(JsValueType::BigInt) {}

    JsValueType type_;
    bool bool_ = false;
    int32_t int32_ = 0;
    uint32_t uint32_ = 0;
    double double_ = 0.0;
    std::string string_;
    JsArray array_;
    JsObject object_;
    std::string function_name_;
    int64_t bigint_ = 0;
};

} // namespace ruyijs::domain::runtime_context
