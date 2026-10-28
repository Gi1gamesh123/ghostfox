// Record queueing and delivery. The Document Model sink fires inside the very
// change that produced the notice, after original node identities are published.
// Matching freezes options and queues original references; delivery uses the
// same microtask and record factory as takeRecords().
#include "web_observers/internal.h"
#include "web_observers/realm.h"
#include "document_model/realm.h"
#include "runtime/exception_capture.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Exception.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "jsfriendapi.h"
#include <algorithm>
#include <utility>

namespace ruyijs::web_observers {
void SinkThunk(void* context) { static_cast<State*>(context)->Sink(); }

bool State::Queue(JS::HandleObject callback) {
  if (!microtask) return false;
  return microtask(microtaskContext, callback);
}

bool State::QueueDelivery() {
  if (mutationGroup->deliveryQueued) return true;
  if (!deliver) return false;
  JS::RootedObject callback(cx, deliver);
  if (!Queue(callback)) return false;
  mutationGroup->deliveryQueued = true;
  return true;
}

void State::Sink() {
  std::vector<MutationNotice> notices;
  if (!document->MutationsSince(cursor, &notices)) {
    JS_ClearPendingException(cx);
    return;
  }
  cursor = document->MutationSequence();
  for (Observer* observer = mutationGroup->first; observer; observer = observer->nextInGroup) {
    const size_t before = observer->pending.get().length();
    for (const MutationNotice& notice : notices) {
      bool matched = false, wants_old_value = false;
      if (!Matches(*observer, notice, &matched, &wants_old_value)) {
        JS_ClearPendingException(cx);
        continue;
      }
      if (matched && !Append(*observer, notice, wants_old_value)) {
        CaptureOperationFailure(cx);
        JS_ClearPendingException(cx);
      }
      if (!AddTransients(*observer, notice)) JS_ClearPendingException(cx);
    }
    if (observer->pending.get().length() == before || observer->scheduled) continue;
    // Queueing allocates. A failure here is not the mutation's failure, so the
    // records stay pending and the next sink call retries the enqueue.
    JS::AutoSaveExceptionState saved(cx);
    if (QueueDelivery()) observer->scheduled = true;
    saved.restore();
  }
}

bool State::Matches(Observer& observer, const MutationNotice& notice,
                    bool* matched, bool* wants_old_value) {
  *matched = false;
  *wants_old_value = false;
  for (const Registration& entry : observer.registrations) {
    if (entry.source != this) continue;
    if (notice.sequence <= entry.baseline) continue;
    bool observed = entry.target.document == notice.target.document &&
                    entry.target.node == notice.target.node;
    if (!observed && entry.subtree) {
      bool ancestor = false;
      if (!document->AncestorOrSelf(entry.target, notice.target, &ancestor)) return false;
      observed = ancestor;
    }
    if (!observed) continue;
    if (notice.kind == MutationKind::Attributes) {
      if (!entry.attributes) continue;
      if (!entry.all_attributes &&
          std::find(entry.filter.begin(), entry.filter.end(), notice.attribute_name) ==
              entry.filter.end()) continue;
      *wants_old_value = *wants_old_value || entry.attribute_old_value;
    } else if (notice.kind == MutationKind::CharacterData) {
      if (!entry.character_data) continue;
      *wants_old_value = *wants_old_value || entry.character_data_old_value;
    } else if (!entry.child_list) {
      continue;
    }
    *matched = true;
  }
  return true;
}

void State::DeliverAll() {
  mutationGroup->deliveryQueued = false;
  // Freeze the notified observers, while their original record queues stay
  // live so earlier callbacks can add records to a later observer's batch.
  std::vector<Observer*> scheduled;
  for (Observer* observer = mutationGroup->first; observer; observer = observer->nextInGroup)
    if (observer->scheduled) scheduled.push_back(observer);
  for (Observer* observer : scheduled) {
    if (!observer->scheduled) continue;
    observer->scheduled = false;
    // DOM "notify mutation observers": this observer's transient
    // registrations end at its delivery, before its callback runs.
    observer->owner->RemoveTransients(*observer, 0);
    if (observer->pending.get().empty()) continue;
    State& owner = *observer->owner;
    JSAutoRealm realm(cx, owner.page);
    JS::Rooted<PendingRecords> batch(cx, PendingRecords());
    batch.get().swap(observer->pending.get());
    JS::RootedObject records(cx);
    if (!owner.BuildBatch(batch.get(), &records)) {
      CaptureOperationFailure(cx);
      JS_ClearPendingException(cx);
      continue;
    }
    JS::RootedObject self(cx, observer->object);
    JS::RootedValue callback(cx, JS::ObjectValue(*observer->callback));
    JS::RootedValueArray<2> call(cx);
    call[0].setObject(*records);
    call[1].setObject(*self);
    JS::RootedValue ignored(cx);
    if (!JS::Call(cx, JS::UndefinedHandleValue, callback, call, &ignored)) {
      // A throwing callback is reported like any other uncaught microtask
      // exception; it never stops the Context or the other observers.
      CaptureOperationFailure(cx);
      JS_ClearPendingException(cx);
    }
  }
}

bool Deliver(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  args.rval().setUndefined();
  if (state) state->DeliverAll();
  return true;
}
}  // namespace ruyijs::web_observers
