// HTMLImageElement image fetch (ARCHITECTURE.md, image fetch). A src change
// starts the fetch through the asynchronous port and returns; the outcome runs
// later at a task boundary and fires the element's trusted load or error, as
// Firefox's LoadBlockingAsyncEventDispatcher does. While in flight the fetch
// delays the load event of the element's node document when that is this
// realm's main document (nsImageLoadingContent's ImageLoadTask BlockOnload).
// Retiring the document cancels its fetches and loads no new ones.
#include "document_model/state.h"
#include "document_model/model_hyperlink.h"
#include "browser_environment/environment.h"
#include "runtime/runtime.h"
#include "interaction/realm.h"
#include "jsfriendapi.h"
#include "js/Object.h"
#include "js/Realm.h"
#include <algorithm>
#include <cstring>
#include <memory>
#include "js/RootingAPI.h"

namespace ruyijs::document_model {
void ImageSourceChanged(State&, NodeKey);
// One in-flight fetch: the element (rooted until the outcome) and the
// generation it was started for. `owner` is cleared when the realm's State goes.
struct ImageRequest {
  State* owner;
  JS::PersistentRootedObject image;
  uint32_t generation;
  bool blocking;
  ImageRequest(State* state, JSObject* element, uint32_t current, bool blocks)
      : owner(state), image(state->cx, element), generation(current), blocking(blocks) {}
};
namespace {
const JSClass kImageTaskClass = {"Image source task", JSCLASS_HAS_RESERVED_SLOTS(2)};
bool RunImageSourceTask(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  if (!state) { args.rval().setUndefined(); return true; }
  const auto keys = std::move(state->pendingImageKeys);
  state->pendingImageKeys.clear();
  for (const auto& key : keys) ImageSourceChanged(*state, key);
  args.rval().setUndefined();
  return true;
}
void QueueImageSourceImpl(State& state, NodeKey key) {
  for (const auto& pending : state.pendingImageKeys)
    if (Same(pending, key)) return;
  state.pendingImageKeys.push_back(key);
  if (state.pendingImageKeys.size() != 1) return;
  JSContext* cx = state.cx;
  JS::RootedFunction fn(cx, js::NewFunctionWithReserved(cx, RunImageSourceTask, 0, 0,
                                                        "Image source task"));
  if (!fn) return;
  JS::RootedObject callback(cx, JS_GetFunctionObject(fn));
  js::SetFunctionNativeReserved(callback, 0, JS::PrivateValue(&state));
  if (!state.environment.PostTask(callback)) {
    state.pendingImageKeys.clear();
  }
}
// The outcome, on the owning thread at a task boundary.
void ImageLoaded(void* raw, const ImageLoadOutcome& outcome) {
  std::unique_ptr<ImageRequest> request(static_cast<ImageRequest*>(raw));
  State* state = request->owner;
  if (!state) return;
  auto& pending = state->imageRequests;
  pending.erase(std::remove(pending.begin(), pending.end(), request.get()), pending.end());
  if (request->blocking && state->imageLoadBlockers) --state->imageLoadBlockers;
  // A document whose ports were cleared (navigated away, frame removed) fires nothing.
  if (!state->imageLoad) return;
  JSContext* cx = state->cx;
  JS::RootedObject image(cx, request->image);
  auto* entry = static_cast<Wrapper*>(JS::GetReservedSlot(image, 0).toPrivate());
  if (!entry || entry->dead || entry->imageGeneration != request->generation) return;
  if (outcome.loaded) {
    entry->imageNaturalWidth = outcome.width;
    entry->imageNaturalHeight = outcome.height;
    entry->imageMime = outcome.mime;
    entry->imageBytes = outcome.bytes;
  } else {
    entry->imageMime.clear();
    entry->imageBytes.clear();
  }
  entry->imageComplete = true;
  JSAutoRealm realm(cx, image);
  (void)entry->owner->interaction.Fire(image, outcome.loaded ? "load" : "error", false, false);
}
}
void QueueImageSource(State& state, NodeKey key) { QueueImageSourceImpl(state, key); }
void OrphanImageRequests(State& state, bool settle) {
  for (ImageRequest* request : state.imageRequests) {
    // A retired document's fetch is cancelled: the element is complete
    // (broken) and fires nothing, as Firefox does for a removed frame.
    if (settle && request->image) {
      auto* entry = static_cast<Wrapper*>(JS::GetReservedSlot(request->image, 0).toPrivate());
      if (entry && !entry->dead && entry->imageGeneration == request->generation)
        entry->imageComplete = true;
    }
    request->owner = nullptr;
    request->image.reset();
  }
  state.imageRequests.clear();
  state.imageLoadBlockers = 0;
}
void ImageSourceChanged(State& state, NodeKey key) {
  NodeSnapshot node;
  if (!state.Check(state.model.Inspect(key, &node)) ||
      std::strcmp(node.interface_name, "HTMLImageElement")) return;
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.page);
  JS::RootedValue element(cx);
  if (!state.Wrap(key, &element)) return;
  JS::RootedObject object(cx, &element.toObject());
  Wrapper* entry = state.Find(object);
  if (!entry) return;
  ++entry->imageGeneration;
  entry->imageComplete = false;
  entry->imageNaturalWidth = 0;
  entry->imageNaturalHeight = 0;
  entry->imageMime.clear();
  entry->imageBytes.clear();
  std::optional<DomString> src;
  if (!state.Check(state.model.GetAttribute(key, u"src", &src))) return;
  if (!src) { entry->imageComplete = true; return; }
  std::string href, referer;
  DocumentSnapshot document;
  JS::RootedValue base(cx), source(cx);
  // Firefox does not resolve src="" against the document. The original async
  // port delivers this empty-URL failure as a later error event without a fetch.
  if (!src->empty() && state.Check(state.model.InspectDocument(node.owner_document, &document)) &&
      (document.url ? Value(cx, DomStringView(*document.url), &base) : state.environment.URL(&base)) &&
      Value(cx, DomStringView(*src), &source)) {
    JS::RootedString baseString(cx, base.toString()), sourceString(cx, source.toString());
    std::string raw;
    if (!ToUTF8(cx, baseString, &referer) || !ToUTF8(cx, sourceString, &raw) ||
        !state.Check(ReadHyperlink(raw, referer, HyperlinkField::Href, &href))) href.clear();
  }
  // An inactive (retired) document loads no image and fires nothing.
  if (!state.imageLoad) { entry->imageComplete = true; return; }
  const bool blocking = Same(node.owner_document, state.main);
  auto* request = new ImageRequest(&state, object, entry->imageGeneration, blocking);
  state.imageRequests.push_back(request);
  if (blocking) ++state.imageLoadBlockers;
  state.imageLoad(state.imageLoadContext, href, referer, &ImageLoaded, request);
}
}
