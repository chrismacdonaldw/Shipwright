#pragma once

#ifdef __cplusplus

#include <string>
#include <ship/window/gui/GuiWindow.h>
#ifdef DIPTYCH_GAME_MODULE
#include <vector>
#include "diptych_notification.h"
#endif

namespace Notification {

struct Options {
    uint32_t id = 0;
    const char* itemIcon = nullptr;
    std::string prefix = "";
    ImVec4 prefixColor = ImVec4(0.5f, 0.5f, 1.0f, 1.0f);
    std::string message = "";
    ImVec4 messageColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    std::string suffix = "";
    ImVec4 suffixColor = ImVec4(1.0f, 0.5f, 0.5f, 1.0f);
    float remainingTime = 0.0f; // Seconds
    bool mute = false;          // whether notification should make a noise
#ifdef DIPTYCH_GAME_MODULE
    uint64_t durableScope = 0;
    int durableOwner = -1;
    std::string durableSource;
    DiptychNotificationPresentation presentation;
    int (*durableAck)(uint64_t scope, int owner, const char* source) = nullptr;
#endif
};

class Window final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override{};
    void DrawElement() override{};
    void Draw() override;
    void UpdateElement() override;
};

void Emit(Options notification);

#ifdef DIPTYCH_GAME_MODULE
std::vector<Options> TakeAll();
void SetDurableScope(uint64_t scope);
#endif

} // namespace Notification

#endif // __cplusplus
