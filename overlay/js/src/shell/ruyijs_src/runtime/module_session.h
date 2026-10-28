#ifndef RUYIJS_RUNTIME_MODULE_SESSION_H
#define RUYIJS_RUNTIME_MODULE_SESSION_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "js/Modules.h"
#include "js/RootingAPI.h"

struct JSContext;
struct JSRuntime;
class JSObject;

namespace ruyijs::resource_network { class Model; }

namespace ruyijs {

// One parser-produced document owns one module map.  The map is deliberately
// independent of PageLoad's classic-script preload table: module records are
// GC-rooted until this document generation is retired, which preserves cycles,
// duplicate imports and a pending top-level-await evaluation promise.
class ModuleSession final {
 public:
  class Scope final {
   public:
    explicit Scope(ModuleSession& session) noexcept;
    ~Scope() noexcept;
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
   private:
    ModuleSession* session_;
    ModuleSession* previous_;
  };

  ModuleSession(JSContext* cx, resource_network::Model& resources);
  ~ModuleSession() noexcept;
  ModuleSession(const ModuleSession&) = delete;
  ModuleSession& operator=(const ModuleSession&) = delete;

  // Compile or retrieve the canonical URL, load its static dependency graph,
  // and return the module record.  Imported bytes are fetched synchronously
  // through the existing Resource Network owner.
  bool Load(const std::string& url, const std::string& source,
            JS::MutableHandleObject module);
  // Starts ModuleEvaluate and returns its promise (including a pending TLA
  // promise).  The caller intentionally does not wait for or drain this value.
  bool Evaluate(JS::HandleObject module, JS::MutableHandleValue promise);

  JSContext* context() const noexcept { return cx_; }
  // The session serving module loads in |cx| right now: the one PageLoad has
  // scoped, otherwise the live session of the document whose realm is current
  // (a dynamic import() from page script, after the parser is done).
  static ModuleSession* Current(JSContext*) noexcept;
  // A retired document generation keeps its records and roots for jobs still
  // in flight, but no longer serves new dynamic imports.
  void Retire() noexcept { retired_ = true; }

 private:
  struct Record {
    std::string url;
    std::unique_ptr<JS::PersistentRootedObject> module;
  };

  static bool LoadImportedModule(JSContext*, JS::HandleValue,
                                 JS::HandleObject, JS::HandleValue,
                                 JS::HandleValue, uint32_t,
                                 JS::ColumnNumberOneOrigin);
  static bool PopulateImportMeta(JSContext*, JS::HandleObject,
                                 JS::HandleObject);
  static bool LinkLoaded(JSContext*, JS::HandleValue);
  static bool LinkRejected(JSContext*, JS::HandleValue, JS::HandleValue);
  // A dynamic import() whose module is new loads its static graph first (as
  // Firefox's ModuleLoaderBase does in the host) and only then completes the
  // engine's HostLoadImportedModule with that module.
  bool LoadDynamic(JS::HandleValue referrer, JS::HandleObject request,
                   JS::HandleValue payload);
  static bool DynamicLoaded(JSContext*, JS::HandleValue);
  static bool DynamicRejected(JSContext*, JS::HandleValue, JS::HandleValue);
  static bool EnsureHooks(JSRuntime*);

  bool FetchAndCompile(const std::string& url, JS::MutableHandleObject module);
  // Set while a dynamic import() resolves, so a failed fetch reports Firefox's
  // "error loading dynamically imported module" TypeError.
  bool dynamic_ = false;
  // |fresh| is set when the module was fetched and compiled by this call.
  bool ResolveImport(JS::HandleValue referrer, JS::HandleObject request,
                     JS::MutableHandleObject module, bool* fresh = nullptr);
  bool ToUtf8(JS::HandleString value, std::string* output);
  bool SetPrivateUrl(JS::HandleObject module, const std::string& url);
  JSObject* Find(const std::string& url) const noexcept;

  // The base URL a classic script or eval import() resolves against.
  bool DocumentBase(std::string* base);

  JSContext* cx_;
  resource_network::Model& resources_;
  JS::PersistentRootedObject global_;
  bool retired_ = false;
  JS::PersistentRootedObject hostDefined_;
  JS::PersistentRootedObject root_;
  std::map<std::string, Record> records_;
  static thread_local ModuleSession* current_;
  static thread_local std::vector<ModuleSession*> live_;
};

}  // namespace ruyijs

#endif
