#pragma once

#include <cstdint>
#include <ostream>

namespace csics::sim::ecs {
struct Entity {
    uint32_t id;
    uint32_t generation;

    Entity() {
        id = std::numeric_limits<uint32_t>::max();
        generation = std::numeric_limits<uint32_t>::max();
    }
    Entity(const Entity& other) = default;
    Entity& operator=(const Entity& other) = default;
    explicit Entity(std::uint64_t id) {
        this->id = id & 0xFFFFFFFF;  // lower 32 bits for ID
        this->generation = (id >> 32) & 0xFFFFFFFF;  // upper 32 bits for generation
    }
    Entity(std::uint8_t generation, std::uint32_t id) {
        this->id = id;
        this->generation = generation;
    }

    bool valid() {
        return id != std::numeric_limits<uint32_t>::max() &&
               generation != std::numeric_limits<uint32_t>::max();
    }

    bool operator==(const Entity& other) {
        return id == other.id && generation == other.generation;
    }

    bool operator!=(const Entity& other) {
        return !(*this == other);
    }

    friend std::ostream& operator<<(std::ostream& os, const Entity& entity) {
        os << "Entity{id: " << entity.id << ", generation: " << entity.generation
           << "}";
        return os;
    }
};
};

