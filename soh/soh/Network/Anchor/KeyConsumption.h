#pragma once

#include <stdint.h>

struct PlayState;

#ifdef __cplusplus
#include <array>

namespace AnchorKeyConsumption {
inline constexpr int kDomains = 19;
using Bank = std::array<uint32_t, kDomains>;

struct Owner {
    uint64_t generation = 0;
    int fileNum = -1;
    char scope[64]{};
    bool active = false;
    bool admitted = false;
    bool sharing = false;
    bool ready = false;
};

struct Lock {
    uint8_t domain;
    uint8_t scene;
    uint8_t flag;
};

// Both callbacks run synchronously on the game thread. ReadOwner identifies the
// loaded file even during admission holds; Publish false retains a saved retry.
struct Binding {
    bool (*ReadOwner)(Owner&) = nullptr;
    bool (*Publish)(const Lock&) = nullptr;
};

// Copies the binding. Reset/unbind clears transient ownership, not saved receipts.
void SetBinding(const Binding* binding);
void Reset();
// Call before projecting admitted scene switches. An empty bank establishes the
// old-save baseline without publishing historical unlocks as consumption.
bool Apply(const Bank& admitted);
} // namespace AnchorKeyConsumption

extern "C" {
#endif
// Replaces only an actual native decrement; existing legacy key-used hooks stay.
void AnchorKeyConsumption_Consume(struct PlayState* play, uint16_t domain, uint16_t flag);
// Called immediately after an additive grant, before renewed stock is usable.
void AnchorKeyConsumption_Granted(uint16_t domain);
#ifdef __cplusplus
}
#endif
