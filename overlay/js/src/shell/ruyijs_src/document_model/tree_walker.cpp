#include "document_model/traversal.h"

#include "document_model/state.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::document_model {
namespace {
enum Field { Root, WhatToShow, Filter, Current };
enum Method { ParentNode, FirstChild, LastChild, PreviousSibling, NextSibling,
              PreviousNode, NextNode };

bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  Walker* walker = WalkerOf(state, args);
  if (!walker) return false;
  switch (FunctionSlot(args)) {
    case Root: return state.Wrap(walker->root, args.rval());
    case WhatToShow: args.rval().setNumber(double(walker->what)); return true;
    case Filter: args.rval().setObjectOrNull(walker->filter.get()); return true;
    case Current: return state.Wrap(walker->reference, args.rval());
  }
  return false;
}

bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  Walker* walker = WalkerOf(state, args);
  if (!walker) return false;
  NodeKey node;
  if (!NodeArgument(state, args.get(0), &node)) return false;
  walker->reference = node;
  args.rval().setUndefined();
  return true;
}

bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  Walker* walker = WalkerOf(state, args);
  if (!walker) return false;
  switch (FunctionSlot(args)) {
    case ParentNode: return WalkerAncestor(state, *walker, args.rval());
    case FirstChild: return WalkerDescend(state, *walker, true, args.rval());
    case LastChild: return WalkerDescend(state, *walker, false, args.rval());
    case PreviousSibling: return WalkerAdjacent(state, *walker, false, args.rval());
    case NextSibling: return WalkerAdjacent(state, *walker, true, args.rval());
    case PreviousNode: return WalkerBackward(state, *walker, args.rval());
    case NextNode: return WalkerForward(state, *walker, args.rval());
  }
  return false;
}
}  // namespace

bool InstallTreeWalker(State& state) {
  struct Property { const char* name; Field field; bool writable; };
  static const Property properties[] = {
    {"root", Root, false}, {"whatToShow", WhatToShow, false},
    {"filter", Filter, false}, {"currentNode", Current, true}
  };
  for (const Property& property : properties)
    if (!state.Accessor("TreeWalker", property.name, Get, property.field,
                        property.writable ? Set : nullptr)) return false;
  struct Operation { const char* name; Method method; };
  static const Operation methods[] = {
    {"parentNode", ParentNode}, {"firstChild", FirstChild}, {"lastChild", LastChild},
    {"previousSibling", PreviousSibling}, {"nextSibling", NextSibling},
    {"previousNode", PreviousNode}, {"nextNode", NextNode}
  };
  for (const Operation& method : methods)
    if (!state.Method("TreeWalker", method.name, Call, 0, method.method)) return false;
  return true;
}
}  // namespace ruyijs::document_model
