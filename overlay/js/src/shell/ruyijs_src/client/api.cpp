#include "ruyijs.h"
#include "context.h"
#include <cstdlib>
#include <cstring>
#include <new>
#include <memory>

static char* Copy(const std::string& text) {
  auto* buffer = static_cast<char*>(malloc(text.size() + 1));
  if (buffer) memcpy(buffer, text.c_str(), text.size() + 1);
  return buffer;
}

extern "C" ruyijs_context* ruyijs_create(const char* shell, uint32_t timeout_ms,
                                         char** error) {
  return ruyijs_create_mode(shell, timeout_ms, "default", error);
}

extern "C" ruyijs_context* ruyijs_create_mode(const char* shell, uint32_t timeout_ms,
                                             const char* mode, char** error) {
  if (error) *error = nullptr;
  try {
    if (!shell || !*shell || !timeout_ms) {
      if (error) *error = Copy("Expected executable path and positive timeout");
      return nullptr;
    }
    std::unique_ptr<ruyijs_context> context(new ruyijs_context);
    context->timeout = timeout_ms;
    std::string message;
    if (!context->Connect(shell, mode, &message)) {
      if (error) *error = Copy(message);
      return nullptr;
    }
    return context.release();
  } catch (...) {
    return nullptr;
  }
}

extern "C" int ruyijs_call(ruyijs_context* context, const char* request,
                            uint32_t length, char** reply, uint32_t* reply_length) {
  if (reply) *reply = nullptr;
  if (reply_length) *reply_length = 0;
  if (!context || !request || !length || length > kRuyijsMaxFrame ||
      !reply || !reply_length) return -1;
  if (context->callback_active) return -2;
  try {
    std::lock_guard<std::mutex> guard(context->mutex);
    if (!context->active) return -1;
    std::string response;
    if (!context->Exchange(request, length, &response)) {
      context->Stop();
      return -1;
    }
    *reply = Copy(response);
    if (!*reply) { context->Stop(); return -1; }
    *reply_length = static_cast<uint32_t>(response.size());
    return 0;
  } catch (...) {
    context->Stop();
    return -1;
  }
}

extern "C" void ruyijs_free(void* buffer) { free(buffer); }

extern "C" void* ruyijs_alloc(uint32_t length) {
  return length && length <= kRuyijsMaxFrame ? malloc(length) : nullptr;
}

extern "C" int ruyijs_set_callback(ruyijs_context* context,
                                    ruyijs_callback callback, void* user) {
  if (!context || context->callback_active) return -2;
  try {
    std::lock_guard<std::mutex> guard(context->mutex);
    if (!context->active) return -1;
    context->callback = callback;
    context->callback_user = user;
    return 0;
  } catch (...) { return -1; }
}

extern "C" void ruyijs_close(ruyijs_context* context) {
  if (!context) return;
  if (context->callback_active) return;
  try {
    std::lock_guard<std::mutex> guard(context->mutex);
    if (context->active) {
      constexpr char close[] = "{\"id\":0,\"op\":\"close\"}";
      std::string response;
      if (context->Exchange(close, sizeof(close) - 1, &response))
        WaitForSingleObject(context->process, context->timeout);
      context->Stop();
    }
  } catch (...) {
    context->Stop();
  }
  delete context;
}
