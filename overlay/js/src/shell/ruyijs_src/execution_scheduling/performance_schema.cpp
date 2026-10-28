#include "execution_scheduling/performance.h"
#include <iterator>
namespace ruyijs::execution_scheduling {
namespace {
const char* const base[]={"name","entryType","startTime","duration"};
const char* const detail[]={"detail"};
const char* const resource[]={"initiatorType","nextHopProtocol","workerStart","redirectStart",
 "redirectEnd","fetchStart","domainLookupStart","domainLookupEnd","connectStart","connectEnd",
 "secureConnectionStart","requestStart","finalResponseHeadersStart","firstInterimResponseStart",
 "responseStart","responseEnd","transferSize","encodedBodySize","decodedBodySize","responseStatus",
 "contentType","serverTiming","deliveryType"};
const char* const navigation[]={"unloadEventStart","unloadEventEnd","domInteractive",
 "domContentLoadedEventStart","domContentLoadedEventEnd","domComplete","loadEventStart",
 "loadEventEnd","type","redirectCount"};
const char* const paint[]={"paintTime","presentationTime"};
const char* const event[]={"processingStart","processingEnd","cancelable","target","interactionId"};
const char* const largest[]={"renderTime","loadTime","size","id","url","element","paintTime","presentationTime"};
const char* const server[]={"name","duration","description"};
const char* const legacyNav[]={"type","redirectCount"};
}
const char* const kLegacyTimingFields[]={"navigationStart","unloadEventStart","unloadEventEnd",
 "redirectStart","redirectEnd","fetchStart","domainLookupStart","domainLookupEnd","connectStart",
 "connectEnd","secureConnectionStart","requestStart","responseStart","responseEnd","domLoading",
 "domInteractive","domContentLoadedEventStart","domContentLoadedEventEnd","domComplete","loadEventStart","loadEventEnd"};
#define ROW(name,parent,fields,json,worker) {name,parent,fields,unsigned(std::size(fields)),json,worker}
const PerfSchema kPerfSchema[]={
 ROW("PerformanceEntry",-1,base,true,true),ROW("PerformanceMark",Entry,detail,false,true),
 ROW("PerformanceMeasure",Entry,detail,false,true),ROW("PerformanceResourceTiming",Entry,resource,true,true),
 ROW("PerformanceNavigationTiming",Resource,navigation,true,false),ROW("PerformancePaintTiming",Entry,paint,true,false),
 ROW("PerformanceEventTiming",Entry,event,true,false),ROW("LargestContentfulPaint",Entry,largest,true,false),
 ROW("PerformanceServerTiming",-1,server,true,true),ROW("PerformanceTiming",-1,kLegacyTimingFields,true,false),
 ROW("PerformanceNavigation",-1,legacyNav,true,false)};
#undef ROW
}
