#include "KeyConsumption.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "SceneSwitches.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/QoL/AutosaveFeedback.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/dungeon.h"
#include "soh/SaveManager.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
extern PlayState* gPlayState;
}

namespace AnchorKeyConsumption {
namespace {
Binding sBinding{};
Bank sLoadBaseline{};
int sBaselineFile = -1;
bool sDirty = false;
bool sInFlight = false;
bool sRetryBlocked = false;
AutosaveFeedback sSaveFeedback;

static_assert(kDomains == ARRAY_COUNT(gSaveContext.inventory.dungeonKeys));

void Changed() {
    sDirty = true;
    sRetryBlocked = false;
}

uint32_t Mask(int domain) {
    if (!IS_RANDO || domain < 0 || domain >= kDomains || domain == SCENE_TREASURE_BOX_SHOP ||
        Flags_GetRandomizerInf(RAND_INF_HAS_SKELETON_KEY)) {
        return 0;
    }
    const auto ctx = Rando::Context::GetInstance();
    const auto* dungeon = ctx->GetDungeons()->GetDungeonFromScene(domain);
    if ((dungeon != nullptr && dungeon->HasKeyRing()) ||
        (domain == SCENE_THIEVES_HIDEOUT && ctx->GetOption(RSK_KEYRINGS) &&
         ctx->GetOption(RSK_KEYRINGS_GERUDO_FORTRESS))) {
        return 0;
    }
    uint32_t mask = 0;
    for (const uint8_t flag : Rando::GetSceneSmallKeyDoorFlags(static_cast<SceneID>(domain))) {
        if (AnchorSceneSwitches::Eligible(domain, flag))
            mask |= uint32_t{ 1 } << flag;
    }
    return mask;
}

bool ReadOwner(Owner& owner) {
    return sBinding.ReadOwner != nullptr && sBinding.ReadOwner(owner) && owner.fileNum == gSaveContext.fileNum &&
           owner.fileNum >= 0 && owner.fileNum < 3 && owner.scope[0] != '\0' &&
           std::memchr(owner.scope, '\0', sizeof(owner.scope)) != nullptr;
}

bool Prepare(Owner& owner) {
    if (!ReadOwner(owner))
        return false;
    auto& saved = gSaveContext.ship.keyConsumption;
    if (saved.initialized && std::strcmp(saved.scope, owner.scope) != 0)
        return false;
    if (!saved.initialized) {
        saved = {};
        std::memcpy(saved.scope, owner.scope, sizeof(saved.scope));
        for (int domain = 0; domain < kDomains; ++domain) {
            // Saved banks predate this scene's actor updates and remote projection.
            const uint32_t baseline =
                sBaselineFile == owner.fileNum ? sLoadBaseline[domain] : gSaveContext.sceneFlags[domain].swch;
            saved.accounted[domain] = baseline & Mask(domain);
        }
        saved.initialized = true;
        Changed();
    }
    return true;
}

void Repay(int domain) {
    if (Mask(domain) == 0)
        return;
    auto& stock = gSaveContext.inventory.dungeonKeys[domain];
    auto& owed = gSaveContext.ship.keyConsumption.owed[domain];
    const int paid = std::min<int>(std::max<int>(stock, 0), owed);
    if (paid != 0) {
        stock -= paid;
        owed -= paid;
        Changed();
    }
}

void Frame() {
    sSaveFeedback.ProcessResults([](bool success) {
        sInFlight = false;
        if (!success) {
            sDirty = true;
            sRetryBlocked = true;
        }
    });
    Owner owner;
    if (!Prepare(owner) || !owner.active)
        return;
    auto& saved = gSaveContext.ship.keyConsumption;
    if (owner.admitted && owner.ready && sBinding.Publish != nullptr) {
        for (int domain = 0; domain < kDomains; ++domain) {
            const uint32_t pending = saved.pendingPublish[domain] & Mask(domain);
            for (int flag = 0; flag < 32; ++flag) {
                const uint32_t bit = uint32_t{ 1 } << flag;
                if ((pending & bit) == 0)
                    continue;
                if (!sBinding.Publish(
                        { static_cast<uint8_t>(domain), static_cast<uint8_t>(domain), static_cast<uint8_t>(flag) })) {
                    break;
                }
                Owner current;
                if (!ReadOwner(current) || current.generation != owner.generation || current.fileNum != owner.fileNum ||
                    std::strcmp(current.scope, owner.scope) != 0)
                    return;
                saved.pendingPublish[domain] &= ~bit;
                Changed();
            }
        }
    }
    if (sDirty && !sInFlight && !sRetryBlocked && Play_CanPerformAutomaticSave() &&
        !GameInteractor::IsGameplayPaused()) {
        sDirty = false;
        sInFlight = true;
        Play_PerformSaveWithCompletion(gPlayState, AutosaveFeedback::CompleteSave, sSaveFeedback.BeginSave());
    }
}

void InitSave(bool) {
    gSaveContext.ship.keyConsumption = {};
    sBaselineFile = -1;
    Reset();
}

void LoadSave() {
    auto& saved = gSaveContext.ship.keyConsumption;
    std::string scope;
    SaveManager::Instance->LoadData("scope", scope);
    if (scope.empty() || scope.size() >= sizeof(saved.scope)) {
        saved.initialized = true; // Refuse this section instead of forgiving invalid receipts.
        return;
    }
    std::memcpy(saved.scope, scope.c_str(), scope.size() + 1);
    SaveManager::Instance->LoadArray(
        "accounted", kDomains, [&](size_t domain) { SaveManager::Instance->LoadData("", saved.accounted[domain]); });
    SaveManager::Instance->LoadArray("pendingPublish", kDomains, [&](size_t domain) {
        SaveManager::Instance->LoadData("", saved.pendingPublish[domain]);
    });
    SaveManager::Instance->LoadArray("owed", kDomains,
                                     [&](size_t domain) { SaveManager::Instance->LoadData("", saved.owed[domain]); });
    for (int domain = 0; domain < kDomains; ++domain) {
        if (saved.owed[domain] > 32 || (saved.pendingPublish[domain] & ~saved.accounted[domain]) != 0) {
            saved = {};
            saved.initialized = true;
            return;
        }
    }
    saved.initialized = true;
}

void Save(const SaveContext& context, int, bool) {
    const auto& saved = context.ship.keyConsumption;
    if (!saved.initialized)
        return;
    SaveManager::Instance->SaveData("scope", std::string(saved.scope));
    SaveManager::Instance->SaveArray(
        "accounted", kDomains, [&](size_t domain) { SaveManager::Instance->SaveData("", saved.accounted[domain]); });
    SaveManager::Instance->SaveArray("pendingPublish", kDomains, [&](size_t domain) {
        SaveManager::Instance->SaveData("", saved.pendingPublish[domain]);
    });
    SaveManager::Instance->SaveArray("owed", kDomains,
                                     [&](size_t domain) { SaveManager::Instance->SaveData("", saved.owed[domain]); });
}

void Register() {
    SaveManager::Instance->AddInitFunction(InitSave);
    SaveManager::Instance->AddLoadFunction("anchorKeyConsumption", 1, LoadSave);
    SaveManager::Instance->AddSaveFunction("anchorKeyConsumption", 1, Save, true, SECTION_PARENT_NONE);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadFile>([](int16_t fileNum) {
        sBaselineFile = fileNum;
        for (int domain = 0; domain < kDomains; ++domain)
            sLoadBaseline[domain] = gSaveContext.sceneFlags[domain].swch;
        sDirty = false;
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(Frame);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneSpawnActors>([]() { sRetryBlocked = false; });
}
RegisterShipInitFunc sRegister(Register);
} // namespace

void SetBinding(const Binding* binding) {
    sBinding = binding != nullptr ? *binding : Binding{};
    Reset();
}

void Reset() {
    sDirty = false;
    sInFlight = false;
    sRetryBlocked = false;
    sSaveFeedback.Clear();
}

bool Apply(const Bank& admitted) {
    Owner owner;
    if (!Prepare(owner) || !owner.active || !owner.admitted || !owner.ready)
        return false;
    auto& saved = gSaveContext.ship.keyConsumption;
    for (int domain = 0; domain < kDomains; ++domain) {
        const uint32_t added = admitted[domain] & Mask(domain) & ~saved.accounted[domain];
        for (int flag = 0; flag < 32; ++flag) {
            if ((added & (uint32_t{ 1 } << flag)) == 0)
                continue;
            saved.accounted[domain] |= uint32_t{ 1 } << flag;
            ++saved.owed[domain];
            Changed();
        }
        Repay(domain);
    }
    return true;
}
} // namespace AnchorKeyConsumption

extern "C" void AnchorKeyConsumption_Consume(PlayState* play, uint16_t domain, uint16_t flag) {
    if (domain >= AnchorKeyConsumption::kDomains)
        return;
    auto& stock = gSaveContext.inventory.dungeonKeys[domain];
    const int before = stock;
    --stock;
    using namespace AnchorKeyConsumption;
    Owner owner;
    if (play == nullptr || play != gPlayState || play->sceneNum != domain || flag >= 32 || before < 0 ||
        (Mask(domain) & (uint32_t{ 1 } << flag)) == 0 || !Prepare(owner) || !owner.active)
        return;
    auto& saved = gSaveContext.ship.keyConsumption;
    const uint32_t bit = uint32_t{ 1 } << flag;
    if ((saved.accounted[domain] & bit) != 0) {
        ++stock; // A concurrent local opening of the same admitted lock costs once.
    } else {
        saved.accounted[domain] |= bit;
        saved.pendingPublish[domain] |= bit;
        if (stock < 0) {
            stock = 0;
            ++saved.owed[domain];
        }
    }
    Changed();
}

extern "C" void AnchorKeyConsumption_Granted(uint16_t domain) {
    using namespace AnchorKeyConsumption;
    Owner owner;
    if (domain < kDomains && Prepare(owner) && owner.active)
        Repay(domain);
}
