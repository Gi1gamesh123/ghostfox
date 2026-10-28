#pragma once
#include <cstdint>
#include <string>
#include <string_view>
namespace ruyijs::domain::execution_scheduling {
struct ResourceTimingRecord {
  std::string name,initiator,protocol,contentType,deliveryType;
  double start=0,workerStart=0,redirectStart=0,redirectEnd=0,fetchStart=0;
  double domainLookupStart=0,domainLookupEnd=0,connectStart=0,connectEnd=0;
  double secureConnectionStart=0,requestStart=0,finalResponseHeadersStart=0;
  double firstInterimResponseStart=0,responseStart=0,responseEnd=0;
  uint64_t transferSize=0,encodedBodySize=0,decodedBodySize=0;
  uint16_t responseStatus=0;
};
inline std::string_view ResourceTrim(std::string_view text){
  auto first=text.find_first_not_of(" \t"),last=text.find_last_not_of(" \t");
  return first==std::string_view::npos?std::string_view{}:text.substr(first,last-first+1);
}
struct ResourceTimingAccess {
  bool timing=true,body=true;
  static bool TimingOrigin(std::string_view field,std::string_view origin){
    while(!field.empty()){
      auto end=field.find(',');auto token=ResourceTrim(field.substr(0,end));
      if(token=="*"||token==origin)return true;
      if(end==std::string_view::npos)break;field.remove_prefix(end+1);
    }
    return false;
  }
  void Hop(bool same,std::string_view origin,std::string_view tao,std::string_view acao,
           std::string_view acac,bool cors,bool credentials){
    timing=timing&&(same||TimingOrigin(tao,origin));
    body=body&&(same||(cors&&(acao==origin||(!credentials&&acao=="*"))&&(!credentials||acac=="true")));
  }
  void Apply(ResourceTimingRecord& record,bool transferred,bool opaqueRedirect)const{
    bool exposeTiming=timing&&!opaqueRedirect,exposeBody=body&&!opaqueRedirect;
    if(!exposeTiming){
      record.protocol.clear();record.fetchStart=record.start;
      record.redirectStart=record.redirectEnd=0;
      record.domainLookupStart=record.domainLookupEnd=record.connectStart=record.connectEnd=0;
      record.secureConnectionStart=record.requestStart=record.finalResponseHeadersStart=0;
      record.firstInterimResponseStart=record.responseStart=0;
    }
    if(!exposeBody){
      record.encodedBodySize=record.decodedBodySize=record.responseStatus=0;record.contentType.clear();
    }
    record.transferSize=transferred&&exposeTiming?record.encodedBodySize+300:0;
  }
};
}
