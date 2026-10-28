#include "runtime/resource_timing_ports.h"
#include "runtime/browsing_context.h"
#include "resource_network/page_http.h"
#include "execution_scheduling/realm.h"
#include "execution_scheduling/resource_timing.h"
#include "domain/browser_environment/url_value.h"
#include <algorithm>
#include <cstring>
namespace ruyijs {
namespace {
using resource_network::HeaderPairs;
using UrlValue=ruyijs::domain::browser_environment::UrlValue;
std::string Header(const HeaderPairs& headers,const char* name){
 std::string result;
 for(const auto& field:headers)if(ruyijs::domain::resource_network::lower_header_name(field.first)==name){
  if(!result.empty())result+=",";result+=field.second;
 }
 return result;
}
bool Record(void* opaque,const resource_network::HttpObservation& observation){
 auto& target=*static_cast<BrowsingContext*>(opaque);
 const auto& hops=observation.hops;const auto& result=observation.result;const auto& options=observation.options;
 if(hops.empty()||options.navigation)return true;
 const std::string origin=UrlValue(observation.context.source).origin();
 ruyijs::domain::execution_scheduling::ResourceTimingAccess access;
 for(const auto& hop:hops){
  bool same=origin==UrlValue(hop.url).origin();
  access.Hop(same,origin,Header(hop.headers,"timing-allow-origin"),
    Header(hop.headers,"access-control-allow-origin"),Header(hop.headers,"access-control-allow-credentials"),
    options.cors,options.credentials==resource_network::Credentials::Include);
 }
 execution_scheduling::ResourceTimingRecord record;
 UrlValue name(observation.url);name.set_hash("");record.name=name.href();record.initiator=options.initiator;
 record.start=hops.front().start;record.responseEnd=std::max(record.start,hops.back().end);
 record.fetchStart=hops.back().start;record.protocol=result.protocol;
 if(!target.scheduling->Inspect().logical) {
  record.requestStart=hops.back().requestStart;
  record.responseStart=hops.back().responseStart;
  record.finalResponseHeadersStart=hops.back().headersStart;
 }
 if(hops.size()>1){record.redirectStart=record.start;record.redirectEnd=hops[hops.size()-2].end;}
 record.encodedBodySize=result.encodedBytes;record.decodedBodySize=result.decodedBytes;
 record.responseStatus=uint16_t(result.status);
 record.contentType=Header(result.headers,"content-type");
 auto end=record.contentType.find(';');if(end!=std::string::npos)record.contentType.resize(end);
 access.Apply(record,result.transferred,result.opaqueRedirect);
 // DNS/connect/TLS remain unmeasured; request/response values above come from
 // the original transport and must never be copied from fetchStart.
 // Firefox exposes a child document's navigation request as a resource entry
 // on the embedding document. Keep the child Performance timeline local for
 // subresources, but route iframe navigation observations to its parent.
 BrowsingContext* owner=&target;
 if(options.initiator&&std::strcmp(options.initiator,"iframe")==0&&target.parent)
  owner=target.parent;
 return owner->scheduling->RecordResource(owner->page,record);
}
double Clock(void* opaque){return static_cast<execution_scheduling::Realm*>(opaque)->Inspect().now;}
}
void BindResourceTiming(BrowsingContext& target,resource_network::Realm& network,
                        execution_scheduling::Realm& scheduling){
 network.Resources().SetHttpObservation(Record,&target,Clock,&scheduling);
}
}
