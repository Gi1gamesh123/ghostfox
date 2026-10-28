#pragma once
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs {
// The global traces owner-created objects; it does not own their API or state.
enum class PageReference : unsigned { Crypto, Geolocation, ResourceStrategyPrototypes,
                                     ResourceStreamPrototypes, ResourceWritablePrototypes, ResourceTransformPrototype, ResourceTransformConstructor, PlatformBodyFactories, PlatformDecoderStreamPrototype,
                                     PlatformCompressionPrototype, PlatformDecompressionPrototype, MediaImageFactories, ResourceTransportPrototypes,
                                     Frames, Count };
bool ReadPageReference(JS::HandleObject page, PageReference, JS::MutableHandleValue);
bool WritePageReference(JS::HandleObject page, PageReference, JS::HandleValue);
// The document-ordered child browsing contexts of a Window, for its indexed
// properties (window[i] is the i-th child WindowProxy, as Firefox's
// nsOuterWindowProxy). The port is owned by the BrowsingContext; Clear before
// the owners die. Invalidate drops the resolved index properties after the
// child list changed (they resolve again on the next access).
struct PageFrames {
  size_t (*count)(void*) = nullptr;
  bool (*at)(void*, size_t, JS::MutableHandleValue) = nullptr;
  void* context = nullptr;
  size_t resolved = 0;  // one past the highest index property defined
};
void SetPageFrames(JS::HandleObject page, PageFrames* frames);
bool InvalidatePageFrames(JSContext* cx, JS::HandleObject page);
}
