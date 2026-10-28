#pragma once
#include <cstdint>
namespace ruyijs::execution_scheduling {
struct PerfSubscription;
PerfSubscription* CreatePerfSubscription() noexcept;
void DeletePerfSubscription(PerfSubscription*) noexcept;
bool ConfigurePerfSubscription(PerfSubscription*,bool,uint32_t) noexcept;
void DisconnectPerfSubscription(PerfSubscription*) noexcept;
bool PerfSubscriptionConnected(PerfSubscription*) noexcept;
bool PerfSubscriptionObserves(PerfSubscription*,uint32_t) noexcept;
}
