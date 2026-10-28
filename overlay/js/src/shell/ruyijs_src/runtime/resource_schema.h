#ifndef RUYIJS_RESOURCE_SCHEMA_H
#define RUYIJS_RESOURCE_SCHEMA_H
#include "resource_network/realm.h"
#include <initializer_list>

namespace ruyijs::resource_wire {
enum class TextKind { UTF8, Latin1, ASCII };
bool Error(JSContext* cx, const char* message, bool range = false);
bool Fields(JSContext* cx, JS::HandleObject object,
            std::initializer_list<const char*> allowed);
bool Object(JSContext* cx, JS::HandleValue value, JS::MutableHandleObject result);
bool Text(JSContext* cx, JS::HandleValue value, std::string* output,
          size_t limit, TextKind kind = TextKind::UTF8);
bool Property(JSContext* cx, JS::HandleObject object, const char* name,
              std::string* output, size_t limit, TextKind kind = TextKind::UTF8);
bool Input(JSContext* cx, JS::HandleObject request, resource_network::ResourceInput* result);
bool Stats(JSContext* cx, const resource_network::ResourceStats& stats, JS::MutableHandleValue result);
bool Snapshot(JSContext* cx, const resource_network::ResourceSnapshot& snapshot,
              JS::MutableHandleValue result);
bool LogEntries(JSContext* cx, const resource_network::NetLogRecords& entries,
                JS::MutableHandleValue result);
}
#endif
