#pragma once

#if defined(BOT_DEBUG)
#define BOT_DEBUG 1
#else
#define BOT_DEBUG 0
#endif

#if __EMSCRIPTEN__
#include <emscripten.h>
#define EXPORT extern "C" EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT extern "C"
#endif
