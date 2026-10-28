/* ruyi-clone stub: Firefox font preference table. */
#ifndef RUYIJS_FIREFOX_FONT_PREFS_H
#define RUYIJS_FIREFOX_FONT_PREFS_H
namespace ruyijs {
struct FontPreference {
  const char* key;
  const char* value;
};
inline const FontPreference kFontPreferences[] = {
    {"font.default.serif", "Times New Roman"},
    {"font.default.sans-serif", "Arial"},
    {"font.default.monospace", "Courier New"},
    {"font.name.serif.en-US", "Times New Roman"},
    {"font.name.sans-serif.en-US", "Arial"},
    {"font.name.monospace.en-US", "Courier New"},
};
}  // namespace ruyijs
#endif
