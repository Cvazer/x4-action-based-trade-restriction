// Base non-owning handle for translating engine memory addresses and offsets into C++ domain types.

#pragma once

#include <cstddef>
#include <cstdint>

namespace action_based_trade_restriction::game {

// Dereferences a global pointer slot exposed by X4Native.
inline void* dereference_slot(void* slot) {
    return slot ? *static_cast<void**>(slot) : nullptr;
}

// Non-owning handle to an engine object in memory.
class Handle {
public:
    explicit Handle(void* object = nullptr) : object_(object) {}

    void* object() const { return object_; }
    explicit operator bool() const { return object_ != nullptr; }
    bool operator==(const Handle& other) const { return object_ == other.object_; }

protected:
    template <typename T>
    T& field(std::size_t offset) const {
        return *reinterpret_cast<T*>(static_cast<std::uint8_t*>(object_) + offset);
    }

    template <typename Function>
    Function method(std::size_t slot) const {
        return reinterpret_cast<Function>(field<void**>(0)[slot]);
    }

private:
    void* object_;
};

}  // namespace action_based_trade_restriction::game
