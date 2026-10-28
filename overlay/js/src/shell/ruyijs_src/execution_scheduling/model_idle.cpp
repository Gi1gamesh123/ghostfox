#include "execution_scheduling/model_internal.h"
#include <algorithm>
#include <climits>
namespace ruyijs::execution_scheduling {
Status Model::ScheduleIdle(bool hasTimeout,uint32_t timeout,uint64_t* id) noexcept {
 auto& loop=impl_->loop;
 if(loop.macrotask_count()>=10000)return Range("Pending timer limit exceeded");
 if(loop.last_task_id()>=INT32_MAX)return Range("Timer identifier limit exceeded");
 // Firefox's TimeoutManager takes a signed interval after unsigned WebIDL
 // conversion. Negative signed values become a zero-delay timeout.
 int32_t interval=static_cast<int32_t>(timeout);
 double now=loop.current_time().ms(),expires=hasTimeout?now+std::max(0,interval):1e18;
 double ready=std::min(loop.clock().idle_running?std::max(now,loop.clock().idle_end):now,expires);
 try {
  *id=loop.schedule_task(domain::Task::Type::IdleCallback,domain::TaskPriority::Idle,ready-now);
  loop.find_task(*id)->set_idle_timeout(expires);return {};
 }catch(const std::bad_alloc&){return OOM();}
 catch(...){return {ErrorCode::InvalidState,"Idle registration failed"};}
}
void Model::CancelIdle(uint64_t id) noexcept {
 auto& loop=impl_->loop;auto task=loop.find_task(id);
 if(!task||task->type()!=domain::Task::Type::IdleCallback)return;
 loop.cancel_task(id);loop.purge_cancelled();
}
Status Model::ScheduleDelayed(double delay,uint64_t* id) noexcept {
 auto& loop=impl_->loop;
 if(loop.macrotask_count()>=10000)return Range("Pending task limit exceeded");
 if(loop.last_task_id()>=INT32_MAX)return Range("Task identifier limit exceeded");
 try{*id=loop.schedule_task(domain::Task::Type::NetworkCallback,domain::TaskPriority::Timer,delay);return {};}
 catch(const std::bad_alloc&){return OOM();}
 catch(...){return {ErrorCode::InvalidState,"Delayed registration failed"};}
}
}
