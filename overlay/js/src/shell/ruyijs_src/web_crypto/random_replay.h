#ifndef RUYIJS_CRYPTO_RANDOM_REPLAY_H
#define RUYIJS_CRYPTO_RANDOM_REPLAY_H
#include "js/RootingAPI.h"
#include <cstddef>

namespace ruyijs::web_crypto {
bool CreateRandomReplay(JSContext*, JS::HandleObject calls, JS::MutableHandleObject tape);
bool AttachRandomReplay(JSContext*, JS::HandleObject page, JS::HandleObject tape);
bool RandomReplayStatus(JSContext*, JS::HandleObject tape, JS::MutableHandleValue result);
bool ReadRandomReplay(JSContext*, JS::HandleObject crypto, size_t bytes, bool uuid,
                      JS::MutableHandleObject buffer, bool* configured);
void CommitRandomReplay(JS::HandleObject crypto);
}
#endif
