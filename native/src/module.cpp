#include "module.hpp"

#include <fstream>
#include <sstream>

#include "log.hpp"

namespace uc {

bool Module::GetBool(const std::string& key, bool dflt) {
    std::lock_guard lock(settingsMutex_);
    for (const Setting& s : settings_) if (s.key == key && s.type == Setting::Type::Bool) return s.boolValue;
    return dflt;
}
float Module::GetFloat(const std::string& key, float dflt) {
    std::lock_guard lock(settingsMutex_);
    for (const Setting& s : settings_) if (s.key == key && s.type == Setting::Type::Float) return s.floatValue;
    return dflt;
}
int Module::GetInt(const std::string& key, int dflt) {
    std::lock_guard lock(settingsMutex_);
    for (const Setting& s : settings_) if (s.key == key && s.type == Setting::Type::Int) return s.intValue;
    return dflt;
}

ModuleManager& ModuleManager::Get() {
    static ModuleManager m;
    return m;
}

Module* ModuleManager::Find(const std::string& id) {
    for (auto& m : modules_) if (m->Id() == id) return m.get();
    return nullptr;
}

void ModuleManager::OnTickGate(TickContext& ctx) {
    for (auto& m : modules_) {
        const bool want = m->DesiredEnabled();
        if (want && !m->appliedEnabled_) {
            m->OnEnable(ctx);
            m->appliedEnabled_ = true;
        } else if (!want && m->appliedEnabled_) {
            m->OnDisable(ctx);
            m->appliedEnabled_ = false;
        }
        if (m->appliedEnabled_) m->OnTickStart(ctx);
    }
}

void ModuleManager::OnNoPlayer() {
    // Player object is gone (disconnect/respawn into new instance): forget
    // applied state WITHOUT calling OnDisable against stale objects.
    // Modules re-apply OnEnable on the next in-world tick.
    for (auto& m : modules_) m->appliedEnabled_ = false;
}

PlayerState ModuleManager::LatestState() {
    std::lock_guard lock(stateMutex_);
    return latestState_;
}
void ModuleManager::PublishState(const PlayerState& s) {
    std::lock_guard lock(stateMutex_);
    latestState_ = s;
}

namespace {
std::wstring ModulesIni() { return log::ConfigDir() + L"\\modules.ini"; }
std::string Trim(std::string s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    const auto e = s.find_last_not_of(" \t\r\n");
    return (b == std::string::npos) ? "" : s.substr(b, e - b + 1);
}
}  // namespace

void ModuleManager::SaveState() {
    std::ofstream out{std::filesystem::path{ModulesIni()}};
    if (!out) return;
    out << "; UtilityClient module state (auto-saved; safe to edit while game is closed)\n";
    for (auto& m : modules_) {
        out << "[module." << m->Id() << "]\n";
        out << "enabled=" << (m->DesiredEnabled() ? "1" : "0") << "\n";
        std::lock_guard lock(m->SettingsMutex());
        for (const Setting& s : m->Settings()) {
            out << "setting." << s.key << "=";
            switch (s.type) {
                case Setting::Type::Bool:  out << (s.boolValue ? "1" : "0"); break;
                case Setting::Type::Float: out << s.floatValue; break;
                case Setting::Type::Int:   out << s.intValue; break;
            }
            out << "\n";
        }
    }
}

void ModuleManager::LoadState() {
    std::ifstream in{std::filesystem::path{ModulesIni()}};
    if (!in) return;
    Module* current = nullptr;
    std::string line;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.rfind("[module.", 0) == 0 && line.back() == ']') {
            current = Find(line.substr(8, line.size() - 9));
            continue;
        }
        if (!current) continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = Trim(line.substr(0, eq));
        const std::string val = Trim(line.substr(eq + 1));
        try {
            if (key == "enabled") {
                current->SetDesiredEnabled(val == "1" || val == "true");
            } else if (key.rfind("setting.", 0) == 0) {
                const std::string skey = key.substr(8);
                std::lock_guard lock(current->SettingsMutex());
                for (Setting& s : current->Settings()) {
                    if (s.key != skey) continue;
                    switch (s.type) {
                        case Setting::Type::Bool:  s.boolValue = (val == "1" || val == "true"); break;
                        case Setting::Type::Float: s.floatValue = std::stof(val); break;
                        case Setting::Type::Int:   s.intValue = std::stoi(val); break;
                    }
                }
            }
        } catch (...) {
            log::Warn("Ignoring bad modules.ini line: " + line);
        }
    }
    log::Info("Module state loaded");
}

}  // namespace uc
