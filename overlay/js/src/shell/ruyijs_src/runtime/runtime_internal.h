#ifndef RUYIJS_RUNTIME_INTERNAL_H
#define RUYIJS_RUNTIME_INTERNAL_H
#include "runtime/runtime.h"
#include "runtime/exception_capture.h"

namespace ruyijs {
class EnvironmentControl;
class PageHelpers;
class SchedulingControl;
class ResourceControl;
class PageLoad;
class ApiMonitor;
class ConsoleControl;
class HostValues;
class TopLevel;
bool EvalRequest(JSContext* cx, JS::HandleObject page, JS::HandleObject request,
                 HostValues& values, Checkpoint checkpoint,
                 JS::MutableHandleValue result, bool* success);
// Every request reads the active top-level document's global from `top`; a
// cross-document navigation between requests moves later ones to the new one.
bool Serve(JSContext* cx, TopLevel& top, JS::HandleObject codec,
           const char* name, Checkpoint checkpoint, ExceptionCapture& errors,
           EnvironmentControl& environment, SchedulingControl& scheduling,
           ResourceControl& resources, ConsoleControl& console, PageLoad& pages,
           ApiMonitor& monitor);
bool Reply(JSContext* cx, JS::HandleObject codec, JS::HandleValue id,
           bool success, JS::MutableHandleValue value, Checkpoint checkpoint,
           ExceptionCapture& errors, std::string* frame, bool* delivered = nullptr);
bool Command(JSContext* cx, JS::HandleObject page, JS::HandleObject codec,
             const char* source, size_t length, const char* filename,
             Checkpoint checkpoint, ExceptionCapture& errors);
bool File(JSContext* cx, JS::HandleObject page, JS::HandleObject codec,
          const char* path, Checkpoint checkpoint, ExceptionCapture& errors);
}
#endif
