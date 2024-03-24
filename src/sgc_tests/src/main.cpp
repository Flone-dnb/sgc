// Custom.
#include "GcInfoCallbacks.hpp"

// External.
#include "catch2/catch_session.hpp"

#if defined(WIN32)
#include <Windows.h>
#include <crtdbg.h>
#endif

int main() {
    // Enable run-time memory check for debug builds (on Windows).
#if defined(WIN32) && defined(DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#elif defined(WIN32) && !defined(DEBUG)
    OutputDebugStringA("Using release build configuration, memory checks are disabled.");
#endif

    // Set warn/error callbacks.
    sgc::GcInfoCallbacks::setCallbacks(
        [](const char* pMessage) { throw std::runtime_error(pMessage); },
        [](const char* pMessage) { throw std::runtime_error(pMessage); });

    // Run tests.
    return Catch::Session().run();
}
