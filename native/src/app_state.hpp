// app_state.hpp - cross-thread flags shared by bootstrap / hooks / menu.
#pragma once

#include <windows.h>

#include <atomic>

namespace uc::app {

inline std::atomic<bool> menuOpen{true};          // INSERT toggles (starts open so injection is visible)
inline std::atomic<bool> unloadRequested{false};  // END key
inline std::atomic<bool> renderShutdownDone{false};
inline HMODULE dllModule = nullptr;

}  // namespace uc::app
