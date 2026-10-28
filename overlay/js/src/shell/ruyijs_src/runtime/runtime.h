#ifndef RUYIJS_RUNTIME_H
#define RUYIJS_RUNTIME_H

#include <string>
#include "domain/runtime_context/eval_options.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace js::cli { class OptionParser; }

namespace ruyijs {
using EvalOptions = ruyijs::domain::runtime_context::EvalOptions;
using Checkpoint = void (*)(JSContext*);
bool Run(JSContext* cx, js::cli::OptionParser* options, Checkpoint checkpoint);
// Worker process mode: the page Context spawned this process and connected
// the named pipe; it installs the restricted worker realm and serves the pipe.
bool RunWorker(JSContext* cx, js::cli::OptionParser* options, Checkpoint checkpoint);
// A same-origin iframe realm shares the compartment of sameOrigin.
JSObject* CreatePage(JSContext* cx, JS::HandleObject sameOrigin = nullptr);
// `host` marks code the embedder supplied (RPC eval, CLI); page scripts pass false.
bool Evaluate(JSContext* cx, JS::HandleObject page, JS::HandleString code,
              const char* filename, Checkpoint checkpoint,
              JS::MutableHandleValue value, bool* success, const EvalOptions& options = {},
              const char* introduction = nullptr, bool host = false);
// Compiles and runs a host target expression in the current realm, marked as host code.
bool EvaluateHostExpression(JSContext* cx, const char* filename, const char16_t* chars, size_t length,
                            JS::MutableHandleValue value);
bool ToUTF8(JSContext* cx, JS::HandleString input, std::string* output);
bool InitCodec(JSContext* cx, JS::MutableHandleObject codec);
bool ParseRequest(JSContext* cx, const std::string& input,
                  JS::MutableHandleObject request);
bool StringProperty(JSContext* cx, JS::HandleObject object, const char* key,
                    std::string* output, const char* fallback = nullptr);
bool EncodeReply(JSContext* cx, JS::HandleObject codec, JS::HandleValue id,
                 bool success, JS::HandleValue value, std::string* output, bool* delivered = nullptr);
}
#endif
