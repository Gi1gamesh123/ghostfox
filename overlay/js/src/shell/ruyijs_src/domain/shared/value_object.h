#pragma once

namespace ruyijs::domain::shared {

/// Base class for Value Objects.
/// A Value Object is defined by its attributes (structural equality),
/// is immutable, and has no identity.
///
/// Derived classes MUST:
///   1. Implement operator== comparing all fields
///   2. Be immutable (no setters, all fields const or logically const)
///   3. Provide a hash() method for use in containers
class ValueObject {
public:
    virtual ~ValueObject() = default;

    /// Structural equality — derived classes must override
    virtual bool equals(const ValueObject& other) const = 0;

    bool operator==(const ValueObject& other) const { return equals(other); }
    bool operator!=(const ValueObject& other) const { return !equals(other); }

    /// Hash for use in unordered containers
    virtual size_t hash() const = 0;

protected:
    ValueObject() = default;
    ValueObject(const ValueObject&) = default;
    ValueObject& operator=(const ValueObject&) = default;
    ValueObject(ValueObject&&) noexcept = default;
    ValueObject& operator=(ValueObject&&) noexcept = default;
};

} // namespace ruyijs::domain::shared

namespace std {
template <>
struct hash<ruyijs::domain::shared::ValueObject> {
    size_t operator()(const ruyijs::domain::shared::ValueObject& vo) const {
        return vo.hash();
    }
};
} // namespace std
