#ifndef RUYIJS_EXCEPTION_CAPTURE_H
#define RUYIJS_EXCEPTION_CAPTURE_H

#include <array>
#include <string>
#include "js/Promise.h"
#include "js/RootingAPI.h"

namespace ruyijs {
struct ExceptionReport {
  std::string name = "Error";
  std::string message = "JavaScript exception";
  std::string stack;
  std::u16string messageUnits;
  bool hasMessageUnits = false;
  std::u16string nameUnits;
  bool hasNameUnits = false;
};

using NativeExceptionReader = bool (*)(JS::HandleObject, JS::MutableHandleString,
                                      JS::MutableHandleString);
using ExceptionSink = bool (*)(void*, JSContext*, JS::HandleValue, JS::HandleObject);
bool ReadNativeException(JSContext* cx, JS::HandleObject object,
                         JS::MutableHandleString name, JS::MutableHandleString message);

ExceptionReport DescribeException(JSContext* cx, JS::HandleValue value,
                                   JS::HandleObject stack);
bool CreateExceptionValue(JSContext* cx, const ExceptionReport& report,
                          JS::MutableHandleValue value);

class ExceptionCapture {
 public:
  explicit ExceptionCapture(JSContext* cx);
  ~ExceptionCapture();
  ExceptionCapture(const ExceptionCapture&) = delete;
  ExceptionCapture& operator=(const ExceptionCapture&) = delete;
  void BeginRequest();
  void CapturePending();
  bool Finish(JS::MutableHandleValue value, bool* success);
  void TrackRejection(JS::HandleObject promise,
                      JS::PromiseRejectionHandlingState state);
  JSContext* context() const { return cx_; }
  void SetNativeExceptionReader(NativeExceptionReader reader) { nativeReader_ = reader; }
  NativeExceptionReader nativeReader() const { return nativeReader_; }
  void SetSink(ExceptionSink sink, void* context) { sink_ = sink; sinkContext_ = context; }

 private:
  static constexpr size_t kLimit = 64;
  JSContext* cx_;
  ExceptionCapture* previous_;
  std::array<JS::PersistentRootedObject, kLimit> rejections_;
  size_t rejectionCount_ = 0;
  size_t exceptionCount_ = 0;
  bool failed_ = false;
  ExceptionReport report_;
  NativeExceptionReader nativeReader_ = nullptr;
  ExceptionSink sink_ = nullptr;
  void* sinkContext_ = nullptr;
  void Record(JS::HandleValue value, JS::HandleObject stack, bool notify = false);
  void Overflow(const char* message);
};

bool CaptureOperationFailure(JSContext* cx);
bool FormatOperationFailure(JSContext* cx, JS::MutableHandleValue value);
bool CaptureShellException(JSContext* cx);
bool CapturePromiseRejection(JSContext* cx, JS::HandleObject promise,
                             JS::PromiseRejectionHandlingState state);
}
#endif
