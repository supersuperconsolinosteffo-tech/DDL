// config.hpp - DLL-level settings loaded from ~/.utilityclient/utilityclient.ini
#pragma once

namespace uc::config {

struct Settings {
    int  menuKey       = 0x2D;  // VK_INSERT
    int  unloadKey     = 0x23;  // VK_END
    bool debugConsole  = false; // AllocConsole for stdout (dev only)
    bool jvmtiTickGate = false; // experimental: JVMTI breakpoint gate instead of bridge mod
};

Settings& Get();

// Loads the ini if present (missing file = defaults). Also (re)writes defaults
// on first run so the file exists for editing.
void Load();

}  // namespace uc::config
