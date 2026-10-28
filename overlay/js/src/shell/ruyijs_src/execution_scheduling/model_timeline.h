#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace ruyijs::execution_scheduling {
struct Timeline;
Timeline* CreateTimeline() noexcept;
void DeleteTimeline(Timeline*) noexcept;
bool InsertTimeline(Timeline*,const std::u16string&,int,double,double,uint32_t*,bool*) noexcept;
bool SelectTimeline(Timeline*,const std::u16string*,int,bool,std::vector<uint32_t>*) noexcept;
bool ResolveTimeline(Timeline*,const std::u16string&,double*) noexcept;
void SetResourceLimit(Timeline*,uint64_t) noexcept;
bool ResourceRoom(Timeline*) noexcept;
size_t PendingResources(Timeline*) noexcept;
void PromoteResources(Timeline*) noexcept;
bool DropPendingResources(Timeline*,std::vector<uint32_t>*) noexcept;
void FinishResources(Timeline*) noexcept;
}
