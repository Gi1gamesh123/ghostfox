#ifndef RUYIJS_CLIENT_H
#define RUYIJS_CLIENT_H
#include <stdint.h>
#ifdef _WIN32
#define RUYIJS_API __declspec(dllexport)
#else
#define RUYIJS_API
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct ruyijs_context ruyijs_context;
typedef int (*ruyijs_callback)(const char* event, uint32_t length, char** response,
                              uint32_t* response_length, void* user);
/* UTF-8 executable path; timeout bounds connect and transport waits.
 * A raw C host callback runs synchronously and must arrange its own deadline;
 * an overdue callback causes transport failure when it returns.
 * error/reply are DLL-owned allocations: release them with ruyijs_free.
 * Caller must not close a context while another thread accesses its pointer. */
RUYIJS_API ruyijs_context* ruyijs_create(const char* shell, uint32_t timeout_ms,
                                      char** error);
/* Same contract as ruyijs_create with an explicit context mode: "default", or
 * "debug" to enable the API monitor read through {op:"api_log"}. Any other mode
 * fails. ruyijs_create is exactly ruyijs_create_mode with "default". */
RUYIJS_API ruyijs_context* ruyijs_create_mode(const char* shell, uint32_t timeout_ms,
                                      const char* mode, char** error);
/* Pass a full JSON request; the reply is the full JSON envelope. Return 0 on
 * transport success (including JS errors in the envelope), -1 on local failure.
 * Transport failure permanently invalidates this context. */
RUYIJS_API int ruyijs_call(ruyijs_context* context, const char* request,
                          uint32_t length, char** reply, uint32_t* reply_length);
/* The callback returns a complete callback-result JSON frame allocated with
 * ruyijs_alloc. The DLL frees it after copying. Same-context call during a
 * callback returns -2. callback code must not close its own context. */
RUYIJS_API int ruyijs_set_callback(ruyijs_context* context, ruyijs_callback callback,
                                 void* user);
RUYIJS_API void* ruyijs_alloc(uint32_t length);
RUYIJS_API void ruyijs_free(void* buffer);
RUYIJS_API void ruyijs_close(ruyijs_context* context);
#ifdef __cplusplus
}
#endif
#endif
