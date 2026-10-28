#include "execution_scheduling/model_internal.h"
#include <climits>
namespace ruyijs::execution_scheduling {
void Model::CancelWebTask(uint64_t id) noexcept {
 auto& loop=impl_->loop;auto task=loop.find_task(id);
 if(task&&task->type()==domain::Task::Type::WebTask){loop.cancel_task(id);loop.purge_cancelled();}
}
Status Model::ScheduleWebTask(double delay,int priority,bool continuation,uint64_t* id) noexcept {
 auto& loop=impl_->loop;
 if(loop.macrotask_count()>=10000)return Range("Pending task limit exceeded");
 if(loop.last_task_id()>=INT32_MAX)return Range("Task identifier limit exceeded");
 try{*id=loop.schedule_task(domain::Task::Type::WebTask,domain::TaskPriority::UserDefined,delay);
  loop.find_task(*id)->set_web_priority(priority,continuation);return {};}
 catch(const std::bad_alloc&){return OOM();}
 catch(...){return {ErrorCode::InvalidState,"Scheduler registration failed"};}
}
void Model::PrioritizeWebTask(uint64_t id,int priority) noexcept {
 auto task=impl_->loop.find_task(id);if(task)task->set_web_priority(priority,task->continuation());
}
}
