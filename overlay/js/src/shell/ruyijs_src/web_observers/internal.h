#ifndef RUYIJS_WEB_OBSERVERS_INTERNAL_H
#define RUYIJS_WEB_OBSERVERS_INTERNAL_H
#include <memory>
#include <string>
#include <vector>
#include "document_model/model.h"
#include "js/Class.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "web_observers/mutation_state.h"
#include "web_observers/mutation_group.h"

namespace ruyijs { class PageHelpers; }
namespace ruyijs::document_model { class Realm; struct NodeLifecycleNotice; }
namespace ruyijs::web_observers {
using document_model::DomString;
using document_model::MutationKind;
using document_model::MutationNotice;
using document_model::NodeKey;

struct State;

// One IntersectionObserver instance of this realm. targets is its
// [[ObservationTargets]] list in observe() order; queued holds the entries the
// next delivery (or takeRecords) hands out. thresholds is the sorted sequence
// and cachedThresholds the [Cached] array the getter returns, so repeated
// reads give one object like the donor's lazily-cached reserved slot.
struct Intersection {
  State* owner = nullptr;
  JS::PersistentRootedObject object, callback, cachedThresholds;
  NodeKey root;  // Null key = implicit root.
  DomString rootMargin, scrollMargin;
  std::vector<double> thresholds;
  std::vector<NodeKey> targets, queued;
  bool scheduled = false;  // The realm delivery microtask covers this observer.
  std::unique_ptr<Intersection> next;
  Intersection(JSContext* cx, JS::HandleObject value, JS::HandleObject callable)
      : object(cx, value), callback(cx, callable), cachedThresholds(cx) {}
};

// One ResizeObserver observation. A repeated observe() of the same target
// replaces the box and queues nothing, matching the donor algorithm.
struct ResizeObservation {
  NodeKey target;
  DomString box;
};

// One ResizeObserver instance of this realm. The donor has no takeRecords, so
// queued entries leave only through delivery.
struct Resize {
  State* owner = nullptr;
  JS::PersistentRootedObject object, callback;
  std::vector<ResizeObservation> targets;
  std::vector<NodeKey> queued;
  bool scheduled = false;
  std::unique_ptr<Resize> next;
  Resize(JSContext* cx, JS::HandleObject value, JS::HandleObject callable)
      : object(cx, value), callback(cx, callable) {}
};

// One ReportingObserver instance of this realm. This engine has no report
// source, so the registration only stores the type filter and buffered flag;
// takeRecords hands out the always-empty report list.
struct Reporting {
  State* owner = nullptr;
  JS::PersistentRootedObject object, callback;
  std::vector<DomString> types;
  bool buffered = false, registered = false;
  std::unique_ptr<Reporting> next;
  Reporting(JSContext* cx, JS::HandleObject value, JS::HandleObject callable)
      : object(cx, value), callback(cx, callable) {}
};

struct State {
  State(JSContext*, JS::HandleObject, PageHelpers&, document_model::Realm*);  // null: a Worker realm
  ~State();
  Observer* New(JS::HandleObject object, JS::HandleObject callback);
  void Observe(Observer&, const Registration&);
  void Disconnect(Observer&);
  bool TakeRecords(Observer&, JS::MutableHandleObject result);
  // Turns the owner's recording flag and sink on or off as the first and last
  // registration come and go, so an unobserved page pays one flag test.
  void RefreshRecording();
  void JoinMutationGroup(const std::shared_ptr<MutationGroup>&);
  void NodeLifecycle(const document_model::NodeLifecycleNotice&) noexcept;
  void Sink();
  void DeliverAll();
  bool BuildBatch(const PendingRecords&, JS::MutableHandleObject result);
  bool RecordOf(const Pending&, JS::MutableHandleObject result);
  bool Resolve(NodeKey key, JS::MutableHandleObject result);
  bool Matches(Observer&, const MutationNotice&, bool* matched, bool* wants_old_value);
  bool Append(Observer&, const MutationNotice&, bool wants_old_value);
  // DOM "remove" step: a subtree registration covering the parent follows each
  // removed node as a transient registration (mutation_registration.cpp).
  bool AddTransients(Observer&, const MutationNotice&);
  // Drops transient registrations: all of them (origin 0) or one origin's.
  void RemoveTransients(Observer&, uint64_t origin);
  // Queues one delivery callback on the Context's single microtask queue.
  bool Queue(JS::HandleObject callback);
  bool QueueDelivery();
  // The T11c observers. Each kind owns its own linked list, but all three
  // reuse this State, this realm's prototypes and the one microtask port.
  Intersection* NewIntersection(JS::HandleObject object, JS::HandleObject callback);
  Resize* NewResize(JS::HandleObject object, JS::HandleObject callback);
  Reporting* NewReporting(JS::HandleObject object, JS::HandleObject callback);
  void DeliverIntersections();
  void DeliverResizes();
  bool BuildIntersectionEntry(JS::HandleObject target, JS::MutableHandleObject result);
  bool BuildResizeEntry(JS::HandleObject target, JS::MutableHandleObject result);
  // A plain object with the eight DOMRectReadOnly members. The DOMRect owner
  // (T33) does not exist yet, so this is the recorded D59/D60 substitute.
  bool Rect(double x, double y, double width, double height, JS::MutableHandleObject result);
  // A branded ResizeObserverSize instance of this realm.
  bool Size(double inline_size, double block_size, JS::MutableHandleObject result);
  // The window's innerWidth/innerHeight, read from the installed facade so the
  // configured screen profile stays the single source of the viewport.
  bool Viewport(double* width, double* height);
  // Milliseconds on the Context's single clock; zero while unbound.
  double Time();
  // WebIDL (Element or Document) conversion support: the value is an object of
  // this realm whose prototype chain reaches the installed prototype.
  bool IsElement(JS::HandleValue value) const;
  bool IsDocument(JS::HandleValue value) const;

