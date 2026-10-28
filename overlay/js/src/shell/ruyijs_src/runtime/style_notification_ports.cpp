#include "runtime/style_notification_ports.h"
#include "runtime/browsing_context.h"

namespace ruyijs {
void BindStyleNotifications(BrowsingContext& owner) {
  owner.document->SetStyleNotifyPort([](void* context, JS::HandleObject object,
                                      document_model::StyleNotifyAction action) {
    return static_cast<web_styling::Realm*>(context)->NotifyStyle(object, action);
  }, owner.styling.get());
}
}
