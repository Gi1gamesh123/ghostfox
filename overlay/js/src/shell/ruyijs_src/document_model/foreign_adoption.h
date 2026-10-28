#ifndef RUYIJS_DOCUMENT_FOREIGN_ADOPTION_H
#define RUYIJS_DOCUMENT_FOREIGN_ADOPTION_H
#include "document_model/state.h"

namespace ruyijs::document_model {
// One operation's unpublished native draft and temporary roots. No state is
// retained after the original identities and log have been committed.
class ForeignAdoption final {
 public:
  explicit ForeignAdoption(State& destination)
      : target_(destination), roots_(destination.cx) {}
  ~ForeignAdoption();
  bool Prepare(Wrapper& source, NodeKey document, bool fragment_insertion = false);
  bool Commit();
  NodeKey root() const { return draft_.root; }
 private:
  State& target_;
  State* source_ = nullptr;
  NodeKey original_;
  AdoptionDraft draft_;
  JS::RootedObjectVector roots_;
  bool committed_ = false;
  bool fragmentInsertion_ = false;
};

// Defer only original owner publication while the original wrappers move.
class AdoptionPublication final {
 public:
  explicit AdoptionPublication(State& state)
      : state_(state), previous_(state.model.SetIdentityTransfer(true)) {}
  ~AdoptionPublication() { state_.model.SetIdentityTransfer(previous_); }
 private:
  State& state_;
  bool previous_;
};
bool AdoptForeign(Wrapper& document, Wrapper& source, JS::MutableHandleValue);
bool InsertForeign(Wrapper& parent, Wrapper& source, NodeKey before, NodeKey replaced);
bool ConvertNodeItems(JSContext*, const JS::CallArgs&, std::vector<Item>*, std::vector<Wrapper*>*);
bool InsertNodeItems(Wrapper&, InsertMode, const JS::CallArgs&);
bool ReplaceNodeChildren(Wrapper&, const JS::CallArgs&);
}  // namespace ruyijs::document_model
#endif
