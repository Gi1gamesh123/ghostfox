#ifndef RUYIJS_BROWSER_ENVIRONMENT_MODEL_H
#define RUYIJS_BROWSER_ENVIRONMENT_MODEL_H
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>
#include "browser_environment/source_snapshot.h"

namespace ruyijs::browser_environment {
enum class Kind : int32_t { Navigator, Screen, Location, History, Locale, WebGL, WebGpu, Viewport,
                            Permissions, Geolocation, Speech, Storage, Media, Canvas,
                            Audio, MediaFeatures, Rtc, Fonts };
// Number is a finite double; a field marked nullable also accepts null.
// Object is a fixed member list and Map a caller-named table; both carry their
// shape in `members`/`count`/`value` below, so one group field can describe a
// nested profile value without a second schema layer.
enum class ValueType { String, Integer, Boolean, Strings, Number, Object, Map };
struct Field {
  const char* name;
  ValueType type;
  bool worker = false;
  bool nullable = false;
  // A "|"-separated allow-list for a String field, validated here so an owner
  // never has to reject a value the schema already accepted.
  const char* allowed = nullptr;
  // Object: `members`/`count` are the declared members, each a scalar.
  // Map: `value` is the element type when `members` is null; otherwise every
  // element is a complete record of `members`/`count`.
  ValueType value = ValueType::Number;
  const Field* members = nullptr;
  size_t count = 0;
};
inline constexpr Field kNavigatorFields[] = {
  {"userAgent", ValueType::String, true}, {"platform", ValueType::String, true},
  {"appCodeName", ValueType::String, true}, {"appName", ValueType::String, true},
  {"appVersion", ValueType::String, true}, {"product", ValueType::String, true},
  {"productSub", ValueType::String}, {"vendor", ValueType::String},
  {"vendorSub", ValueType::String}, {"language", ValueType::String, true},
  {"languages", ValueType::Strings, true}, {"hardwareConcurrency", ValueType::Integer, true},
  {"maxTouchPoints", ValueType::Integer}, {"cookieEnabled", ValueType::Boolean},
  {"webdriver", ValueType::Boolean},
  // Firefox-only platform traits the imported ruyijs owner does not model; the
  // values live in Model::Impl (model_internal.h) and are configured through
  // the same validated profile as the imported fields.
  {"oscpu", ValueType::String}, {"buildID", ValueType::String},
  {"doNotTrack", ValueType::String}, {"pdfViewerEnabled", ValueType::Boolean},
  {"onLine", ValueType::Boolean, true},
  {"globalPrivacyControl", ValueType::Boolean, true}
};
// Index of maxTouchPoints in kNavigatorFields. State::TouchCapable reads it via
// Model::Read; keep it in sync with the array order above (the five Firefox
// traits above are appended after it, so this index never moves).
inline constexpr uint32_t kNavigatorMaxTouchPoints = 12;
inline constexpr Field kScreenFields[] = {
  {"width", ValueType::Integer}, {"height", ValueType::Integer},
  {"availWidth", ValueType::Integer}, {"availHeight", ValueType::Integer},
  {"availLeft", ValueType::Integer}, {"availTop", ValueType::Integer},
  {"colorDepth", ValueType::Integer}, {"pixelDepth", ValueType::Integer},
  // CSSOM-View screen origin relative to the primary screen; kept in Model::Impl
  // because the imported ScreenInfo has no such fields (v8_parity T5b second
  // slice). Appended so indices 0..7 keep their existing meaning.
  {"top", ValueType::Integer}, {"left", ValueType::Integer}
};
inline constexpr Field kLocationFields[] = {
  {"href", ValueType::String}, {"protocol", ValueType::String},
  {"host", ValueType::String}, {"hostname", ValueType::String},
  {"port", ValueType::String}, {"pathname", ValueType::String},
  {"search", ValueType::String}, {"hash", ValueType::String},
  {"origin", ValueType::String}
};
inline constexpr Field kLocaleFields[] = {{"timezone", ValueType::String}};
// The content viewport is separate from Screen: Firefox's browser content
// area is commonly shorter than screen.availHeight and iframe contexts have
// their own content boxes. It is optional in profiles for backward parity.
// devicePixelRatio is the one window scalar that is neither screen nor frame
// geometry: Firefox derives window.devicePixelRatio and the CSS `resolution` /
// `-webkit-device-pixel-ratio` media features from it, so it lives here and the
// styling owner reads it through Describe() instead of keeping a second value.
inline constexpr Field kViewportFields[] = {{"width", ValueType::Integer},
                                             {"height", ValueType::Integer},
                                             {"devicePixelRatio", ValueType::Number}};
// Index of devicePixelRatio in kViewportFields; the Window facade and the
// styling owner's media-feature source both read it through this index.
inline constexpr uint32_t kViewportDevicePixelRatio = 2;
// The "webgl" group configures the six GL strings a fingerprint probe reads
// (the donor's webgl.vendor / webgl.renderer / webgl.unmasked_vendor /
// webgl.unmasked_renderer keys plus the two version strings); the numeric
// limits and the extension list are read-only source data. These names match
// the WebGL rendering context's environment keys.
// The WebGLContextAttributes dictionary getContextAttributes() reports, in
// WebIDL member order. powerPreference is the one non-boolean member.
inline constexpr Field kWebGLAttributeFields[] = {
  {"alpha", ValueType::Boolean}, {"antialias", ValueType::Boolean},
  {"depth", ValueType::Boolean}, {"failIfMajorPerformanceCaveat", ValueType::Boolean},
  {"powerPreference", ValueType::String, false, false,
   "default|low-power|high-performance"},
  {"premultipliedAlpha", ValueType::Boolean},
  {"preserveDrawingBuffer", ValueType::Boolean}, {"stencil", ValueType::Boolean}};
inline constexpr Field kWebGLFields[] = {
  {"vendor", ValueType::String}, {"renderer", ValueType::String},
  {"version", ValueType::String}, {"glslVersion", ValueType::String},
  {"unmaskedVendor", ValueType::String}, {"unmaskedRenderer", ValueType::String},
  {"version2", ValueType::String}, {"glslVersion2", ValueType::String},
  {"attributes", ValueType::Object, false, false, nullptr, ValueType::Number,
   kWebGLAttributeFields, std::size(kWebGLAttributeFields)},
  // The extension lists getSupportedExtensions()/getExtension() answer, one
  // entry per name, in the order the page sees them.
  {"extensions", ValueType::Strings}, {"extensions2", ValueType::Strings},
  // Per-pname getParameter overrides, keyed by the decimal pname ("3379").
  {"limits", ValueType::Map, false, false, nullptr, ValueType::Number}};
// The "webgpu" group only carries the stub-adapter opt-in. The adapter and
// device feature sets, and every limit, stay read-only data from the imported
// WebGpuCapabilitiesAggregate; the baseline has no such group.
inline constexpr Field kWebGpuFields[] = {
  {"enabled", ValueType::Boolean}, {"vendor", ValueType::String},
  {"architecture", ValueType::String}, {"device", ValueType::String},
  {"description", ValueType::String}, {"features", ValueType::Strings},
  {"deviceFeatures", ValueType::Strings}, {"limits", ValueType::Map},
  {"fallback", ValueType::Boolean}};
// The "permissions" group: PermissionName -> granted|denied|prompt. The table
// belongs to the Permissions owner (docs/owners/PERMISSIONS.md); the baseline
// has no such group, so the owner's Firefox defaults apply.
inline constexpr Field kPermissionsFields[] = {
  {"geolocation", ValueType::String}, {"notifications", ValueType::String},
  {"push", ValueType::String}, {"persistent-storage", ValueType::String},
  {"midi", ValueType::String}, {"storage-access", ValueType::String},
  {"screen-wake-lock", ValueType::String}, {"camera", ValueType::String},
  {"microphone", ValueType::String}, {"loopback-network", ValueType::String},
  {"local-network", ValueType::String}};
// The "geolocation" group: the Geolocation owner's fixed position
// (docs/owners/GEOLOCATION.md). Configure-only; the baseline has no such group.
inline constexpr Field kGeolocationFields[] = {
  {"latitude", ValueType::Number}, {"longitude", ValueType::Number},
  {"accuracy", ValueType::Number}, {"altitude", ValueType::Number, false, true},
  {"altitudeAccuracy", ValueType::Number, false, true}, {"heading", ValueType::Number, false, true},
  {"speed", ValueType::Number, false, true}, {"timestamp", ValueType::Number, false, true}};
// The "speech" group: the Speech Synthesis owner's voice list
// (docs/owners/SPEECH_SYNTHESIS.md): names, one language per name, default name.
inline constexpr Field kSpeechFields[] = {
  {"voices", ValueType::Strings}, {"langs", ValueType::Strings}, {"default", ValueType::String}};
// The "media" group: enumerateDevices() results, one "kind|label|deviceId|
// groupId" string per device (Firefox's un-granted answer is one audio input
// with three empty strings), and per-content-type MediaCapabilities decoding
// answers keyed by the page's contentType string.
inline constexpr Field kMediaDecodingFields[] = {
  {"supported", ValueType::Boolean}, {"smooth", ValueType::Boolean},
  {"powerEfficient", ValueType::Boolean}};
inline constexpr Field kMediaFields[] = {
  {"devices", ValueType::Strings},
  {"decoding", ValueType::Map, false, false, nullptr, ValueType::Number,
   kMediaDecodingFields, std::size(kMediaDecodingFields)}};
// The "storage" group: the StorageManager owner's estimate and persistence
// answer (storage_manager.cpp). quota/usage are bytes, persisted is the
// navigator.storage.persisted() result.
inline constexpr Field kStorageFields[] = {
  {"quota", ValueType::Number}, {"usage", ValueType::Number},
  {"persisted", ValueType::Boolean}};
// The "canvas" group: the donor build's --fpfile canvas.* keys
// (dom/canvas/CanvasFingerprint.cpp ParseConfig), delivered to the Canvas owner.
// Ranges are the donor parser's and are checked there (web_canvas/fingerprint_group.cpp).
inline constexpr Field kCanvasFields[] = {
  {"mode", ValueType::String, false, false, "none|pixel"}, {"seed", ValueType::Number},
  {"strength", ValueType::Number}, {"density", ValueType::Number},
  {"preserveAlpha", ValueType::Boolean}, {"preserveWhitePoint", ValueType::Boolean},
  {"pngMetadata", ValueType::Boolean}};
// Configure-only owner groups: listed nowhere in the baseline, each delivered
// whole to its owner's port (owner_groups.cpp), which keeps today's values for
// any field the caller leaves out. The field tables are the owners' contracts.
// "mediaFeatures" (web_styling/media_feature_profile.cpp): matchMedia answers
// for the user-preference and device media features, keyed by camelCase name.
inline constexpr Field kMediaFeatureFields[] = {
  {"prefersColorScheme", ValueType::String, false, false, "light|dark"},
  {"prefersReducedMotion", ValueType::String, false, false, "no-preference|reduce"},
  {"prefersContrast", ValueType::String, false, false, "no-preference|more|less|custom"},
  {"prefersReducedTransparency", ValueType::String, false, false, "no-preference|reduce"},
  {"forcedColors", ValueType::String, false, false, "none|active"},
  {"invertedColors", ValueType::String, false, false, "none|inverted"},
  {"colorGamut", ValueType::String, false, false, "srgb|p3|rec2020"},
  {"videoColorGamut", ValueType::String, false, false, "srgb|p3|rec2020"},
  {"dynamicRange", ValueType::String, false, false, "standard|high"},
  {"videoDynamicRange", ValueType::String, false, false, "standard|high"},
  {"displayMode", ValueType::String, false, false,
   "browser|fullscreen|standalone|minimal-ui|picture-in-picture"},
  {"update", ValueType::String, false, false, "none|slow|fast"},
  {"scripting", ValueType::String, false, false, "none|initial-only|enabled"},
  {"pointer", ValueType::String, false, false, "none|coarse|fine"},
  {"anyPointer", ValueType::String, false, false, "none|coarse|fine"},
  {"hover", ValueType::String, false, false, "none|hover"},
  {"anyHover", ValueType::String, false, false, "none|hover"},
  {"overflowBlock", ValueType::String, false, false, "none|scroll|paged"},
  {"overflowInline", ValueType::String, false, false, "none|scroll"}};
// "rtc" (web_rtc/profile.cpp): getCapabilities codec lists
// ("mimeType|clockRate|channels|sdpFmtpLine[|receive]"), the offer's session
// strings and the icecandidate lines the peer reports.
inline constexpr Field kRtcFields[] = {
  {"audioCodecs", ValueType::Strings}, {"videoCodecs", ValueType::Strings},
  {"sessionId", ValueType::String, false, true}, {"iceUfrag", ValueType::String, false, true},
  {"icePwd", ValueType::String, false, true}, {"dtlsFingerprint", ValueType::String, false, true},
  {"iceCandidates", ValueType::Strings}};
// "fonts" (web_geometry/font_config.cpp): installed-family allow/deny lists and
// the generic-family mapping; a filtered family probes as not installed.
inline constexpr Field kFontFields[] = {
  {"families", ValueType::Strings}, {"hidden", ValueType::Strings},
  {"serif", ValueType::String}, {"sansSerif", ValueType::String},
  {"monospace", ValueType::String}, {"cursive", ValueType::String},
  {"fantasy", ValueType::String}};
// "audio" (web_audio/profile.cpp): live AudioContext sampleRate / latencies and
// the destination's maxChannelCount; ranges are checked by the owner.
inline constexpr Field kAudioFields[] = {
  {"sampleRate", ValueType::Number}, {"baseLatency", ValueType::Number},
  {"outputLatency", ValueType::Number}, {"maxChannelCount", ValueType::Integer}};
struct OwnerGroupSpec {
  Kind kind;
  const char* name;
  const Field* fields;
  size_t count;
};
inline constexpr OwnerGroupSpec kOwnerGroups[] = {
  {Kind::Canvas, "canvas", kCanvasFields, std::size(kCanvasFields)},
  {Kind::Audio, "audio", kAudioFields, std::size(kAudioFields)},
  {Kind::Fonts, "fonts", kFontFields, std::size(kFontFields)},
  {Kind::Rtc, "rtc", kRtcFields, std::size(kRtcFields)},
  {Kind::MediaFeatures, "mediaFeatures", kMediaFeatureFields,
   std::size(kMediaFeatureFields)},
};
inline constexpr size_t kOwnerGroupCount = std::size(kOwnerGroups);
struct FieldValue {
  std::string text;
  std::vector<std::string> strings;
  int32_t integer = 0;
  bool boolean = false;
  double number = 0;
  bool null = false;
  // Object/Map payload, in declared-member or profile-key order; `strings`
  // holds one Map key per item. `present` marks a member the caller supplied.
  std::vector<FieldValue> items;
  bool present = false;
};
struct Change { Kind kind; uint32_t field; FieldValue value; };

// This bridge is compiled with C++ exceptions enabled. No exception crosses
// these noexcept methods into the SpiderMonkey adapter.
class Model final {
 public:
  Model() noexcept = default;
  ~Model() noexcept;
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;
  bool Initialize() noexcept;
  bool SetPlatformTouchSupport(bool supported) noexcept;
  bool TouchEventsEnabled(bool* result) const noexcept;
  bool Configure(const std::vector<Change>& changes) noexcept;
  bool Read(Kind kind, uint32_t field, FieldValue* result) const noexcept;
  bool ReadOriginValue(OriginValue*) const noexcept;
  static bool ReadUrlOrigin(const std::string&, OriginValue*) noexcept;
  static bool CopyOriginValue(const OriginValue&, OriginValue*) noexcept;
  // Session-history operations (T9). The adapter encodes the structured
  // clone and hands the opaque bytes here; the domain never sees a JS value.
  bool HistoryPush(const std::string& url, const std::vector<uint8_t>& state) noexcept;
  bool HistoryReplace(const std::string& url, const std::vector<uint8_t>& state) noexcept;
  bool HistoryGo(int32_t delta, bool* changed) noexcept;
  bool HistoryState(std::vector<uint8_t>* state) const noexcept;
  bool HistoryIndex(int32_t* index) const noexcept;
  bool HistoryCurrentUrl(std::string* url) const noexcept;
  // Navigation API (navigation.cpp): the session history as the Navigation
  // owner reads it, one navigate-state per entry, and same-document commits
  // that carry it.
  struct HistoryView { uint64_t id = 0, revision = 0; std::string url; bool sameDocument = false; };
  bool HistoryEntries(std::vector<HistoryView>* entries, int32_t* index) const noexcept;
  bool HistoryNavigationState(int32_t index, std::vector<uint8_t>* state) const noexcept;
  bool SetHistoryNavigationState(const std::vector<uint8_t>& state) noexcept;
  bool HistoryCommit(bool replace, const std::string& url, const std::vector<uint8_t>& state,
                     const std::vector<uint8_t>& navigation) noexcept;
  // A JS-driven navigation commit (Location.assign/href/component setters):
  // appends a document history entry, or replaces the current one without
  // changing the length (Location.replace / reload).
  bool Navigate(const std::string& url, bool replace) noexcept;
  // The session-history scrollRestoration flag (Firefox default "auto").
  bool ScrollRestoration(bool* manual) const noexcept;
  bool SetScrollRestoration(bool manual) noexcept;
 private:
  struct Impl;
  Impl* impl_ = nullptr;
};
}
#endif
