#ifndef RUYIJS_WEB_CLIPBOARD_INTERNAL_H
#define RUYIJS_WEB_CLIPBOARD_INTERNAL_H
#include "web_clipboard/realm.h"
#include "runtime/secure_slot.h"
#include "runtime/web_kit.h"
#include <string>

namespace ruyijs::web_clipboard {
// A ClipboardItem: presentation style, the cached frozen types array and one
// entry object per representation, in construction order.
enum ItemSlot { ItemStyle = 0, ItemTypes = 1, ItemEntries = 2, ItemOwner = 3, ItemSlotCount = 4 };
// One representation (donor ClipboardItem::ItemEntry): its normalized type,
// the load status of its data promise, the settled DOMString or Blob, and the
// getType() promises waiting for it.
enum EntrySlot { EntryType = 0, EntryStatus = 1, EntryData = 2, EntryPending = 3,
                 EntrySlotCount = 4 };
enum EntryState { Loading = 0, Loaded = 1, Failed = 2 };
extern const JSClass kItemClass, kEntryClass;

struct State {
  State(JSContext* cx, JS::HandleObject page, PageHelpers& helpers)
      : cx(cx), kit(cx, page, helpers, this), face(cx), clipboardFace(cx), blob(cx), exception(cx),
        clipboard(cx), secure(cx) {}
  JSContext* cx;
  WebKit kit;
  WebFace face, clipboardFace;
  JS::PersistentRootedObject blob, exception, clipboard;
  Realm::BlobBrandPort blobBrand = nullptr;
  // Interaction's EventTarget registration for the Clipboard instance.
  Realm::AttachPort attach = nullptr;
  void* attachContext = nullptr;
  // [SecureContext]: navigator.clipboard, Clipboard, ClipboardItem.
  SecureSlots secure;
};
extern const JSClass kClipboardClass;
// navigator.clipboard and the Clipboard interface (clipboard.cpp).
bool InstallClipboard(State& state, JS::HandleObject eventTarget, JS::HandleObject navigatorPrototype);

// The ClipboardItem behind |self|, or a pending TypeError naming |member|.
JSObject* ItemReceiver(JSContext* cx, JS::HandleValue self, const char* member);
bool Construct(JSContext* cx, unsigned argc, JS::Value* vp);
bool StyleGet(JSContext* cx, unsigned argc, JS::Value* vp);
bool TypesGet(JSContext* cx, unsigned argc, JS::Value* vp);
bool GetType(JSContext* cx, unsigned argc, JS::Value* vp);
bool Supports(JSContext* cx, unsigned argc, JS::Value* vp);
// Starts loading one entry from its data promise (item_data.cpp).
bool LoadEntry(State& state, JS::HandleObject entry, JS::HandleObject promise);

bool ToUtf16(JSContext* cx, JS::HandleValue value, std::u16string* out);
JSString* NewString(JSContext* cx, const std::u16string& text);
// Throws a TypeError whose message may carry script-supplied text.
bool ThrowType(JSContext* cx, const std::u16string& message);
// The settled-promise forms of a failed promise-returning operation.
bool RejectPending(State& state, JS::MutableHandleValue result);
bool NewDomException(State& state, const std::u16string& message, const char* name,
                     JS::MutableHandleValue error);
}
#endif
