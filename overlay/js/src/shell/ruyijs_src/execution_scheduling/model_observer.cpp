#include "execution_scheduling/model_observer.h"
#include "domain/execution_scheduling/performance_subscription.h"
#include <new>
namespace ruyijs::execution_scheduling {
struct PerfSubscription {ruyijs::domain::execution_scheduling::PerformanceSubscription value;};
PerfSubscription* CreatePerfSubscription() noexcept{return new(std::nothrow) PerfSubscription;}
void DeletePerfSubscription(PerfSubscription* p) noexcept{delete p;}
bool ConfigurePerfSubscription(PerfSubscription* p,bool multiple,uint32_t types) noexcept{return p->value.Configure(multiple,types);}
void DisconnectPerfSubscription(PerfSubscription* p) noexcept{p->value.Disconnect();}
bool PerfSubscriptionConnected(PerfSubscription* p) noexcept{return p->value.connected();}
bool PerfSubscriptionObserves(PerfSubscription* p,uint32_t type) noexcept{return p->value.Observes(type);}
}