  JSContext* cx;
  JS::PersistentRootedObject page, observerPrototype, recordPrototype, deliver;
  // T11c interfaces of this realm, captured by their installers.
  JS::PersistentRootedObject elementPrototype, documentPrototype;
  JS::PersistentRootedObject intersectionPrototype, intersectionEntryPrototype;
  JS::PersistentRootedObject resizePrototype, resizeEntryPrototype, resizeSizePrototype;
  JS::PersistentRootedObject reportingPrototype, reportPrototype, reportBodyPrototype;
  JS::PersistentRootedObject intersectionDeliver, resizeDeliver;
  PageHelpers* helpers = nullptr;
  document_model::Realm* document = nullptr;
  std::shared_ptr<MutationGroup> mutationGroup;
  size_t registrationCount = 0;
  using MicrotaskPort = bool (*)(void*, JS::HandleObject);
  MicrotaskPort microtask = nullptr;
  void* microtaskContext = nullptr;
  // The Scheduling owner's clock, in milliseconds; bound after it exists.
  using ClockPort = double (*)(void* context);
  ClockPort clock = nullptr;
  void* clockContext = nullptr;
  std::unique_ptr<Observer> observers;
  std::unique_ptr<Intersection> intersections;
  std::unique_ptr<Resize> resizes;
  std::unique_ptr<Reporting> reportings;
  size_t count = 0, intersectionCount = 0, resizeCount = 0, reportingCount = 0;
  uint64_t cursor = 0;
  bool recording = false;
  static constexpr size_t kObserverLimit = 1024;
};

// Native entry points and instance factories shared by install.cpp and the
// per-kind source files. Slot 0 of every function is the owning State, slot 1
// the record field index for the [Constant] getters.
JSObject* Function(State& state, JSNative native, const char* name, unsigned length,
                   int32_t slot, unsigned flags = 0);
// Shared installer scaffolding: one prototype + interface object pair, the
// member installers and the Trace registration that must accompany each one.
bool Tag(JSContext* cx, JS::HandleObject object, const char* text);
bool NewInterface(State& state, const char* name, unsigned length, JSNative constructor,
                  JS::MutableHandleObject prototype, JS::MutableHandleObject object);
// Links constructor and prototype. The donor defines the prototype's members
// first, then @@toStringTag, then constructor, so this runs after the members.
bool LinkInterface(State& state, JS::HandleObject prototype, JS::HandleObject object);
bool Getter(State& state, const char* iface, JS::HandleObject prototype,
            const char* name, JSNative native, int32_t slot);
bool Method(State& state, const char* iface, JS::HandleObject prototype,
            const char* name, JSNative native, unsigned length);
bool ObserverConstructor(JSContext*, unsigned, JS::Value*);
bool RecordConstructor(JSContext*, unsigned, JS::Value*);
bool RecordMember(JSContext*, unsigned, JS::Value*);
bool Observe(JSContext*, unsigned, JS::Value*);
bool Disconnect(JSContext*, unsigned, JS::Value*);
bool TakeRecords(JSContext*, unsigned, JS::Value*);
bool Deliver(JSContext*, unsigned, JS::Value*);
bool InstallIntersection(State& state);
bool InstallResize(State& state);
bool InstallReporting(State& state);
// Window-only ReportBody subclasses, after InstallReporting (reporting_bodies.cpp).
bool InstallReportBodies(State& state);
// A TypeError in the observer's realm: through the Document Model's factory in a
// Window, the engine's own TypeError in a Worker realm (no document).
bool ThrowType(State* state, const char* message);
bool IntersectionDeliver(JSContext*, unsigned, JS::Value*);
bool ResizeDeliver(JSContext*, unsigned, JS::Value*);
bool IntersectionConstructor(JSContext*, unsigned, JS::Value*);
bool IntersectionEntryConstructor(JSContext*, unsigned, JS::Value*);
bool IntersectionMember(JSContext*, unsigned, JS::Value*);
bool IntersectionEntryMember(JSContext*, unsigned, JS::Value*);
bool IntersectionObserve(JSContext*, unsigned, JS::Value*);
bool IntersectionUnobserve(JSContext*, unsigned, JS::Value*);
bool IntersectionDisconnect(JSContext*, unsigned, JS::Value*);
bool IntersectionTakeRecords(JSContext*, unsigned, JS::Value*);
bool ResizeConstructor(JSContext*, unsigned, JS::Value*);
bool ResizeEntryConstructor(JSContext*, unsigned, JS::Value*);
bool ResizeSizeConstructor(JSContext*, unsigned, JS::Value*);
bool ResizeEntryMember(JSContext*, unsigned, JS::Value*);
bool ResizeObserve(JSContext*, unsigned, JS::Value*);
bool ResizeUnobserve(JSContext*, unsigned, JS::Value*);
bool ResizeDisconnect(JSContext*, unsigned, JS::Value*);
bool ReportingConstructor(JSContext*, unsigned, JS::Value*);
bool ReportConstructor(JSContext*, unsigned, JS::Value*);
bool ReportBodyConstructor(JSContext*, unsigned, JS::Value*);
bool ReportMember(JSContext*, unsigned, JS::Value*);
bool ReportBodyToJSON(JSContext*, unsigned, JS::Value*);
bool ReportToJSON(JSContext*, unsigned, JS::Value*);
bool ReportingObserve(JSContext*, unsigned, JS::Value*);
bool ReportingDisconnect(JSContext*, unsigned, JS::Value*);
bool ReportingTakeRecords(JSContext*, unsigned, JS::Value*);
// The Document Model sink entry point. It is a plain C function because the
// owner stores no type of this domain; the context is the State above.
void SinkThunk(void* context);
void NodeLifecycleThunk(void*, const document_model::NodeLifecycleNotice&) noexcept;
extern const JSClass kObserverClass, kRecordClass;
extern const JSClass kIntersectionClass, kIntersectionEntryClass;
extern const JSClass kResizeClass, kResizeEntryClass, kResizeSizeClass;
extern const JSClass kReportingClass, kReportBodyClass, kReportClass;
}
#endif
