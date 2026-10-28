#ifndef RUYIJS_BROWSER_ENVIRONMENT_PLUGINS_H
#define RUYIJS_BROWSER_ENVIRONMENT_PLUGINS_H

namespace ruyijs::browser_environment {
struct State;

// The HTML "PDF viewing support" surface: the PluginArray/Plugin/MimeTypeArray/
// MimeType interfaces, their four global constructors and the two Navigator
// accessors (`plugins` and `mimeTypes`) that expose them. This is the single
// production installer for all ten definitions; the data is the donor''s
// hard-coded PDF set (dom/base/nsPluginArray.cpp, dom/base/nsMimeTypeArray.h).
bool InstallPluginSurface(State& state);
}  // namespace ruyijs::browser_environment
#endif
