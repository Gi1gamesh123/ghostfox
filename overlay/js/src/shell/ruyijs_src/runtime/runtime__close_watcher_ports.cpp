// Narrow original-owner capabilities; no cross-domain implementation sharing.
#include "runtime/close_watcher_ports.h"
#include "runtime/browsing_context.h"

namespace ruyijs {
namespace {
bool Current(void* context) {
  auto& owner=*static_cast<BrowsingContext*>(context);
  return owner.frames && owner.frames->FindCurrentDocument(*owner.document)==&owner;
}
bool History(void* context) {
  auto& owner=*static_cast<BrowsingContext*>(context);
  return Current(context) && owner.environment->HasHistoryActivation();
}
void Consume(void* context) {
  auto& owner=*static_cast<BrowsingContext*>(context);
  if (owner.frames) owner.frames->ConsumeHistoryActivation(owner);
}
bool Input(void* context,JS::HandleObject target) {
  auto& owner=*static_cast<BrowsingContext*>(context);
  auto* document=owner.document->InputDocument(target);
  if (!document || !owner.frames) return true;
  auto* current=owner.frames->FindCurrentDocument(*document);
  return !current || current->interaction->ProcessCloseWatchers();
}
}
void BindCloseWatcher(BrowsingContext& owner) {
  interaction::CloseWatcherPorts ports;
  ports.active=Current;ports.history=History;ports.consume=Consume;
  ports.input=Input;
  owner.interaction->SetCloseWatcherPorts(ports,&owner);
}
void UnbindCloseWatcher(BrowsingContext& owner) {
  owner.interaction->SetCloseWatcherPorts({},nullptr);
}
}
