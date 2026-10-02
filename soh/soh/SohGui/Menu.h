#pragma once

#include <libultraship/libultra.h>
#include <ship/audio/Audio.h>
#include <ship/window/gui/GuiWindow.h>
#include <fast/Fast3dWindow.h>
#include "MenuTypes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <functional>
#include <algorithm>

namespace Ship {
uint32_t GetVectorIndexOf(std::vector<std::string>& vector, std::string value);
class Menu : public GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    Menu(const std::string& cVar, const std::string& name, uint8_t searchSidebarIndex_ = 0,
         UIWidgets::Colors menuThemeIndex_ = UIWidgets::Colors::LightBlue);

    void InitElement() override;
    void DrawElement() override;
    void UpdateElement() override;
    void Draw() override;
    void InsertSidebarSearch();
    void RemoveSidebarSearch();
    void UpdateAudioBackendObjects();
    void UpdateWindowBackendObjects();
    bool IsMenuPopped();
    UIWidgets::Colors GetMenuThemeColor();

    void MenuDrawItem(WidgetInfo& widget, UIWidgets::Colors menuThemeIndex);
    void AddMenuEntry(std::string entryName, const char* entryCvar);
    void AddSearchWidget(SearchWidget widget);
    std::unordered_map<uint32_t, disabledInfo>& GetDisabledMap();
    struct SidebarNavigationEntry {
        std::string id, label, group;
        bool pinned = false;
    };
    struct NativeSidebarEntry {
        std::string name;
        bool runtimeTool;
    };
    std::vector<NativeSidebarEntry> GetNativeSidebars(const std::string& section) const {
        std::vector<NativeSidebarEntry> entries;
        if (!menuEntries.contains(section)) return entries;
        for (const auto& name : menuEntries.at(section).sidebarOrder) entries.push_back({name, false});
        if (const auto found = runtimeSidebarMetadata.find(section); found != runtimeSidebarMetadata.end())
            for (const auto& name : found->second) entries.push_back({name, true});
        return entries;
    }
    void SetMenuEntryLabel(const std::string& section, const std::string& label) {
        if (menuEntries.contains(section)) menuEntries.at(section).label = label;
    }
    void PlaceMenuEntryAfter(const std::string& section, const std::string& previous) {
        if (!menuEntries.contains(section) || !menuEntries.contains(previous) || section == previous) return;
        menuOrder.erase(std::remove(menuOrder.begin(), menuOrder.end(), section), menuOrder.end());
        const auto position = std::find(menuOrder.begin(), menuOrder.end(), previous);
        if (position != menuOrder.end()) menuOrder.insert(position + 1, section);
    }
    // Move existing tracker widgets intact; keep their original update-function registration key.
    void MoveRuntimeSidebar(const std::string& section, const std::string& sidebar, const std::string& target) {
        if (!menuEntries.contains(section) || !menuEntries.contains(target)) return;
        auto& source = menuEntries.at(section);
        auto& destination = menuEntries.at(target);
        if (!source.sidebars.contains(sidebar) || destination.sidebars.contains(sidebar)) return;
        destination.sidebars.emplace(sidebar, std::move(source.sidebars.at(sidebar)));
        source.sidebars.erase(sidebar);
        source.sidebarOrder.erase(std::remove(source.sidebarOrder.begin(), source.sidebarOrder.end(), sidebar),
                                  source.sidebarOrder.end());
        destination.sidebarOrder.push_back(sidebar);
        runtimeSidebarOrigins[target][sidebar] = section;
        runtimeSidebarMetadata[section].push_back(sidebar);
    }
    bool DrawSidebarNavigation(const std::vector<SidebarNavigationEntry>& entries, std::string& selected,
                               ImVec2& pos, ImVec2& size, float sectionHeight,
                               const char* navigationId = "Randomizer Sidebar");
    void DrawSectionContent(const std::string& section, ImVec2 pos, ImVec2 size, float sectionHeight,
                            const char* search = nullptr, bool dormant = false, bool sectionSearch = false);
    // The provider composes navigation; native content keeps its own widgets and callbacks.
#ifdef DIPTYCH_GAME_MODULE
    void SetMenuSearch(const char* text) { menuSearch = ImGuiTextFilter(text ? text : ""); }
    bool IsMenuSearchEmpty() const { return menuSearch.InputBuf[0] == 0; }
    bool SelectSidebar(const std::string& section, const std::string& sidebar) {
        if (!menuEntries.contains(section) || !menuEntries.at(section).sidebars.contains(sidebar)) return false;
        CVarSetString(menuEntries.at(section).sidebarCvar, sidebar.c_str());
        return true;
    }
    void HideMenuEntryHeader(const std::string& section) {
        menuOrder.erase(std::remove(menuOrder.begin(), menuOrder.end(), section), menuOrder.end());
    }
    void SetSectionProvider(const std::string& section,
                            std::function<void(ImVec2, ImVec2, const char*)> draw) {
        sectionProviders[section] = std::move(draw);
    }
#endif
    void DrawSectionBody(const std::string& section, ImVec2 pos, ImVec2 size, float sectionHeight,
                         const char* search = nullptr, bool dormant = false, bool sectionSearch = false);

  protected:
    ImVec2 mOriginalSize;
    std::string mName;
    uint32_t mWindowFlags;
    std::unordered_map<std::string, MainMenuEntry> menuEntries;
    std::vector<std::string> menuOrder;
    uint32_t DrawSearchResults(std::string& menuSearchText);
    ImGuiTextFilter menuSearch;
    uint8_t searchSidebarIndex;
    UIWidgets::Colors defaultThemeIndex;
    std::shared_ptr<std::vector<int32_t>> availableWindowBackends;
    std::map<Fast::WindowBackend, const char*> availableWindowBackendsMap;
    Fast::WindowBackend configWindowBackend;
    std::shared_ptr<std::vector<Ship::AudioBackend>> availableAudioBackends;
    std::map<Ship::AudioBackend, const char*> availableAudioBackendsMap;

    std::unordered_map<uint32_t, disabledInfo> disabledMap;
    std::vector<disabledInfo> disabledVector;
    const SidebarEntry searchSidebarEntry = {
        .columnCount = 1,
        .columnWidgets = { { { .name = "Sidebar Search",
                               .type = WIDGET_SEARCH,
                               .options = std::make_shared<UIWidgets::WidgetOptions>(UIWidgets::WidgetOptions{}.Tooltip(
                                   "Searches all menus for the given text, including tooltips.")) } } }
    };

  private:
#ifdef DIPTYCH_GAME_MODULE
    std::unordered_map<std::string, std::function<void(ImVec2, ImVec2, const char*)>> sectionProviders;
#endif
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> runtimeSidebarOrigins;
    std::unordered_map<std::string, std::vector<std::string>> runtimeSidebarMetadata;
    std::unordered_map<std::string, std::string> sidebarSelections;
    bool dormantBody = false;
    std::string searchSection;
    bool allowPopout = true; // PortNote: should be set to false on small screen ports
    bool popped;
    ImVec2 poppedSize;
    ImVec2 poppedPos;
    float windowHeight;
    float windowWidth;
    UIWidgets::Colors menuThemeIndex;
};
} // namespace Ship
