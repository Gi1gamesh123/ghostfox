#ifndef RUYIJS_FONT_LOADING_INTERNAL_H
#define RUYIJS_FONT_LOADING_INTERNAL_H
#include "js/CallArgs.h"
#include "js/ErrorReport.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "font_loading/realm.h"
namespace ruyijs { class PageHelpers; }
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::font_loading {
using Native = bool (*)(JSContext* cx, unsigned argc, JS::Value* vp);
// FontFace keeps every observable in reserved slots: the validated family, the
// status machine, the binary loadability flag, the cached load() promise and the
// seven CSS descriptors. No other member owns this state.
enum FontFaceSlot {
  kFontFamily = 0, kFontStatus, kFontLoadable, kFontPromise,
  kFontStyle, kFontWeight, kFontStretch, kFontUnicodeRange,
  kFontFeatureSettings, kFontVariant, kFontDisplay, kFontSlotCount
};
// FontFaceSet keeps the insertion-ordered faces, the ready promise and the
// loading status. It owns no second index of the tree.
enum FontFaceSetSlot {
  kSetFaces = 0, kSetReady, kSetStatus, kSetSlotCount
};
extern const JSClass kFontFaceClass;
extern const JSClass kFontFaceSetClass;
bool IsFontFace(JSObject* object);
// Installs the FontFace constructor and prototype in the current realm and
// returns the constructor object, or null on failure.
JSObject* InstallFontFace(JSContext* cx, JS::HandleObject page, PageHelpers& helpers);
// Installs the seven validated CSS descriptors on an existing prototype.
bool InstallFontFaceAttributes(JSContext* cx, JS::HandleObject prototype, PageHelpers& helpers);
// The seven FontFaceDescriptors keys, in IDL order.
extern const char* const kFontDescriptorNames[7];
// Applies one FontFaceDescriptors key; an undefined value restores the default
// and recognized stays false for unknown keys.
bool ApplyFontFaceDescriptor(JSContext* cx, JS::HandleObject face, const char* name,
                             JS::HandleValue value, bool* recognized);
// Runs the per-instance load state machine; the promise settles at most once.
bool LoadFontFace(JSContext* cx, JS::HandleObject face, JS::MutableHandleObject promise);
// Installs FontFaceSet, its document.fonts accessor and the loading/loadingdone/
// loadingerror events on the shared interaction dispatch tree.
bool InstallFontFaceSet(JSContext* cx, JS::HandleObject page, JS::HandleObject document,
                        PageHelpers& helpers, interaction::Realm& events,
                        JS::HandleObject constructor, JS::HandleObject holder, ReceiverBrand isDocument);
// FontFaceSet.prototype.load / check (font_face_set_match.cpp).
bool Load(JSContext* cx, unsigned argc, JS::Value* vp);
bool Check(JSContext* cx, unsigned argc, JS::Value* vp);
// FontFaceSet receiver check, shared by the set and its iteration helpers.
bool SetReceiver(JSContext* cx, const JS::CallArgs& args, JS::MutableHandleObject self);
// Installs forEach, values/keys/entries and @@iterator on a set prototype.
bool InstallFontFaceSetIterators(JSContext* cx, JS::HandleObject setPrototype, PageHelpers& helpers);
bool Fail(JSContext* cx, JSExnType type, const char* message);
bool SyntaxErrorValue(JSContext* cx, JS::MutableHandleValue value);
JSObject* Function(JSContext* cx, Native native, const char* name, unsigned length,
                   unsigned flags = 0);
bool Tag(JSContext* cx, JS::HandleObject object, const char* text);
bool Receiver(JSContext* cx, const JS::CallArgs& args, JS::MutableHandleObject self);
}  // namespace ruyijs::font_loading
#endif
