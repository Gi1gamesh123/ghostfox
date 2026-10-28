#include "resource_network/worker_fetch_session.h"
#include "resource_network/worker_network_wire.h"
#include <map>
#include <new>

namespace ruyijs::resource_network {
namespace {
namespace wire=worker_network_wire;
struct Pending {AsyncHttpTicket ticket;bool done=false;AsyncHttpResult result;};
void Complete(void* target,AsyncHttpResult&& result) noexcept {
  auto& pending=*static_cast<Pending*>(target);
  pending.result=std::move(result);pending.done=true;
}
class Session final:public WorkerFetchSession {
 public:
  std::shared_ptr<AsyncHttpSource> source;
  uint64_t next=1;
  std::map<uint64_t,std::shared_ptr<Pending>> requests;
  ~Session() override {for (const auto& item:requests) CancelAsyncHttp(source,item.second->ticket);}
  bool Request(std::string_view input,std::string* output) noexcept override {
    try {
      if (input.substr(0,2)=="FS") {
        if (requests.size()>=10000 || !next) return false;
        wire::Request request;
        if (!wire::DecodeRequest(input.substr(2),&request)) return false;
        auto pending=std::make_shared<Pending>();
        const uint64_t id=next++;
        requests.emplace(id,pending);
        const auto status=StartAsyncHttp(source,{request.method,request.url,request.headers,request.body,
            request.redirect,request.options},{pending,Complete},&pending->ticket);
        if (status.code!=ErrorCode::None) {requests.erase(id);return false;}
        output->assign("FO");wire::Writer writer(output);writer.Wide(id);return true;
      }
      wire::Reader reader(input.substr(2));uint64_t id=0;
      if (input.size()<2 || input[0]!='F' || !reader.Wide(&id) || !reader.Done()) return false;
      auto found=requests.find(id);
      if (input[1]=='C') {
        if (found!=requests.end()) {CancelAsyncHttp(source,found->second->ticket);requests.erase(found);}
        output->assign("FO");return true;
      }
      if (input[1]!='P' || found==requests.end()) return false;
      auto& pending=*found->second;
      if (!pending.done) {output->assign("FP");return true;}
      const auto& result=pending.result;
      std::string response;
      if (!wire::EncodeResult(result.outcome==AsyncHttpOutcome::Http && result.ok,result.response,&response)) return false;
      output->assign("FR");output->append(response);requests.erase(found);return true;
    } catch (...) {return false;}
  }
};
}
Status NewWorkerFetchSession(const std::shared_ptr<AsyncHttpSource>& source,
                             std::shared_ptr<WorkerFetchSession>* result) noexcept {
  try {auto session=std::make_shared<Session>();session->source=source;*result=std::move(session);return {};}
  catch (...) {return {ErrorCode::OutOfMemory,"Worker fetch session allocation failed"};}
}
}
