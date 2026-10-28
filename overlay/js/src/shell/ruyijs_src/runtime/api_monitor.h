#ifndef RUYIJS_API_MONITOR_H
#define RUYIJS_API_MONITOR_H
#include <memory>
#include <string>
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs {
// Holds the imported ApiMonitorAggregate. Only api_monitor.cpp sees that
// domain header, so the throwing engine-independent boundary stays inside one
// translation unit and no exception crosses the JS engine stack.
struct ApiMonitorState;

// Debug-session coordinator for the single API monitor of one Context. It
// installs no page property, constructor or prototype, so it cannot contend
// with a browser API installer; it only records accesses an installer already
// registered, reported through that owner's own port. Outside debug mode every
// entry point is inert and records nothing.
class ApiMonitor final {
 public:
  ApiMonitor(JSContext* cx, bool debug);
  ~ApiMonitor();
  ApiMonitor(const ApiMonitor&) = delete;
  ApiMonitor& operator=(const ApiMonitor&) = delete;
  bool enabled() const;
  // Port a domain owner calls from an install-registered member. |iface| and
  // |member| are that installer's own static names, never script-supplied.
  // |type| is the document_model::ApiAccess int contract.
  static void Port(void* context, const char* iface, const char* member, int type);
  void Record(const char* iface, const char* member, int type);
  // RPC: {op:"api_log", action:"peek"|"get"|"clear", filter?}
  bool Request(JS::HandleObject request, JS::MutableHandleValue result);

 private:
  bool Describe(const std::string& filter, bool drain, JS::MutableHandleValue result);
  JSContext* cx_;
  std::unique_ptr<ApiMonitorState> state_;
};
}
#endif
