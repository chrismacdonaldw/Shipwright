#include "SceneSwitches.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
extern "C" {
#include "variables.h"
#include "z64scene.h"
}

namespace AnchorSceneSwitches {
static_assert(SCENE_ID_MAX == kScenes);
static_assert(SCENE_FOREST_TEMPLE == 3 && SCENE_WATER_TEMPLE == 5 && SCENE_GANONS_TOWER_COLLAPSE_EXTERIOR == 0x1a);
uint32_t Mask(int scene) {
    if (scene < 0 || scene >= kScenes) return 0;
    switch (scene) {
        case SCENE_WATER_TEMPLE: return ~(uint32_t{7} << 0x1c);
        case SCENE_FOREST_TEMPLE: return ~(uint32_t{1} << 0x1b);
        case SCENE_GANONS_TOWER_COLLAPSE_EXTERIOR: return ~(uint32_t{1} << 0x17);
        default: return UINT32_MAX;
    }
}
bool Eligible(int scene, int bit) {
    return bit >= 0 && bit < 32 && (Mask(scene) & (uint32_t{1} << bit)) != 0;
}
namespace {
bool Running(PlayState* play) {
    return play != nullptr && play == gPlayState && play->state.running &&
           gSaveContext.gameMode == GAMEMODE_NORMAL && play->sceneNum >= 0 && play->sceneNum < kScenes &&
           play->transitionTrigger == TRANS_TRIGGER_OFF && play->transitionMode == TRANS_MODE_OFF;
}
}
bool Capture(PlayState* play, Bank& out) {
    if (!Running(play)) return false;
    for (int scene = 0; scene < kScenes; ++scene) out[scene] = gSaveContext.sceneFlags[scene].swch & Mask(scene);
    out[play->sceneNum] = play->actorCtx.flags.swch & Mask(play->sceneNum);
    return true;
}
bool Apply(PlayState* play, const Bank& value, const Bank& pending) {
    if (!Running(play)) return false;
    for (int scene = 0; scene < kScenes; ++scene) {
        const uint32_t saved = gSaveContext.sceneFlags[scene].swch;
        const uint32_t current = scene == play->sceneNum ? play->actorCtx.flags.swch : saved;
        const uint32_t changed = ((saved ^ value[scene]) | (current ^ value[scene])) & Mask(scene) & ~pending[scene];
        for (int bit = 0; bit < 32; ++bit) if ((changed & (uint32_t{1} << bit)) != 0) {
            // Native RawAction updates saved/current banks without producer or reward hooks.
            // Existing connected Anchor actor follow-ups retain their normal polling/re-entry behavior.
            if ((value[scene] & (uint32_t{1} << bit)) != 0)
                GameInteractor::RawAction::SetSceneFlag(scene, FlagType::FLAG_SCENE_SWITCH, bit);
            else GameInteractor::RawAction::UnsetSceneFlag(scene, FlagType::FLAG_SCENE_SWITCH, bit);
        }
    }
    return true;
}
}
