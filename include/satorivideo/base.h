#pragma once

// Flag shows if bot was compiled in debug mode
#if defined(BOT_DEBUG)
#define BOT_DEBUG true
#else
#define BOT_DEBUG false
#endif

#if __EMSCRIPTEN__
#include <emscripten.h>
#define EXPORT extern "C" EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT extern "C"
#endif

#if defined(__APPLE__)
#define PLATFORM_APPLE true
#else
#define PLATFORM_APPLE false
#endif

#if NDEBUG
#define RELEASE_MODE true
#define DEBUG_MODE false
#else
#define RELEASE_MODE false
#define DEBUG_MODE true
#endif
