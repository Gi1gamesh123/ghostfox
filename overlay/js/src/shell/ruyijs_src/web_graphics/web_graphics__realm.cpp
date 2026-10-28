#include "web_graphics/realm.h"

#include "browser_environment/environment.h"
#include "js/Realm.h"
#include "web_graphics/internal.h"

namespace ruyijs::web_graphics {
State::State(JSContext* js, JS::HandleObject page, PageHelpers& helpers,
             browser_environment::Environment& owner)
    : cx(js), environment(&owner), kit(js, page, helpers, this),
      context(js), buffer(js), texture(js), framebuffer(js), renderbuffer(js),
      program(js), shader(js), uniform(js), context2(js), query(js), sampler(js), sync(js),
      transformFeedback(js), vertexArray(js), activeInfo(js), precision(js), event(js),
      eventMessages(js), eventFactory(js), extensionPrototypes(js) {}
State::~State() = default;

WebFace& State::Face(int32_t kind) {
  switch (kind) {
    case kBufferObject: return buffer;
    case kTextureObject: return texture;
    case kFramebufferObject: return framebuffer;
    case kRenderbufferObject: return renderbuffer;
    case kProgramObject: return program;
    case kShaderObject: return shader;
    case kQueryObject: return query;
    case kSamplerObject: return sampler;
    case kSyncObject: return sync;
    case kTransformFeedbackObject: return transformFeedback;
    case kVertexArrayObject: return vertexArray;
    default: return uniform;
  }
}

Context* State::Find(JSObject* canvas, int32_t version) const {
  for (const auto& entry : contexts)
    if (entry->canvas == canvas && entry->version == version) return entry.get();
  return nullptr;
}

Context* State::FindOrCreate(JSObject* canvas, int32_t version) {
  if (Context* found = Find(canvas, version)) return found;
  auto entry = std::make_unique<Context>(cx, version);
  entry->canvas = canvas;
  contexts.push_back(std::move(entry));
  return contexts.back().get();
}

Realm::Realm(JSContext* js, JS::HandleObject page, PageHelpers& helpers,
             browser_environment::Environment& owner)
    : state_(std::make_unique<State>(js, page, helpers, owner)) {}
Realm::~Realm() = default;

State* Realm::state() const noexcept { return state_.get(); }

bool Realm::Install() {
  State& s = *state_;
  if (!s.model.Initialize()) {
    JS_ReportOutOfMemory(s.cx);
    return false;
  }
  // Every constructor, prototype and object below belongs to the installing
  // realm, so an iframe gets its own WebGL interfaces and never aliases the
  // parent's.
  const JSAutoRealm realm(s.cx, s.kit.page);
  if (!InstallContext(s) || !InstallObjects(s) || !InstallPublicSurface(s) ||
      !InstallResultInterfaces(s) || !InstallExtensions(s) || !InstallContextEvent(s)) return false;
  s.environment->SetGraphicsPort(&GraphicsPort, &s);
  s.environment->SetGraphicsDescribe(&GraphicsDescribe, &s);
  return true;
}
}  // namespace ruyijs::web_graphics
