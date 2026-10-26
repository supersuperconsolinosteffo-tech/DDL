// log.hpp - tiny thread-safe file logger for the injected DLL.
// Logs to %USERPROFILE%\.utilityclient\utilityclient.log and OutputDebugString.
#pragma once

#include <string>

namespace uc::log {

// Creates ~/.utilityclient and opens the log file. Safe to call once from bootstrap.
void Init();
void Shutdown();

void Write(const char* level, const std::string& message);

inline void Info(const std::string& m)  { Write("INFO ", m); }
inline void Warn(const std::string& m)  { Write("WARN ", m); }
inline void Error(const std::string& m) { Write("ERROR", m); }

// Directory used for config + log (%USERPROFILE%\.utilityclient). Created by Init().
std::wstring ConfigDir();

}  // namespace uc::log
