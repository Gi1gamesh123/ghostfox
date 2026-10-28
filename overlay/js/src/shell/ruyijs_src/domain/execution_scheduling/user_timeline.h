#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
namespace ruyijs::domain::execution_scheduling {
struct UserEntry {
  uint32_t id;
  std::u16string name;
  int kind;  // 0 mark, 1 measure, 2 resource.
  double start, duration;
  bool pending = false;
};
class UserTimeline {
 public:
  uint32_t Insert(std::u16string name, int kind, double start, double duration, bool* schedule) {
    *schedule=false;
    const bool pending=kind==2&&(resourcePending_||!ResourceRoom());
    UserEntry value{next_,std::move(name),kind,start,duration,pending};
    auto pos=std::upper_bound(entries_.begin(),entries_.end(),start,
        [](double time,const UserEntry& row){return time<row.start;});
    entries_.insert(pos,std::move(value));
    if(kind==2){if(pending){*schedule=!resourcePending_;resourcePending_=true;}else ++resourceCount_;}
    return next_++;
  }
  std::vector<uint32_t> Select(const std::u16string* name, int kind) const {
    std::vector<uint32_t> result;
    for(const auto& row:entries_)
      if(!row.pending&&(!name||row.name==*name)&&(kind<0||row.kind==kind))result.push_back(row.id);
    return result;
  }
  std::vector<uint32_t> Clear(const std::u16string* name,int kind) {
    auto ids=Select(name,kind);
    entries_.erase(std::remove_if(entries_.begin(),entries_.end(),[&](const auto& row){
      bool remove=!row.pending&&(!name||row.name==*name)&&(kind<0||row.kind==kind);
      if(remove&&row.kind==2)--resourceCount_;
      return remove;}),entries_.end());
    return ids;
  }
  bool Resolve(const std::u16string& name,double* value) const {
    for(auto i=entries_.rbegin();i!=entries_.rend();++i)
      if(i->kind==0&&i->name==name){*value=i->start;return true;}
    return false;
  }
  void ResourceLimit(uint64_t value){resourceLimit_=value;}
  bool ResourceRoom()const{return resourceCount_<resourceLimit_;}
  size_t PendingResources()const{
    return std::count_if(entries_.begin(),entries_.end(),[](const auto& row){return row.pending;});
  }
  void PromoteResources(){
    for(auto& row:entries_)if(row.pending&&ResourceRoom()){row.pending=false;++resourceCount_;}
  }
  std::vector<uint32_t> DropPendingResources(){
    std::vector<uint32_t> ids;
    for(const auto& row:entries_)if(row.pending)ids.push_back(row.id);
    entries_.erase(std::remove_if(entries_.begin(),entries_.end(),
        [](const auto& row){return row.pending;}),entries_.end());
    return ids;
  }
  void FinishResources(){resourcePending_=false;}
 private:
  uint32_t next_=1;
  uint64_t resourceLimit_=250,resourceCount_=0;
  bool resourcePending_=false;
  std::vector<UserEntry> entries_;
};
}
