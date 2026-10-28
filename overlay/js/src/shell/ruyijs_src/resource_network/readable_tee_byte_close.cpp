// BYOB EOF restores the original detached branch buffer before completing it.
#include "resource_network/readable_tee_byte.h"
#include "resource_network/readable_byte.h"
#include <initializer_list>

namespace ruyijs::resource_network {
bool TeeByteClose(JSContext* cx,JS::HandleObject operation,JS::HandleValue chunk,int byob) {
  const unsigned first=byob<0?0:unsigned(byob),second=1-first;
  for (unsigned i:{first,second}) {
    JS::RootedObject branch(cx,SlotObject(operation,i?TBranch2:TBranch1));
    JS::RootedObject controller(cx,SlotObject(branch,SController));
    if (!TeeCanceled(operation,i) && ReadableStatus(branch)==StreamStatus::Readable &&
        !(DefaultFlags(controller)&DCloseRequested)) {
      ByteCloseOutcome outcome;
      if (!ByteClose(cx,controller,ByteCloseKind::Intrinsic,&outcome))
        return outcome==ByteCloseOutcome::IncompleteElement?false:TeeOperationFailure(cx,operation);
    }
  }
  if (byob<0 || !chunk.isUndefined()) {
    for (unsigned i:{first,second}) {
      JS::RootedObject branch(cx,SlotObject(operation,i?TBranch2:TBranch1));
      JS::RootedObject controller(cx,SlotObject(branch,SController));
      if (TeeCanceled(operation,i) || ReadableStatus(branch)!=StreamStatus::Closed ||
          !SlotObject(controller,BPendingHead)) continue;
      if (byob>=0 && i==unsigned(byob)) {
        if (!ByteRespondNewView(cx,controller,chunk)) return TeeOperationFailure(cx,operation);
      } else if (!ByteRespond(cx,controller,0)) return TeeOperationFailure(cx,operation);
    }
  }
  return TeeFinishCancel(cx,operation) || TeeOperationFailure(cx,operation);
}
}
