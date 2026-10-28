#include <AppVersion.h>
#if defined(CROSSINK_PLAY)
#include <PlayVersion.h>
#endif

// Only this translation unit receives the changing version and Git defines.
#ifndef CROSSINK_VERSION
#define CROSSINK_VERSION "dev"
#endif

#ifndef CROSSINK_GIT_SHA
#define CROSSINK_GIT_SHA "unknown"
#endif

#ifndef CROSSINK_GIT_DIRTY
#define CROSSINK_GIT_DIRTY "unknown"
#endif

namespace AppVersion {
#if defined(CROSSINK_PLAY)
const char* version() { return CROSSINK_VERSION "+play." CROSSINK_PLAY_VERSION; }
const char* versionLabel() { return "CrossInk Play " CROSSINK_PLAY_VERSION; }
const char* userAgent() { return "CrossInk-Play-" CROSSINK_PLAY_VERSION; }
#else
const char* version() { return CROSSINK_VERSION; }
const char* versionLabel() { return "CrossInk " CROSSINK_VERSION; }
const char* userAgent() { return "CrossInk-ESP32-" CROSSINK_VERSION; }
#endif
const char* gitSha() { return CROSSINK_GIT_SHA; }
const char* gitDirtyFlag() { return CROSSINK_GIT_DIRTY; }
}  // namespace AppVersion
