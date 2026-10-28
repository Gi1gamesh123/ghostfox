#pragma once
#include <cstdint>
#include <string>

// The "fonts" profile group: process-wide, set once at configure time. With
// nothing configured every query below is a no-op and font resolution is the
// system collection exactly as before.
namespace ruyijs::web_geometry {
// False when `family` (any case) is not installed as far as the page can tell:
// absent from a configured allow-list, or present in the hidden list.
bool FontFamilyVisible(const std::wstring& family);
// The configured family for a generic ("serif", "sans-serif", "monospace",
// "cursive", "fantasy"); empty when not overridden.
std::string GenericFamilyOverride(const std::string& generic);
// Bumped on every configure so cached collections drop their memos.
uint64_t FontConfigGeneration();
}
