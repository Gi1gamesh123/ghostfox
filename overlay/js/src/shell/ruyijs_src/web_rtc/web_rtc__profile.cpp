// The "rtc" profile group (docs/FINGERPRINT_CONFIG_INVENTORY.md section J).
// Codec entries are "mimeType|clockRate|channels|sdpFmtpLine[|receive]" (the
// last token marks a receive-only codec); channels and fmtp may be empty.
#include "web_rtc/profile.h"
#include "web_rtc/realm.h"
#include "runtime/runtime.h"
#include "runtime/web_kit.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/CharacterEncoding.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include <cstdlib>

namespace ruyijs::web_rtc {
namespace {
Profile& Stored() { static Profile profile; return profile; }

bool Fail(JSContext* cx, const std::string& name, const char* what) {
  return WebKit::Fail(cx, ("rtc." + name + " " + what).c_str());
}

// Reads `name` as an array of strings; *present stays false when absent.
bool Strings(JSContext* cx, JS::HandleObject group, const char* name, bool* present,
             std::vector<std::string>* out) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, group, name, &value)) return false;
  if (value.isUndefined()) return true;
  bool array = false;
  if (!value.isObject()) return Fail(cx, name, "must be an array of strings");
  JS::RootedObject list(cx, &value.toObject());
  uint32_t length = 0;
  if (!JS::IsArray(cx, list, &array) || !array || !JS::GetArrayLength(cx, list, &length))
    return array ? false : Fail(cx, name, "must be an array of strings");
  JS::RootedValue item(cx);
  for (uint32_t i = 0; i < length; ++i) {
    if (!JS_GetElement(cx, list, i, &item)) return false;
    if (!item.isString()) return Fail(cx, name, "must be an array of strings");
    JS::RootedString text(cx, item.toString());
    std::string utf8;
    if (!ToUTF8(cx, text, &utf8)) return false;
    out->push_back(utf8);
  }
  *present = true;
  return true;
}

bool Text(JSContext* cx, JS::HandleObject group, const char* name, std::optional<std::string>* out) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, group, name, &value)) return false;
  if (value.isUndefined() || value.isNull()) return true;
  if (!value.isString()) return Fail(cx, name, "must be a string or null");
  JS::RootedString text(cx, value.toString());
  std::string utf8;
  if (!ToUTF8(cx, text, &utf8)) return false;
  *out = utf8;
  return true;
}

bool Number(const std::string& text, unsigned long limit, unsigned long* out) {
  if (text.empty()) { *out = 0; return true; }
  char* end = nullptr;
  *out = std::strtoul(text.c_str(), &end, 10);
  return *end == '\0' && *out <= limit && text[0] != '-';
}

bool ParseCodec(const std::string& line, ProfileCodec* codec) {
  std::vector<std::string> parts;
  size_t start = 0;
  for (size_t at; (at = line.find('|', start)) != std::string::npos; start = at + 1)
    parts.push_back(line.substr(start, at - start));
  parts.push_back(line.substr(start));
  if (parts.size() < 4 || parts.size() > 5 || parts[0].find('/') == std::string::npos) return false;
  unsigned long clock = 0, channels = 0;
  if (!Number(parts[1], 0xffffffffUL, &clock) || !clock || !Number(parts[2], 65535, &channels))
    return false;
  if (parts.size() == 5 && parts[4] != "receive") return false;
  codec->mime = parts[0];
  codec->clock = static_cast<uint32_t>(clock);
  codec->channels = static_cast<uint16_t>(channels);
  codec->fmtp = parts[3];
  codec->receiveOnly = parts.size() == 5;
  return true;
}

bool Codecs(JSContext* cx, JS::HandleObject group, const char* name,
            std::optional<std::vector<ProfileCodec>>* out) {
  bool present = false;
  std::vector<std::string> lines;
  if (!Strings(cx, group, name, &present, &lines)) return false;
  if (!present) return true;
  std::vector<ProfileCodec> codecs(lines.size());
  for (size_t i = 0; i < lines.size(); ++i)
    if (!ParseCodec(lines[i], &codecs[i]))
      return Fail(cx, name, "entries must be mimeType|clockRate|channels|sdpFmtpLine[|receive]");
  *out = std::move(codecs);
  return true;
}
}  // namespace

const Profile& CurrentProfile() { return Stored(); }
void SetProfile(Profile profile) { Stored() = std::move(profile); }

bool ConfigureProfileGroup(void* context, JS::HandleObject group) {
  JSContext* cx = static_cast<JSContext*>(context);
  Profile profile;
  bool present = false;
  if (!Codecs(cx, group, "audioCodecs", &profile.audioCodecs) ||
      !Codecs(cx, group, "videoCodecs", &profile.videoCodecs) ||
      !Text(cx, group, "sessionId", &profile.sessionId) ||
      !Text(cx, group, "iceUfrag", &profile.iceUfrag) ||
      !Text(cx, group, "icePwd", &profile.icePwd) ||
      !Text(cx, group, "dtlsFingerprint", &profile.dtlsFingerprint) ||
      !Strings(cx, group, "iceCandidates", &present, &profile.iceCandidates)) return false;
  for (const auto& line : profile.iceCandidates)
    if (line.rfind("candidate:", 0) != 0) return Fail(cx, "iceCandidates", "entries must start with candidate:");
  SetProfile(std::move(profile));
  return true;
}
}  // namespace ruyijs::web_rtc
