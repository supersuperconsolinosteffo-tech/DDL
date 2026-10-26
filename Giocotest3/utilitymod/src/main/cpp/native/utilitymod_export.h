// utilitymod_export.h
// Export macro for the public C API of utilitymod_native.
//
// The library is always built as a shared library, so every symbol tagged with
// UTILITYMOD_API is exported from the resulting DLL. This is required because
// the target is built with CXX_VISIBILITY_PRESET hidden, and because MSVC does
// not export symbols by default.

#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
    #define UTILITYMOD_API __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
    #define UTILITYMOD_API __attribute__((visibility("default")))
#else
    #define UTILITYMOD_API
#endif
