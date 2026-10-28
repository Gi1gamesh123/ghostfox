#include "runtime/styling_tasks.h"
namespace ruyijs {
void BindStylingTasks(web_styling::Realm& styling, execution_scheduling::Realm& scheduling) {
  styling.SetTaskPort([](void* owner, JS::HandleObject callback) {
    return static_cast<execution_scheduling::Realm*>(owner)->PostTask(callback);
  }, &scheduling);
}
}
