#include <nlohmann/json.hpp>

#include "soh/Network/Anchor/Anchor.h"
#include "soh/OTRGlobals.h"
#include "soh/Enhancements/randomizer/randomizer_check_tracker.h"
#include "soh/Enhancements/randomizer/randomizer.h"
#include "soh/Enhancements/randomizer/randomizerEnumStrings.h"

static bool isResultOfHandling = false;

/**
 * SET_CHECK_STATUS
 *
 * Fired when a check status is updated or skipped
 */

void Anchor::SendPacket_SetCheckStatus(RandomizerCheck rc) {
#ifdef DIPTYCH_GAME_MODULE
    if (!SyncOn()) return;
#endif
    if (!IsSaveLoaded() || isResultOfHandling) {
        return;
    }

    auto randoContext = Rando::Context::GetInstance();

    nlohmann::json payload;
    payload["type"] = SET_CHECK_STATUS;
    payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    payload["addToQueue"] = true;
    payload["rc"] = rc;
    payload["status"] = randoContext->GetItemLocation(rc)->GetCheckStatus();
    payload["skipped"] = randoContext->GetItemLocation(rc)->GetIsSkipped();
    payload["quiet"] = true;

    SendJsonToRemote(payload);
}

// The paired-save bridge calls this same native effect under its owning-save identity gate.
// Raw stock packets still require SyncOn and cannot cross paired item/receipt ownership.
bool Anchor_ApplyCheckMetadata(RandomizerCheck rc, int status, bool skipped, bool protectObtained) {
    if (rc < 0 || rc >= RC_MAX) return false;
    auto location = Rando::Context::GetInstance()->GetItemLocation(rc);
    bool changed = false;
    const bool handling = isResultOfHandling;
    isResultOfHandling = true;
    if (status >= 0 && (!protectObtained || !location->HasObtained()) &&
        location->GetCheckStatus() != status) {
        location->SetCheckStatus(static_cast<RandomizerCheckStatus>(status));
        changed = true;
    }
    if (location->GetIsSkipped() != skipped) {
        location->SetIsSkipped(skipped);
        changed = true;
    }
    isResultOfHandling = handling;
    return changed;
}

void Anchor::HandlePacket_SetCheckStatus(nlohmann::json payload) {
    if (!IsSaveLoaded() || !SyncOn()) return;
    RandomizerCheck rc = payload.at("rc").get<RandomizerCheck>();
    if (rc < 0 || rc >= RC_MAX) {
        SPDLOG_ERROR("[Anchor] SET_CHECK_STATUS: {} out of range", rc);
        return;
    }
    Anchor_ApplyCheckMetadata(rc, payload.at("status").get<int>(), payload.at("skipped").get<bool>(), false);
    CheckTracker::RecalculateAllAreaTotals();
    CheckTracker::RecalculateAvailableChecks();
}
