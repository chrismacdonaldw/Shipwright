
#include "Notification.h"
#ifdef DIPTYCH_GAME_MODULE
#include <algorithm>
#include <cmath>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#endif
#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
}

#include <fast/Fast3dGui.h>

namespace Notification {

static uint32_t nextId = 0;
static std::vector<Options> notifications = {};

void Window::Draw() {
    auto vp = ImGui::GetMainViewport();
#ifdef DIPTYCH_GAME_MODULE
    if (ImGui::GetIO().DisplaySize.x <= 0.0f || ImGui::GetIO().DisplaySize.y <= 0.0f) return;
#ifdef _WIN32
    const HWND window = static_cast<HWND>(vp->PlatformHandleRaw);
    if (window == nullptr || !IsWindowVisible(window) || IsIconic(window)) return;
#endif
#endif

    const float margin = 30.0f;
    const float padding = 10.0f;

    int position = CVarGetInteger(CVAR_SETTING("Notifications.Position"), 3);

    // Top Left
    ImVec2 basePosition;
    switch (position) {
        case 0: // Top Left
            basePosition = ImVec2(vp->Pos.x + margin, vp->Pos.y + margin);
            break;
        case 1: // Top Right
            basePosition = ImVec2(vp->Pos.x + vp->Size.x - margin, vp->Pos.y + margin);
            break;
        case 2: // Bottom Left
            basePosition = ImVec2(vp->Pos.x + margin, vp->Pos.y + vp->Size.y - margin);
            break;
        case 3: // Bottom Right
            basePosition = ImVec2(vp->Pos.x + vp->Size.x - margin, vp->Pos.y + vp->Size.y - margin);
            break;
        case 4: // Hidden
            return;
    }

    ImGui::PushStyleColor(ImGuiCol_WindowBg,
                          ImVec4(0, 0, 0, CVarGetFloat(CVAR_SETTING("Notifications.BgOpacity"), 0.5f)));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);

    for (size_t index = 0; index < notifications.size(); ++index) {
        auto& notification = notifications[index];
#ifdef DIPTYCH_GAME_MODULE
        if (notification.durableScope != 0 && notification.remainingTime <= 0.0f) continue;
#endif
        int inverseIndex = ABS(static_cast<int>(index) - (static_cast<int>(notifications.size()) - 1));

        ImGui::SetNextWindowViewport(vp->ID);
#ifdef DIPTYCH_GAME_MODULE
        if (notification.durableScope != 0 && !notification.presentation.acknowledged &&
            notification.presentation.duration < 4.0f) {
            // Short configured durations must still be readable for their entire useful interval.
            const float alpha = std::clamp(notification.remainingTime /
                                              std::min(3.0f, notification.presentation.duration), 0.25f, 1.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        } else
#endif
        if (notification.remainingTime < 4.0f) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, (notification.remainingTime - 1) / 3.0f);
        } else {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
        }

        const bool drew = ImGui::Begin(("notification#" + std::to_string(notification.id)).c_str(), nullptr,
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                         ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);

        ImGui::SetWindowFontScale(CVarGetFloat(CVAR_SETTING("Notifications.Size"), 1.8f)); // Make this adjustable

        ImVec2 notificationPos;
        switch (position) {
            case 0: // Top Left
                notificationPos =
                    ImVec2(basePosition.x, basePosition.y + ((ImGui::GetWindowSize().y + padding) * inverseIndex));
                break;
            case 1: // Top Right
                notificationPos = ImVec2(basePosition.x - ImGui::GetWindowSize().x,
                                         basePosition.y + ((ImGui::GetWindowSize().y + padding) * inverseIndex));
                break;
            case 2: // Bottom Left
                notificationPos = ImVec2(basePosition.x,
                                         basePosition.y - ((ImGui::GetWindowSize().y + padding) * (inverseIndex + 1)));
                break;
            case 3: // Bottom Right
                notificationPos = ImVec2(basePosition.x - ImGui::GetWindowSize().x,
                                         basePosition.y - ((ImGui::GetWindowSize().y + padding) * (inverseIndex + 1)));
                break;
        }

        ImGui::SetWindowPos(notificationPos);

