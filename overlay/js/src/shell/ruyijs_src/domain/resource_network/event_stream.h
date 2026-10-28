#pragma once
// text/event-stream interpretation (HTML "Server-sent events", 9.2.6): lines end at
// CR LF, LF or CR; a blank line dispatches the buffered event; fields are event,
// data, id (ignored when it contains U+0000) and retry (ASCII digits only).
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ruyijs::domain::resource_network {
struct ServerEvent {
  std::u16string type, data, lastEventId;
};
class EventStreamParser {
 public:
  // Feeds decoded text; complete events are appended to `events`.
  void Feed(std::u16string_view text, std::vector<ServerEvent>* events) {
    for (char16_t c : text) {
      if (skipLf_ && c == u'\n') { skipLf_ = false; continue; }
      skipLf_ = false;
      if (c == u'\r' || c == u'\n') {
        skipLf_ = c == u'\r';
        Line(line_, events);
        line_.clear();
      } else {
        line_.push_back(c);
      }
    }
  }
  // End of the response: an unterminated event is discarded (spec).
  void End() { line_.clear(); data_.clear(); type_.clear(); }
  const std::u16string& lastEventId() const { return lastEventId_; }
  std::optional<uint32_t> TakeRetry() { auto value = retry_; retry_.reset(); return value; }

 private:
  void Line(const std::u16string& line, std::vector<ServerEvent>* events) {
    if (line.empty()) {
      if (!data_.empty()) {
        if (data_.back() == u'\n') data_.pop_back();
        events->push_back({type_.empty() ? u"message" : type_, data_, lastEventId_});
      }
      data_.clear();
      type_.clear();
      return;
    }
    if (line[0] == u':') return;
    const size_t colon = line.find(u':');
    std::u16string field = line.substr(0, colon), value;
    if (colon != std::u16string::npos) {
      value = line.substr(colon + 1);
      if (!value.empty() && value[0] == u' ') value.erase(0, 1);
    }
    if (field == u"event") type_ = value;
    else if (field == u"data") { data_ += value; data_.push_back(u'\n'); }
    else if (field == u"id") { if (value.find(u'\0') == std::u16string::npos) lastEventId_ = value; }
    else if (field == u"retry") {
      if (value.empty()) return;
      uint64_t ms = 0;
      for (char16_t c : value) {
        if (c < u'0' || c > u'9') return;
        ms = ms * 10 + uint64_t(c - u'0');
        if (ms > UINT32_MAX) return;
      }
      retry_ = uint32_t(ms);
    }
  }
  std::u16string line_, data_, type_, lastEventId_;
  std::optional<uint32_t> retry_;
  bool skipLf_ = false;
};
}  // namespace ruyijs::domain::resource_network
