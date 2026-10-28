// Configure-only "mediaFeatures" profile group: the CSS media-query answers a
// profile overrides (matchMedia / @media / <link media>). Owned by Web Styling.
#pragma once
#include <map>
#include <string>
namespace ruyijs::web_styling {
// Process-scoped store, replaced whole by each ConfigureProfileGroup call.
std::map<std::string, std::string> MediaFeatureProfile();
void SetMediaFeatureProfile(std::map<std::string, std::string> profile);
}  // namespace ruyijs::web_styling
