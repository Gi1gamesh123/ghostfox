#pragma once

#include <stdexcept>
#include <string>

namespace ruyijs::domain::shared {

/// Base exception for all domain rule violations.
/// Thrown when an operation violates a domain invariant.
class DomainException : public std::logic_error {
public:
    explicit DomainException(const std::string& message)
        : std::logic_error(message) {}

    explicit DomainException(const char* message)
        : std::logic_error(message) {}
};

/// Thrown when an entity is not found in a repository
class EntityNotFoundException : public DomainException {
public:
    explicit EntityNotFoundException(const std::string& entity_type,
                                     const std::string& id)
        : DomainException(entity_type + " not found: " + id),
          entity_type_(entity_type), entity_id_(id) {}

    const std::string& entity_type() const noexcept { return entity_type_; }
    const std::string& entity_id() const noexcept { return entity_id_; }

private:
    std::string entity_type_;
    std::string entity_id_;
};

/// Thrown when an operation is performed on an entity in an invalid state
class InvalidStateException : public DomainException {
public:
    explicit InvalidStateException(const std::string& message)
        : DomainException(message) {}
};

/// Thrown when a value object receives invalid arguments
class InvalidArgumentException : public DomainException {
public:
    explicit InvalidArgumentException(const std::string& message)
        : DomainException(message) {}
};

} // namespace ruyijs::domain::shared