        if (notification.itemIcon != nullptr) {
            ImGui::Image(
                std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui())
                    ->GetTextureByName(notification.itemIcon),
                ImVec2(24, 24));
            ImGui::SameLine();
        }
        if (!notification.prefix.empty()) {
            ImGui::TextColored(notification.prefixColor, "%s", notification.prefix.c_str());
            ImGui::SameLine();
        }
        ImGui::TextColored(notification.messageColor, "%s", notification.message.c_str());
        if (!notification.suffix.empty()) {
            ImGui::SameLine();
            ImGui::TextColored(notification.suffixColor, "%s", notification.suffix.c_str());
        }

#ifdef DIPTYCH_GAME_MODULE
        if (notification.durableScope != 0 && !notification.presentation.acknowledged) {
            const ImVec2 at = ImGui::GetWindowPos();
            const ImVec2 size = ImGui::GetWindowSize();
            const bool onScreen = DiptychNotificationFullyVisible(
                at.x, at.y, size.x, size.y, vp->Pos.x, vp->Pos.y, vp->Size.x, vp->Size.y);
            notification.presentation.Draw(
                ImGui::GetFrameCount(), ImGui::GetTime(), drew && onScreen && ImGui::GetStyle().Alpha > 0.0f);
            notification.remainingTime = std::max(0.0f, notification.presentation.duration - notification.presentation.shown);
        }
#endif
        ImGui::End();
        ImGui::PopStyleVar();
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

void Window::UpdateElement() {
    for (size_t index = 0; index < notifications.size(); ++index) {
        auto& notification = notifications[index];

#ifdef DIPTYCH_GAME_MODULE
        if (notification.durableScope != 0) {
            const double now = ImGui::GetTime();
            if (notification.durableAck != nullptr && notification.presentation.AckDue(now)) {
                // A failed host write can Emit the native save-failure alert, reallocating this vector.
                // Keep callback arguments alive and reacquire our queue entry after the callback returns.
                const uint32_t id = notification.id;
                const uint64_t scope = notification.durableScope;
                const int owner = notification.durableOwner;
                const std::string source = notification.durableSource;
                const auto ack = notification.durableAck;
                const bool committed = ack(scope, owner, source.c_str()) == 1;
                if (index >= notifications.size() || notifications[index].id != id) continue;
                notifications[index].presentation.AckResult(now, committed);
            }
            // Pending lifetime belongs to actual Draw. After a committed ACK, ordinary native expiry resumes.
            if (!notifications[index].presentation.acknowledged) continue;
        }
#endif
        // Refetch after a potentially reentrant durable ACK callback.
        auto& current = notifications[index];
        // decrement remainingTime
        current.remainingTime -= ImGui::GetIO().DeltaTime;

        // remove notification if it has expired
        if (current.remainingTime <= 0) {
            notifications.erase(notifications.begin() + index);
            --index;
        }
    }
}

void Emit(Options notification) {
#ifdef DIPTYCH_GAME_MODULE
    if (notification.durableScope != 0 && std::any_of(notifications.begin(), notifications.end(),
            [&](const Options& queued) {
                return queued.durableScope == notification.durableScope &&
                       queued.durableOwner == notification.durableOwner &&
                       queued.durableSource == notification.durableSource;
            })) return;
#endif
    notification.id = nextId++;
    if (notification.remainingTime == 0.0f) {
        notification.remainingTime = CVarGetFloat(CVAR_SETTING("Notifications.Duration"), 10.0f);
    }
#ifdef DIPTYCH_GAME_MODULE
    if (notification.durableScope != 0) {
        if (!std::isfinite(notification.remainingTime) || notification.remainingTime <= 0.0f) return;
        notification.presentation.duration = notification.remainingTime;
    }
#endif
    notifications.push_back(notification);
    if (!notification.mute && !CVarGetInteger(CVAR_SETTING("Notifications.Mute"), 0)) {
        Audio_PlaySfxGeneral(NA_SE_SY_METRONOME, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }
}

#ifdef DIPTYCH_GAME_MODULE
std::vector<Options> TakeAll() {
    std::vector<Options> taken;
    taken.swap(notifications);
    // The host journal replays durable alerts in the next foreground game.
    taken.erase(std::remove_if(taken.begin(), taken.end(), [](const Options& notification) {
        return notification.durableScope != 0;
    }), taken.end());
    return taken;
}

void SetDurableScope(uint64_t scope) {
    notifications.erase(std::remove_if(notifications.begin(), notifications.end(), [&](const Options& notification) {
        return notification.durableScope != 0 && notification.durableScope != scope;
    }), notifications.end());
}
#endif

} // namespace Notification
