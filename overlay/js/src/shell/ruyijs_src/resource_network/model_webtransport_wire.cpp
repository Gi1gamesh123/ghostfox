#include "resource_network/model_webtransport_wire.h"

#ifdef _WIN32
#include "runtime/ghostfox_port.h"
namespace ruyijs { using ::GetTickCount64; }
#else
#include "runtime/ghostfox_port.h"
namespace ruyijs {
inline Ticks GetTickCount64() { return TicksNow(); }
using ULONGLONG = unsigned long long;
}
#include <fcntl.h>
#include <unistd.h>
#endif


namespace ruyijs::resource_network::webtransport_wire {
Writer Begin(const WebTransportOptions& options) {
  Writer out;out.Number(uint32_t(1));out.Text(options.url);out.Text(options.origin);
  out.Text(options.congestion);out.Number(uint32_t(options.allowPooling));out.Number(uint32_t(options.requireUnreliable));
  out.Number(uint32_t(options.hashes.size()));
  for(const auto& hash:options.hashes){out.Text(hash.algorithm);out.Text(hash.bytes);}
  out.Number(uint32_t(options.protocols.size()));for(const auto& value:options.protocols)out.Text(value);
  return out;
}
Writer Command(uint64_t ticket,const WebTransportCommand& value) {
  Writer out;out.Number(uint32_t(2));out.Number(ticket);out.Number(uint32_t(value.action));
  out.Number(value.stream);out.Number(value.group);out.Number(value.order);
  out.Number(uint32_t(value.bidirectional));out.Number(uint32_t(value.hasGroup));
  out.Number(uint32_t(value.hasOrder));out.Number(uint32_t(value.sending));
  out.Number(value.code);out.Number(value.size);
  out.Text(value.data);out.Text(value.label);out.Text(value.context);out.Text(value.reason);return out;
}
Writer Poll(){Writer out;out.Number(uint32_t(3));return out;}
namespace {
WebTransportEvent Event(Reader& input) {
  WebTransportEvent event;const auto kind=input.Number<uint32_t>();
  if(kind>uint32_t(WebTransportEventKind::StopSending))throw std::runtime_error("WebTransport event kind");
  event.kind=WebTransportEventKind(kind);event.ticket=input.Number<uint64_t>();
  event.stream=input.Number<uint64_t>();event.group=input.Number<uint64_t>();event.consumed=input.Number<uint64_t>();
  event.success=input.Number<uint32_t>()!=0;event.bidirectional=input.Number<uint32_t>()!=0;
  event.pending=input.Number<uint32_t>()!=0;event.fin=input.Number<uint32_t>()!=0;event.clean=input.Number<uint32_t>()!=0;
  event.code=input.Number<uint32_t>();event.maxDatagramSize=input.Number<uint32_t>();
  event.data=input.Text();event.reason=input.Text();event.protocol=input.Text();
  const auto count=input.Number<uint32_t>();
  if(count>64)throw std::runtime_error("WebTransport statistics limit");
  for(uint32_t index=0;index<count;++index)event.statistics.push_back({input.Text(),input.Number<double>()});
  return event;
}
}
bool Exchange(NeckoPipe& input,NeckoPipe& output,const Writer& request,std::vector<WebTransportEvent>* events) {
  if(request.bytes.size()>kFrameLimit)return false;
  uint32_t size=uint32_t(request.bytes.size());const auto deadline=GetTickCount64()+15000;
  if(!input.Write(&size,4,deadline)||!input.Write(request.bytes.data(),size,deadline)||
      !output.Read(&size,4,deadline)||size>kFrameLimit)return false;
  std::string bytes(size,'\0');if(!output.Read(bytes.data(),size,deadline))return false;
  Reader response{bytes};const auto count=response.Number<uint32_t>();
  if(count>257)throw std::runtime_error("WebTransport events limit");
  for(uint32_t i=0;i<count;++i)events->push_back(Event(response));
  if(!response.bytes.empty())throw std::runtime_error("Trailing WebTransport frame data");
  return true;
}
}
