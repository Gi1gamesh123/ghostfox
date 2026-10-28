#include "resource_network/model_webtransport_internal.h"
#include <limits>

namespace ruyijs::resource_network {
WebTransportSession::WebTransportSession():impl_(new Impl){}
WebTransportSession::~WebTransportSession(){Stop();}
bool WebTransportSession::Begin(const WebTransportOptions& options,std::string* error) noexcept {
  try {
    auto& state=*impl_;std::lock_guard<std::mutex> guard(state.lock);
    if(state.started){*error="WebTransport session already started";return false;}
    state.started=true;state.worker=std::thread([&state,options]{state.Run(options);});return true;
  }catch(const std::exception&){try{*error="WebTransport worker allocation failed";}catch(...){}return false;}
}
bool WebTransportSession::Submit(const WebTransportCommand& command,uint64_t* ticket,std::string* error) noexcept {
  try {
    auto& state=*impl_;std::lock_guard<std::mutex> guard(state.lock);
    const size_t bytes=command.data.size()+command.label.size()+command.context.size()+command.reason.size();
    if(!state.started||state.stopping||state.terminal){*error="WebTransport session is closed";return false;}
    if(state.requests.size()>=256||bytes>1024*1024||state.requestBytes+bytes>2*1024*1024||
        state.next==std::numeric_limits<uint64_t>::max()){*error="WebTransport command budget exceeded";return false;}
    state.requests.push_back({state.next,command});*ticket=state.next++;
    state.requestBytes+=bytes;state.wake.notify_one();return true;
  }catch(const std::exception&){try{*error="WebTransport command allocation failed";}catch(...){}return false;}
}
bool WebTransportSession::Poll(std::vector<WebTransportEvent>* events,std::string* error) noexcept {
  try {
    auto& state=*impl_;std::lock_guard<std::mutex> guard(state.lock);
    if(state.failed){*error=state.error;return false;}
    events->swap(state.events);state.events.clear();state.eventBytes=0;state.wake.notify_one();return true;
  }catch(const std::exception&){try{*error="WebTransport completion allocation failed";}catch(...){}return false;}
}
void WebTransportSession::Stop() noexcept {
  if(!impl_)return;auto& state=*impl_;state.stopping=true;state.wake.notify_one();state.helper.Cancel();
  if(state.worker.joinable())state.worker.join();
}
}
