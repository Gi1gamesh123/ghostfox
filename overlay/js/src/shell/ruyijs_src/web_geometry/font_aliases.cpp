/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// Family names DirectWrite's system collection does not list by itself, as
// gfxDWriteFontList resolves them: registry FontSubstitutes and the bitmap-font
// replacements (GetFontSubstitutes / GetDirectWriteSubstitutes), then each
// face's legacy Win32 family name (ReadFaceNames other-family aliases).
#include "web_geometry/font_backend.h"
#include <cwctype>
#pragma comment(lib, "advapi32.lib")

namespace ruyijs::web_geometry {
namespace {
std::wstring Key(std::wstring name) {
  if (const auto comma = name.find(L','); comma != name.npos) name.resize(comma);
  for (auto& ch : name) ch = wchar_t(std::towlower(ch));
  return name;
}
bool Exists(IDWriteFontCollection* fonts, const std::wstring& name) {
  UINT32 index = 0; BOOL exists = FALSE;
  return SUCCEEDED(fonts->FindFamilyName(name.c_str(), &index, &exists)) && exists;
}
void Substitute(FontAliases& aliases, IDWriteFontCollection* fonts,
                const std::wstring& alias, const std::wstring& actual) {
  const auto key = Key(alias), target = Key(actual);
  if (key.empty() || alias[0] == L'@' || Exists(fonts, key)) return;
  if (Exists(fonts, target)) aliases.substitutes[key] = target;
  else if (auto known = aliases.substitutes.find(target); known != aliases.substitutes.end())
    aliases.substitutes[key] = known->second;
}
void ReadRegistry(FontAliases& aliases, IDWriteFontCollection* fonts) {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\FontSubstitutes",
                    0, KEY_READ, &key) != ERROR_SUCCESS) return;
  for (DWORD i = 0; i < 4096; ++i) {
    wchar_t name[512]{}, value[512]{};
    DWORD nameLength = DWORD(std::size(name)), valueLength = sizeof(value) - sizeof(wchar_t), type = 0;
    const LONG rv = RegEnumValueW(key, i, name, &nameLength, nullptr, &type,
                                  reinterpret_cast<BYTE*>(value), &valueLength);
    if (rv == ERROR_NO_MORE_ITEMS) break;
    if (rv == ERROR_SUCCESS && type == REG_SZ && nameLength) Substitute(aliases, fonts, name, value);
  }
  RegCloseKey(key);
}
void ReadLegacyNames(FontAliases& aliases, IDWriteFontCollection* fonts) {
  const UINT32 count = fonts->GetFontFamilyCount();
  for (UINT32 f = 0; f < count; ++f) {
    ComPtr<IDWriteFontFamily> family;
    if (FAILED(fonts->GetFontFamily(f, &family))) continue;
    for (UINT32 i = 0, faces = family->GetFontCount(); i < faces; ++i) {
      ComPtr<IDWriteFont> font; ComPtr<IDWriteLocalizedStrings> names; BOOL exists = FALSE;
      if (FAILED(family->GetFont(i, &font)) || font->GetSimulations() != DWRITE_FONT_SIMULATIONS_NONE ||
          FAILED(font->GetInformationalStrings(DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES,
                                               &names, &exists)) || !exists) continue;
      // Named instances of a variable font have no legacy name in its tables.
      ComPtr<IDWriteFontFace> face;
      const void* table = nullptr; UINT32 size = 0; void* context = nullptr; BOOL variable = FALSE;
      if (FAILED(font->CreateFontFace(&face))) continue;
      if (SUCCEEDED(face->TryGetFontTable(DWRITE_MAKE_OPENTYPE_TAG('f', 'v', 'a', 'r'), &table, &size,
                                          &context, &variable)) && variable) {
        face->ReleaseFontTable(context);
        continue;
      }
      for (UINT32 n = 0, total = names->GetCount(); n < total; ++n) {
        UINT32 length = 0;
        if (FAILED(names->GetStringLength(n, &length)) || length >= 512) continue;
        std::wstring name(length + 1, L'\0');
        if (FAILED(names->GetString(n, name.data(), length + 1))) continue;
        name.resize(length);
        const auto key = Key(name);
        if (!Exists(fonts, key)) aliases.faces[key].push_back(font);
      }
    }
  }
}
}
const FontAliases& FontCollection::Aliases() {
  if (aliases) return *aliases;
  aliases = std::make_unique<FontAliases>();
  static const wchar_t* kDirectWrite[][2] = {
    {L"MS Sans Serif", L"Microsoft Sans Serif"}, {L"MS Serif", L"Times New Roman"},
    {L"Courier", L"Courier New"}, {L"Small Fonts", L"Arial"},
    {L"Roman", L"Times New Roman"}, {L"Script", L"Mistral"}};
  // The bitmap replacements resolve first so registry aliases of them chain.
  for (const auto& sub : kDirectWrite) Substitute(*aliases, fonts.Get(), sub[0], sub[1]);
  ReadRegistry(*aliases, fonts.Get());
  ReadLegacyNames(*aliases, fonts.Get());
  return *aliases;
}
// The alias face nearest the requested weight (CSS font matching order).
ComPtr<IDWriteFont> AliasFace(const std::vector<ComPtr<IDWriteFont>>& faces, unsigned weight, bool italic) {
  ComPtr<IDWriteFont> best;
  int bestScore = INT_MAX;
  for (const auto& face : faces) {
    const int w = int(face->GetWeight());
    int distance = w == int(weight) ? 0 : weight <= 500 && w < int(weight) ? int(weight) - w
        : weight <= 500 ? 1000 + w - int(weight) : w > int(weight) ? w - int(weight) : 1000 + int(weight) - w;
    if ((face->GetStyle() != DWRITE_FONT_STYLE_NORMAL) != italic) distance += 10000;
    if (distance < bestScore) { bestScore = distance; best = face; }
  }
  return best;
}
}
