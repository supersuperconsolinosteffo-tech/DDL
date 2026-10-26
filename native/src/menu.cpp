#include "menu.hpp"

#include <windows.h>

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <unordered_set>
#include <vector>

#include "app_state.hpp"
#include "jni_bridge.hpp"
#include "module.hpp"
#include "packet_queue.hpp"
#include "tick_gate.hpp"

namespace uc::menu {
namespace {

// --- Design tokens (single source of truth for the overlay look) ---
// Neutral near-black surfaces, one desaturated green accent used ONLY for
// "enabled/active" meaning, one muted red used ONLY for errors. No gradients.
const ImVec4 kBg{0.063f, 0.071f, 0.078f, 0.97f};        // #101214
const ImVec4 kPanel{0.086f, 0.094f, 0.110f, 0.98f};     // #16181C
const ImVec4 kPanelHover{0.125f, 0.137f, 0.157f, 1.0f};
const ImVec4 kBorder{0.165f, 0.180f, 0.208f, 1.0f};     // #2A2E35
const ImVec4 kText{0.902f, 0.910f, 0.918f, 1.0f};       // #E6E8EA
const ImVec4 kMuted{0.608f, 0.639f, 0.671f, 1.0f};      // #9BA3AB
const ImVec4 kFaint{0.420f, 0.443f, 0.471f, 1.0f};
const ImVec4 kAccent{0.561f, 0.690f, 0.353f, 1.0f};     // muted green = ON
const ImVec4 kAccentDim{0.561f, 0.690f, 0.353f, 0.25f};
const ImVec4 kDanger{0.776f, 0.357f, 0.306f, 1.0f};     // errors only
const ImVec4 kWarn{0.780f, 0.635f, 0.290f, 1.0f};       // paused gate only
constexpr float kPanelWidth = 232.0f;
constexpr float kRowHeight = 26.0f;
constexpr float kRadius = 3.0f;

ImFont* g_fontHeader = nullptr;
char g_search[64] = {};
bool g_collapsed[static_cast<int>(Category::Count)] = {};
std::unordered_set<std::string> g_expanded;

ImVec4 WithAlpha(ImVec4 c, float a) { c.w = a; return c; }

std::string Upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

bool MatchesSearch(const Module& m) {
    if (g_search[0] == '\0') return true;
    std::string name = m.Name(), q = g_search;
    std::transform(name.begin(), name.end(), name.begin(), [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
    std::transform(q.begin(), q.end(), q.begin(), [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
    return name.find(q) != std::string::npos;
}

void RenderSettings(Module& m) {
    std::lock_guard lock(m.SettingsMutex());
    for (Setting& s : m.Settings()) {
        ImGui::PushID(s.key.c_str());
        ImGui::Indent(10.0f);
        switch (s.type) {
            case Setting::Type::Bool:
                if (ImGui::Checkbox(s.label.c_str(), &s.boolValue)) ModuleManager::Get().SaveState();
                break;
            case Setting::Type::Float:
                ImGui::TextColored(kMuted, "%s", s.label.c_str());
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 10.0f);
                ImGui::SliderFloat("##v", &s.floatValue, s.floatMin, s.floatMax, "%.2f");
                if (ImGui::IsItemDeactivatedAfterEdit()) ModuleManager::Get().SaveState();
                break;
            case Setting::Type::Int:
                ImGui::TextColored(kMuted, "%s", s.label.c_str());
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 10.0f);
                ImGui::SliderInt("##v", &s.intValue, s.intMin, s.intMax);
                if (ImGui::IsItemDeactivatedAfterEdit()) ModuleManager::Get().SaveState();
                break;
        }
        ImGui::Unindent(10.0f);
        ImGui::PopID();
    }
}

void RenderModuleRow(Module& m) {
    ImGui::PushID(m.Id().c_str());
    const bool on = m.DesiredEnabled();
    const bool hasSettings = !m.Settings().empty();
    const bool expanded = g_expanded.count(m.Id()) != 0;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    ImGui::InvisibleButton("##row", ImVec2(width, kRowHeight));
    const bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        m.SetDesiredEnabled(!on);
        ModuleManager::Get().SaveState();
    }
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && hasSettings) {
        if (expanded) g_expanded.erase(m.Id()); else g_expanded.insert(m.Id());
    }
    if (hovered && !expanded) ImGui::SetTooltip("%s\n%s", m.Name().c_str(), m.Description().c_str());

    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (hovered) dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + kRowHeight), ImGui::GetColorU32(kPanelHover), kRadius);
    if (on) dl->AddRectFilled(pos, ImVec2(pos.x + 2.0f, pos.y + kRowHeight), ImGui::GetColorU32(kAccent));
    const float textY = pos.y + (kRowHeight - ImGui::GetFontSize()) * 0.5f;
    dl->AddText(ImVec2(pos.x + 10.0f, textY), ImGui::GetColorU32(on ? kText : kMuted), m.Name().c_str());
    const char* state = on ? "ON" : "OFF";
    const float stateW = ImGui::CalcTextSize(state).x;
    float rightX = pos.x + width - 8.0f - stateW;
    if (hasSettings) {
        const char* glyph = expanded ? "-" : "+";
        dl->AddText(ImVec2(rightX - 14.0f, textY), ImGui::GetColorU32(kFaint), glyph);
    }
    dl->AddText(ImVec2(rightX, textY), ImGui::GetColorU32(on ? kAccent : kFaint), state);

