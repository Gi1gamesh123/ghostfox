#include "web_console/realm.h"

namespace ruyijs::web_console {
void Log::Append(Record record) {
  if (record.text.size() > kTextLimit) record.text.resize(kTextLimit);
  if (record.stack.size() > kStackLimit) record.stack.resize(kStackLimit);
  for (std::string& frame : record.stack)
    if (frame.size() > kTextLimit) frame.resize(kTextLimit);
  records_.push_back(std::move(record));
  while (records_.size() > kCapacity) records_.pop_front();
}

void Log::ClearRealm(uint32_t realm) {
  std::deque<Record> kept;
  for (Record& record : records_)
    if (record.realm != realm) kept.push_back(std::move(record));
  records_ = std::move(kept);
}
}
