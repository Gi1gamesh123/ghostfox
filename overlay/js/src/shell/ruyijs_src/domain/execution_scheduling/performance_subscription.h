#pragma once
#include <cstdint>
namespace ruyijs::domain::execution_scheduling {
class PerformanceSubscription {
 public:
  bool Configure(bool multiple,uint32_t types) {
    int requested=multiple?2:1;
    if(mode_&&mode_!=requested)return false;
    mode_=requested;
    if(!types)return true;
    types_=multiple?types:types_|types;
    connected_=true;return true;
  }
  void Disconnect(){if(connected_){types_=0;connected_=false;}}
  bool Observes(uint32_t type) const{return connected_&&(types_&type);}
  bool connected() const{return connected_;}
 private:
  int mode_=0;
  uint32_t types_=0;
  bool connected_=false;
};
}
