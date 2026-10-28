#ifndef RUYIJS_RESOURCE_BODY_H
#define RUYIJS_RESOURCE_BODY_H
#include <string>
#include "resource_network/model.h"
#include "js/CallArgs.h"
#include "js/RootingAPI.h"
#include "js/ValueArray.h"

namespace ruyijs { class PageHelpers; }
namespace ruyijs::resource_network {
struct State;

// One Request/Response body: the bytes plus the single consumption flag. The
// bytes come from the resource owner or from the constructor argument; the
// object keeps no second copy of resource state.
enum class BodySource { OwnBytes, NativeBytesAlias, ScriptStream };
struct BodyRecord {
  BodySource source=BodySource::OwnBytes;
  std::string bytes;
  BodyKind kind = BodyKind::Text;
  bool used = false;
  bool nullBody = true;  // Absence is distinct from a present zero-byte body.
};
// The body behind a Request/Response receiver, or nullptr for any other value.
BodyRecord* BodyOf(JS::HandleValue);
BodyRecord* BodyOf(JS::HandleObject);
JSObject* NewBodyHolder(State&);
void CommitBodyHolder(JSObject*,BodyRecord*);
JSObject* BodyHolderOf(JS::HandleValue);
JSObject* BodyReceiver(JS::HandleValue);
BodyRecord* HolderBody(JSObject*);
bool BodyLocked(JSObject* holder);
bool BodyUsed(JSObject* holder);
bool StreamDisturbed(JSObject*);
const BodyRecord* BodyBytesOf(JSObject* holder);
bool SetResponseStream(JSContext*,JS::HandleObject holder,JS::HandleObject source);
bool CopyBodyHeaders(State&,JS::HandleObject,JS::MutableHandleObject);
bool ResponseClone(JSContext*,const JS::CallArgs&);
bool BeginBodyConsume(JSContext*,JS::HandleValue);
// The interface whose installer registered the Body mixin member being
// dispatched (slot 1 of the callee): "Request" or "Response". Static, so a
// record never carries script-supplied text.
const char* BodyInterface(const JS::CallArgs&);
// Fills the record from a constructor argument. False reports the conversion
// error; null and undefined leave the body absent.
bool BodyInput(State&, JS::HandleValue, BodyRecord*);
// Starts one consumption of the body; a second read rejects with the TypeError
// the shell builds. text/json/arrayBuffer and blob/formData all go through this,
// so the one-shot rule has one implementation.
bool ConsumeBody(JSContext*, const JS::CallArgs&, const char* method, BodyRecord**, bool* consumed);
bool BodyOperationError(JSContext*, const char* interfaceName, const char* method, bool invalidReceiver);
// Body.blob() and Body.formData(): both build their result through the page
// realm's own Blob/File/FormData constructors, so no second object factory.
bool BlobMethod(JSContext*, unsigned, JS::Value*);
bool FormDataMethod(JSContext*, unsigned, JS::Value*);
// The page-constructor and MIME helpers the two readers share.
bool HeaderValue(State&, JS::HandleValue, const char*, JS::MutableHandleValue);
bool MimeType(State&, JS::HandleValue, std::string* full);
std::string MimeEssence(const std::string& full);
// Installs the shared methods and bodyUsed/headers once per interface. Firefox
// exposes the body accessor only on Response.
bool InstallBody(State&, PageHelpers&, const char*, JS::HandleObject prototype);
// The public ReadableStream constructor and its pipeTo/getReader prototype
// methods. Body instances use the same class and prototype.
bool StreamConstructor(JSContext*, unsigned, JS::Value*);
bool StreamGetReader(JSContext*, unsigned, JS::Value*);
bool StreamPipeTo(JSContext*, unsigned, JS::Value*);
}  // namespace ruyijs::resource_network
#endif
