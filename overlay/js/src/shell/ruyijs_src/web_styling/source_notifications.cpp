// Immediate retirement and migration reuse the original factories and GC slots.
#include "web_styling/realm.h"
#include "web_styling/adopted.h"
#include "js/Realm.h"
#include <algorithm>

namespace ruyijs::web_styling {
bool Realm::NotifyStyle(JS::HandleObject value, document_model::StyleNotifyAction action) {
  JSContext* cx = state_->cx;
  JS::RootedObject object(cx, js::CheckedUnwrapStatic(value));
  if (!object) return true;
  if (action == document_model::StyleNotifyAction::MigrateAdopted) {
    if (JS::GetClass(object) != &kAdoptedClass) return true;
    State& state = AdoptedOwnerOf(object);
    JSAutoRealm realm(cx, state.page);
    JS::RootedObject rows(cx);
    return AdoptedBacking(cx, object, &rows);
  }
  // Document/Shadow StyleSheetList shares the original DOM association slot.
  if (JS::GetClass(object) != &kSheetClass) return true;
  State& state = SheetOwnerOf(object);
  JSAutoRealm realm(cx, state.page);
  auto cache = *static_cast<std::shared_ptr<SheetCache>*>(
      JS::GetReservedSlot(object, SheetData).toPrivate());
  if (cache->constructed || cache->retired) return true;
  if (!RetireSheet(state, cache)) return false;
  state.sheets.erase(std::remove(state.sheets.begin(), state.sheets.end(), cache), state.sheets.end());
  return true;
}
}
