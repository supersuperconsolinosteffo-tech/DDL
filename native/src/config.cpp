#include "config.hpp"

#include "log.hpp"

#include <windows.h>

#include <fstream>
#include <string>

namespace uc::config {
namespace {
Settings g_settings;

std::wstring IniPath() { return log::ConfigDir() + L"\\utilityclient.ini"; }

bool ParseBool(const std::string& v, bool dflt) {
    if (v == "1" || v == "true" || v == "yes") return true;
    if (v == "0" || v == "false" || v == "no") return false;
    return dflt;
}

std::string Trim(std::string s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    const auto e = s.find_last_not_of(" \t\r\n");
    return (b == std::string::npos) ? "" : s.substr(b, e - b + 1);
}
}  // namespace

Settings& Get() { return g_settings; }

void Load() {
    const std::filesystem::path path = IniPath();
    std::ifstream in(path);
    if (!in) {
        std::ofstream out(path);
        out << "; UtilityClient (Minecraft 1.21.11) - singleplayer / test-server utility DLL\n"
               "; menu_key / unload_key are Win32 virtual-key codes (decimal or 0x..).\n"
               "; INSERT=45 (0x2D), END=35 (0x23), RSHIFT=161 (0xA1), F8=119 (0x77)\n"
               "menu_key=45\n"
               "unload_key=35\n"
               "debug_console=false\n"
               "; Experimental: use a JVMTI breakpoint on MinecraftClient.tick() as the\n"
               "; tick-start gate instead of the Fabric bridge mod. Leave false unless\n"
               "; you know why (bridge mod is the supported path).\n"
               "jvmti_tick_gate=false\n";
        log::Info("Wrote default config");
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = Trim(line.substr(0, eq));
        const std::string val = Trim(line.substr(eq + 1));
        try {
            if (key == "menu_key")        g_settings.menuKey = std::stoi(val, nullptr, 0);
            else if (key == "unload_key") g_settings.unloadKey = std::stoi(val, nullptr, 0);
            else if (key == "debug_console")  g_settings.debugConsole = ParseBool(val, false);
            else if (key == "jvmti_tick_gate") g_settings.jvmtiTickGate = ParseBool(val, false);
        } catch (...) {
            log::Warn("Ignoring bad config line: " + line);
        }
    }
    log::Info("Config loaded (menu_key=" + std::to_string(g_settings.menuKey) +
              ", jvmti_tick_gate=" + (g_settings.jvmtiTickGate ? "true" : "false") + ")");
}

}  // namespace uc::config
