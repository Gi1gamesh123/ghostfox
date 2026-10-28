#pragma once

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

namespace ruyijs::domain::shared {

/// Base class for all Entities.
/// An Entity is defined by its identity (ID), not its attributes.
template <typename ID>
class Entity {
public:
    using IdType = ID;

    explicit Entity(ID id) : id_(std::move(id)) {
        if (id_ == ID{}) {
            throw std::invalid_argument("Entity ID must not be empty/default");
        }
    }

    virtual ~Entity() = default;

    const ID& id() const noexcept { return id_; }

    bool operator==(const Entity& other) const noexcept {
        return id_ == other.id_;
    }
    bool operator!=(const Entity& other) const noexcept {
        return !(*this == other);
    }

protected:
    // Entities are not copyable by default, but movable
    Entity(const Entity&) = default;
    Entity& operator=(const Entity&) = default;
    Entity(Entity&&) noexcept = default;
    Entity& operator=(Entity&&) noexcept = default;

private:
    ID id_;
};

} // namespace ruyijs::domain::shared

// Hash support for entities by ID
namespace std {
template <typename ID>
struct hash<ruyijs::domain::shared::Entity<ID>> {
    size_t operator()(const ruyijs::domain::shared::Entity<ID>& e) const {
        return hash<ID>{}(e.id());
    }
};
} // namespace std
