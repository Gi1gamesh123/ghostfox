#include "execution_scheduling/model_timeline.h"
#include "domain/execution_scheduling/user_timeline.h"
#include <new>
namespace ruyijs::execution_scheduling {
struct Timeline { ruyijs::domain::execution_scheduling::UserTimeline data; };
Timeline* CreateTimeline() noexcept {return new(std::nothrow) Timeline;}
void DeleteTimeline(Timeline* value) noexcept {delete value;}
bool InsertTimeline(Timeline* value,const std::u16string& name,int kind,
                    double start,double duration,uint32_t* id,bool* schedule) noexcept {
  try{*id=value->data.Insert(name,kind,start,duration,schedule);return true;}catch(...){return false;}
}
bool SelectTimeline(Timeline* value,const std::u16string* name,int kind,bool clear,
                    std::vector<uint32_t>* output) noexcept {
  try{*output=clear?value->data.Clear(name,kind):value->data.Select(name,kind);return true;}
  catch(...){return false;}
}
bool ResolveTimeline(Timeline* value,const std::u16string& name,double* out) noexcept {
  return value->data.Resolve(name,out);
}
void SetResourceLimit(Timeline* value,uint64_t size) noexcept {value->data.ResourceLimit(size);}
bool ResourceRoom(Timeline* value) noexcept {return value->data.ResourceRoom();}
size_t PendingResources(Timeline* value) noexcept {return value->data.PendingResources();}
void PromoteResources(Timeline* value) noexcept {value->data.PromoteResources();}
bool DropPendingResources(Timeline* value,std::vector<uint32_t>* ids) noexcept {
  try{*ids=value->data.DropPendingResources();return true;}catch(...){return false;}
}
void FinishResources(Timeline* value) noexcept {value->data.FinishResources();}
}
