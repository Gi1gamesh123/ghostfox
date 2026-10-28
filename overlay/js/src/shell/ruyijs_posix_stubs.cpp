// POSIX v1 degradation points: symbols the Windows-native backends (CNG
// crypto, WIC image codecs, DirectWrite font config and glyph rasterization,
// worker child processes) define. Kept linkable so the portable JS/DOM/network
// core runs; every entry reports failure and the JS layer surfaces a clean
// error instead of reaching native code that does not exist here.
#include "web_crypto/cng_backend.h"
#include "web_crypto/digest_backend.h"
#include "web_geometry/font_config.h"
#include "web_geometry/glyph_mask.h"
#include "web_geometry/realm.h"
#include "web_geometry/svg_path.h"
#include "web_geometry/system_font.h"
#include "web_media/image_codec.h"
#include "web_workers/child.h"
#include "web_workers/process.h"
#include "web_workers/state.h"
#include "js/Class.h"

namespace ruyijs::web_crypto {
bool AesCbc(const Bytes&, const Bytes&, const Bytes&, bool, Bytes*) { return false; }
bool AesGcm(const Bytes&, const Bytes&, const Bytes&, size_t, const Bytes&, bool, Bytes*) {
  return false;
}
bool EcGenerate(const domain::crypto::EcCurve&, bool, domain::crypto::EcKey*) { return false; }
bool EcValidate(const domain::crypto::EcKey&, bool) { return false; }
bool EcdsaSign(const domain::crypto::EcKey&, HashAlgorithm, const Bytes&, Bytes*) { return false; }
bool EcdsaVerify(const domain::crypto::EcKey&, HashAlgorithm, const Bytes&, const Bytes&,
                 bool*) {
  return false;
}
bool ComputeDigest(HashAlgorithm, const void*, size_t, DigestBytes*) noexcept { return false; }
bool Hmac(HashAlgorithm, const Bytes&, const Bytes&, Bytes*) { return false; }
bool EcdhSecret(const domain::crypto::EcKey&, const domain::crypto::EcKey&, Bytes*) {
  return false;
}
}  // namespace ruyijs::web_crypto

namespace ruyijs::web_geometry {
bool ConfigureProfileGroup(void*, JS::HandleObject) { return false; }
bool SystemFonts::MeasureCanvas(const std::u16string&, const FontInput&, CanvasTextMetrics*) {
  return false;
}
std::string GenericFamilyOverride(const std::string&) { return {}; }
uint64_t FontConfigGeneration() { return 0; }
bool GlyphMask(const SelectedFont&, float, const float*, float, float, uint16_t,
               const uint8_t*, CanvasGlyphMask*) {
  return false;
}
bool GlyphColorMask(const SelectedFont&, float, bool, const float*, float, float, uint16_t,
                    const uint8_t*, CanvasGlyphMask*) {
  return false;
}
bool SvgPathBounds(const std::vector<domain::geometry::SvgPathCommand>&, float,
                   domain::geometry::SvgBox*) {
  return false;
}
}  // namespace ruyijs::web_geometry

namespace ruyijs::web_media {
domain::web_media::ImageDecodeStatus DecodeImageBytes(const std::string&,
                                                      const std::vector<uint8_t>&, uint32_t,
                                                      uint32_t, bool,
                                                      domain::web_media::ImageData*) {
  return {};  // No codec backend on this platform.
}
bool ImageCodecSupports(const std::string&) { return false; }
}  // namespace ruyijs::web_media

namespace ruyijs::web_workers {
Process::~Process() {}
bool Process::Queue(std::string) { return false; }
PipeResult Process::Poll(std::string*, bool*) { return PipeResult::Failed; }
bool Process::Stop() { return false; }
bool Process::Running() const { return false; }
StartResult Process::Start(std::wstring_view, std::string, WorkerKind) noexcept {
  StartResult result;
  result.error = StartError::Allocation;
  return result;
}
bool Child::Send(JSContext*, unsigned, JS::Value*) { return false; }
bool Child::Close(JSContext*, unsigned, JS::Value*) { return false; }
Child::Child(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
             interaction::Realm& events, Pipe& pipe, JS::HandleObject exception,
             WorkerKind kind)
    : cx_(cx), kit_(cx, page, helpers, this), events_(events), pipe_(pipe),
      globalScope_(cx), dedicatedScope_(cx), sharedScope_(cx), name_(cx),
      exception_(cx, exception), handlers_(cx), errorPrototype_(cx), kind_(kind) {}
bool Child::Queue(std::string) { return false; }
bool Child::Serve(Checkpoint, ExceptionCapture&, const ChildTasks&, const ChildStartup&) {
  return false;
}
bool Child::Exchange(std::string_view, std::string*) { return false; }
}  // namespace ruyijs::web_workers
