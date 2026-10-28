// Document stream members, installed by the existing Document owner.
// write/writeln feed the active document parser at its script-created
// insertion point (the parser is paused at the script that is running, or
// document.open() started a script-created parser); with no insertion point
// they stay inert (recorded in DOCUMENT_MODEL owner). close() ends the input of
// a parser open() started.
#include "document_model/state.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
enum Method { Write, Writeln, Open, Close, HasFocus, ProcessingInstruction };  // last: node factory
constexpr const char* names[] = {
    "write", "writeln", "open", "close", "hasFocus", "createProcessingInstruction"};

bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  const auto method = static_cast<Method>(FunctionSlot(args));
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  auto* entry = DocumentReceiver(cx, object, &args);
  if (!entry) return false;
  state.Observe("Document", names[method], int(ApiAccess::Call));
  if (method == ProcessingInstruction) {
    if (argc < 2)
      return TypeError(cx, (std::string("Document.createProcessingInstruction: At least 2 arguments required, but only ") +
                            std::to_string(argc) + " passed").c_str());
    DomString target, data;
    if (!String(cx, args[0], &target) || !String(cx, args[1], &data)) return false;
    State& owner = entry->owner ? *entry->owner : state;  // The node belongs to the document's realm.
    Draft draft;
    return owner.Check(owner.model.Create(entry->key, {NodeKind::ProcessingInstruction, target, data}, &draft)) &&
           owner.Commit(draft, args.rval());
  }

  const unsigned count = method == Close || method == HasFocus ? 0 : argc;
  DomString text;
  for (unsigned i = 0; i < count; ++i) {
    JS::RootedString converted(cx, JS::ToString(cx, args[i]));
    if (converted && (method == Write || method == Writeln)) {
      JS::RootedValue value(cx, JS::StringValue(converted));
      DomString part;
      if (!String(cx, value, &part)) return false;
      text += part;
      continue;
    }
    if (!converted) {
      // Public write/writeln discard each failed conversion and continue.
      // Never swallow an engine termination or an allocation failure.
      if ((method != Write && method != Writeln) ||
          JS_IsThrowingOutOfMemory(cx) || !JS_IsExceptionPending(cx)) return false;
      JS_ClearPendingException(cx);
    }
  }
  if (method == Write || method == Writeln) {
    if (method == Writeln) text += u'\n';
    bool written = false;
    State& owner = entry->owner ? *entry->owner : state;  // The document's own parser.
    if (!owner.Check(owner.model.ParserWrite(entry->key, text, &written))) return false;
    args.rval().setUndefined();
    return true;
  }
  State& owner = entry->owner ? *entry->owner : state;  // The document's own parser.
  if (method == Open) {
    bool opened = false;
    if (!owner.Check(owner.model.OpenParse(entry->key, &opened))) return false;
    args.rval().set(args.thisv());
  } else if (method == Close) {
    if (!owner.Check(owner.model.CloseParse(entry->key))) return false;
    args.rval().setUndefined();
  } else if (method == HasFocus) {
    args.rval().setBoolean(true);
  } else {
    args.rval().setUndefined();
  }
  return true;
}
}  // namespace

bool InstallDocumentStream(State& state) {
  for (int method = Write; method <= ProcessingInstruction; ++method)
    if (!state.Method("Document", names[method], Call,
                      method == ProcessingInstruction ? 2 : 0, method)) return false;
  return true;
}
}  // namespace ruyijs::document_model