    if (expanded && hasSettings) RenderSettings(m);
    ImGui::PopID();
}

void RenderCategoryPanel(Category cat, float initialX) {
    const int idx = static_cast<int>(cat);
    const bool searching = g_search[0] != '\0';
    std::vector<Module*> mods;
    for (auto& m : ModuleManager::Get().All()) {
        if (m->GetCategory() == cat && MatchesSearch(*m)) mods.push_back(m.get());
    }
    ImGui::SetNextWindowPos(ImVec2(initialX, 52.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(kPanelWidth, 0), ImVec2(kPanelWidth, 560.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kPanel);
    char title[64];
    std::snprintf(title, sizeof(title), "##cat_%d", idx);
    if (ImGui::Begin(title, nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
        // Header: uppercase micro-label + count + collapse toggle.
        if (g_fontHeader) ImGui::PushFont(g_fontHeader);
        ImGui::TextColored(kText, "%s", Upper(CategoryName(cat)).c_str());
        if (g_fontHeader) ImGui::PopFont();
        ImGui::SameLine();
        ImGui::TextColored(kFaint, "%d", static_cast<int>(mods.size()));
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 6.0f);
        if (ImGui::SmallButton(g_collapsed[idx] && !searching ? "+" : "-")) g_collapsed[idx] = !g_collapsed[idx];
        ImGui::Separator();
        if (!g_collapsed[idx] || searching) {
            if (mods.empty()) {
                ImGui::TextColored(kFaint, "No modules");
            } else {
                for (Module* m : mods) RenderModuleRow(*m);
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

void RenderStatusStrip() {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(display.x, 40.0f), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kBg);
    if (ImGui::Begin("##status", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar)) {
        if (g_fontHeader) ImGui::PushFont(g_fontHeader);
        ImGui::TextColored(kText, "UTILITYCLIENT");
        if (g_fontHeader) ImGui::PopFont();
        ImGui::SameLine();
        ImGui::TextColored(kMuted, "1.21.11");
        ImGui::SameLine(0, 20.0f);

        const auto& refs = jni::Refs();
        if (!refs.ready) {
            ImGui::TextColored(kWarn, "resolving mappings...");
        } else if (!refs.versionOk) {
            ImGui::TextColored(kDanger, "wrong game version (%s) - packets OFF", refs.versionString.c_str());
        } else if (tick_gate::ActiveMode() == tick_gate::Mode::Bridge) {
            ImGui::TextColored(kAccent, "tick gate: bridge");
        } else if (tick_gate::ActiveMode() == tick_gate::Mode::JvmtiBreakpoint) {
            ImGui::TextColored(kAccent, "tick gate: jvmti (exp.)");
        } else if (!tick_gate::BridgeSeen()) {
            ImGui::TextColored(kWarn, "no tick gate - install bridge mod (packets paused)");
        } else {
            ImGui::TextColored(kWarn, "no tick gate (packets paused)");
        }
        ImGui::SameLine(0, 20.0f);
        ImGui::TextColored(kMuted, "tick %llu | queue %d | sent %llu | dropped %llu",
                           static_cast<unsigned long long>(tick_gate::TicksObserved()),
                           PacketQueue::Get().PendingCount(),
                           static_cast<unsigned long long>(PacketQueue::Get().SentTotal()),
                           static_cast<unsigned long long>(PacketQueue::Get().DroppedTotal()));

        const float rightEdge = ImGui::GetContentRegionAvail().x;
        ImGui::SameLine(std::max(0.0f, rightEdge - 470.0f));
        ImGui::SetNextItemWidth(200.0f);
        ImGui::InputTextWithHint("##search", "Search modules", g_search, sizeof(g_search));
        ImGui::SameLine(0, 16.0f);
        ImGui::TextColored(kFaint, "INSERT menu | END unload | R-click = settings");
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

void RenderHud() {
    ModuleManager& mgr = ModuleManager::Get();
    Module* hud = mgr.Find("hud");
    if (!hud || !hud->IsHudVisible()) return;
    const PlayerState s = mgr.LatestState();
    const ImGuiIO& io = ImGui::GetIO();
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_NoNav |
                                   ImGuiWindowFlags_NoScrollbar;
    ImGui::SetNextWindowPos(ImVec2(12.0f, 52.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.55f);
    if (ImGui::Begin("##hud", nullptr, flags)) {
        ImGui::TextColored(kText, "UtilityClient");
        ImGui::TextColored(kMuted, "%.0f fps", io.Framerate);
        if (s.inWorld) {
            ImGui::TextColored(kMuted, "XYZ %.1f / %.1f / %.1f", s.x, s.y, s.z);
            ImGui::TextColored(kMuted, "HP %.0f  Food %d", s.health, s.foodLevel);
        }
    }
    ImGui::End();

    // Active modules, top-right.
    std::vector<const char*> active;
    for (auto& m : mgr.All()) if (m->DesiredEnabled() && m->Id() != "hud") active.push_back(m->Name().c_str());
    if (!active.empty()) {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 12.0f, 52.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        if (ImGui::Begin("##active", nullptr, flags)) {
            for (const char* n : active) {
                const float w = ImGui::CalcTextSize(n).x;
                ImGui::SetCursorPosX(ImGui::GetWindowWidth() - w - 8.0f);
                ImGui::TextColored(kAccent, "%s", n);
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
}
}  // namespace

void LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    const char* regular = "C:\\Windows\\Fonts\\segoeui.ttf";
    const char* bold = "C:\\Windows\\Fonts\\segoeuib.ttf";
    if (GetFileAttributesA(regular) != INVALID_FILE_ATTRIBUTES) {
        io.Fonts->AddFontFromFileTTF(regular, 16.0f);
        if (GetFileAttributesA(bold) != INVALID_FILE_ATTRIBUTES) {
            g_fontHeader = io.Fonts->AddFontFromFileTTF(bold, 13.0f);
        }
    } else {
        io.Fonts->AddFontDefault();
    }
}

void ApplyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowPadding = ImVec2(12, 10);
    s.FramePadding = ImVec2(8, 4);
    s.ItemSpacing = ImVec2(8, 6);
    s.ItemInnerSpacing = ImVec2(6, 4);
    s.WindowRounding = kRadius;
    s.ChildRounding = kRadius;
    s.FrameRounding = kRadius;
    s.PopupRounding = kRadius;
    s.ScrollbarRounding = kRadius;
    s.GrabRounding = kRadius;
    s.WindowBorderSize = 1.0f;
    s.FrameBorderSize = 1.0f;
    s.ScrollbarSize = 10.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = kText;
    c[ImGuiCol_TextDisabled] = kFaint;
    c[ImGuiCol_WindowBg] = kPanel;
    c[ImGuiCol_ChildBg] = kPanel;
    c[ImGuiCol_PopupBg] = kPanel;
    c[ImGuiCol_Border] = kBorder;
    c[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.12f, 0.14f, 1.0f);
    c[ImGuiCol_FrameBgHovered] = kPanelHover;
    c[ImGuiCol_FrameBgActive] = ImVec4(0.17f, 0.19f, 0.22f, 1.0f);
    c[ImGuiCol_TitleBg] = kPanel;
    c[ImGuiCol_TitleBgActive] = kPanel;
    c[ImGuiCol_CheckMark] = kAccent;
    c[ImGuiCol_SliderGrab] = kAccent;
    c[ImGuiCol_SliderGrabActive] = WithAlpha(kAccent, 0.85f);
    c[ImGuiCol_Button] = ImVec4(0.11f, 0.12f, 0.14f, 1.0f);
    c[ImGuiCol_ButtonHovered] = kPanelHover;
    c[ImGuiCol_ButtonActive] = ImVec4(0.17f, 0.19f, 0.22f, 1.0f);
    c[ImGuiCol_Header] = kAccentDim;
    c[ImGuiCol_HeaderHovered] = kPanelHover;
    c[ImGuiCol_HeaderActive] = kAccentDim;
    c[ImGuiCol_Separator] = kBorder;
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = kBorder;
    c[ImGuiCol_ScrollbarGrabHovered] = kFaint;
    c[ImGuiCol_ScrollbarGrabActive] = kMuted;
    c[ImGuiCol_NavHighlight] = kAccent;
    c[ImGuiCol_TextSelectedBg] = kAccentDim;
}

void RenderFrame() {
    RenderHud();
    if (!app::menuOpen.load()) return;
    RenderStatusStrip();
    float x = 16.0f;
    for (int i = 0; i < static_cast<int>(Category::Count); ++i) {
        const auto cat = static_cast<Category>(i);
        bool any = false;
        for (auto& m : ModuleManager::Get().All()) any |= (m->GetCategory() == cat);
        if (!any) continue;  // never render an empty dropdown column
        RenderCategoryPanel(cat, x);
        x += kPanelWidth + 12.0f;
    }
}

}  // namespace uc::menu
