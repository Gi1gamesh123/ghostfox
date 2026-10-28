#include "resource_network/model_webtransport_internal.h"
#include <chrono>

namespace ruyijs::resource_network {
void WebTransportSession::Impl::Fail(const char* message) noexcept {
  std::lock_guard<std::mutex> guard(lock);terminal=failed=true;
  try{error=message;}catch(...){}
}
bool WebTransportSession::Impl::Publish(std::vector<WebTransportEvent>& incoming) {
  size_t bytes=0;for(const auto& event:incoming)bytes+=event.data.size()+event.reason.size()+event.protocol.size()+512;
  std::unique_lock<std::mutex> guard(lock);
  if(bytes>2*1024*1024||incoming.size()>257)throw std::runtime_error("WebTransport completion bound");
  wake.wait(guard,[&]{return stopping||(events.size()+incoming.size()<=512&&eventBytes+bytes<=2*1024*1024);});
  if(stopping)return false;
  for(auto& event:incoming){
    if(event.kind==WebTransportEventKind::Closed||event.kind==WebTransportEventKind::Error)terminal=true;
    events.push_back(std::move(event));
  }
  eventBytes+=bytes;return !terminal;
}
void WebTransportSession::Impl::Run(WebTransportOptions options) noexcept {
  try {
    ruyijs::resource_network::WorkerString python;
    if(stopping)return;
    if(!NeckoWorkerCommand(&python)||!input.Open(true)||!output.Open(false)||
        !helper.Start(python,input,output,L"--webtransport"))throw std::runtime_error("WebTransport helper startup failed");
    input.CloseChild();output.CloseChild();if(stopping){helper.Cancel();return;}
    std::vector<WebTransportEvent> completed;
    if(!webtransport_wire::Exchange(input,output,webtransport_wire::Begin(options),&completed))
      throw std::runtime_error("WebTransport handshake channel failed");
    if(!Publish(completed)){input.CloseParent();helper.Cancel();return;}
    while(!stopping){
      Pending pending{};bool have=false;
      {std::unique_lock<std::mutex> guard(lock);
       wake.wait_for(guard,std::chrono::milliseconds(2),[&]{return stopping||!requests.empty();});
       if(stopping)break;
       if(!requests.empty()){pending=std::move(requests.front());requests.pop_front();have=true;
         const auto& c=pending.command;requestBytes-=c.data.size()+c.label.size()+c.context.size()+c.reason.size();}}
      completed.clear();
      auto request=have?webtransport_wire::Command(pending.ticket,pending.command):webtransport_wire::Poll();
      if(!webtransport_wire::Exchange(input,output,request,&completed))throw std::runtime_error("WebTransport helper channel failed");
      if(!Publish(completed))break;
    }
  }catch(const std::exception& e){if(!stopping)Fail(e.what());}
  catch(...){if(!stopping)Fail("WebTransport transport failure");}
  input.CloseParent();helper.Cancel();
}
}
