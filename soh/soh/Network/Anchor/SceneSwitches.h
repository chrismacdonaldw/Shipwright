#pragma once
#include <array>
#include <cstdint>

struct PlayState;
namespace AnchorSceneSwitches {
inline constexpr int kScenes = 0x6e;
using Bank = std::array<uint32_t, kScenes>;
uint32_t Mask(int scene);
bool Eligible(int scene, int bit);
// Callers additionally prove loaded paired identity. Never capture a parked/global dormant save.
bool Capture(PlayState* play, Bank& out);
bool Apply(PlayState* play, const Bank& value, const Bank& pending);
}
