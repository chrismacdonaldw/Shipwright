#pragma once

#include <array>
#include <functional>
#include <string>

#include <nlohmann/json.hpp>

using RandomizerHash = std::array<std::string, 5>;

// RANDOTODO this is primarily used by the check tracker now, and should probably be moved
typedef enum {
    SPOILER_CHK_NONE,
    SPOILER_CHK_CHEST,
    SPOILER_CHK_COLLECTABLE,
    SPOILER_CHK_GOLD_SKULLTULA,
    SPOILER_CHK_ITEM_GET_INF,
    SPOILER_CHK_EVENT_CHK_INF,
    SPOILER_CHK_INF_TABLE,
    SPOILER_CHK_GRAVEDIGGER,
    SPOILER_CHK_RANDOMIZER_INF,
} SpoilerCollectionCheckType;

void GenerateHash();

void SpoilerLog_Write();

bool SpoilerLog_WriteDiptych(const std::string& path, const std::function<void(nlohmann::ordered_json&)>& patch);
