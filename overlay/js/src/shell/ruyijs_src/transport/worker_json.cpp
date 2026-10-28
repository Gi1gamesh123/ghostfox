#include "transport/worker_json.h"
#include "transport/worker_json_internal.h"
#include "transport/worker_frame.h"
#include "transport/worker_json_scan.h"
#include <json/json.h>
#include <memory>
#include <new>
#include <ostream>
#include <streambuf>
#include <utility>

namespace ruyijs::transport::worker {
struct ControlDocument::Impl {
  Json::Value root;
  uint64_t epoch = 0, sequence = 0;
};
const Json::Value* ControlAccess::Root(const ControlDocument& document) noexcept {
  return document.impl_ ? &document.impl_->root : nullptr;
}
namespace {
std::string_view Text(const Json::Value& value) noexcept {
  const char* begin = nullptr;
  const char* end = nullptr;
  return value.getString(&begin, &end) ? std::string_view(begin, end - begin) : std::string_view{};
}
bool MessageType(std::string_view type) noexcept {
  constexpr std::string_view types[] = {"hello", "ready", "reserve", "accepted", "activate",
      "activated", "release-reservation", "reservationReleased", "ack-reservation",
      "upload-credit", "upload-end", "headers", "download-credit", "body-end", "cancel",
      "terminal", "released", "ack-terminal", "close", "closed", "fatal"};
  for (auto item : types) if (item == type) return true;
  return false;
}
Status Envelope(const Json::Value& root, uint64_t* epoch, uint64_t* sequence) {
  if (!root.isObject() || root.size() != 5) return Failure(Error::Envelope);
  for (const char* key : {"version", "type", "epoch", "channelSeq", "payload"})
    if (!root.isMember(key)) return Failure(Error::Envelope);
  const auto& version = root["version"];
  if ((version.type() != Json::intValue && version.type() != Json::uintValue) ||
      version.asUInt() != 1 || !root["payload"].isObject() ||
      !root["type"].isString() || !MessageType(Text(root["type"])))
    return Failure(Error::Envelope);
  if (!root["epoch"].isString() || !root["channelSeq"].isString() ||
      !ValidateU64(Text(root["epoch"]), epoch) || !*epoch ||
      !ValidateU64(Text(root["channelSeq"]), sequence) || *sequence == UINT64_MAX)
    return Failure(Error::Envelope);
  return {};
}
class BoundedBuffer final : public std::streambuf {
 public:
  explicit BoundedBuffer(std::string& text) : text_(text) {}
  bool exceeded = false;
 protected:
  std::streamsize xsputn(const char* bytes, std::streamsize count) override {
    if (count < 0 || static_cast<size_t>(count) > kControlLimit + 4 - text_.size()) {
      exceeded = true;
      return 0;
    }
    text_.append(bytes, static_cast<size_t>(count));
    return count;
  }
  int_type overflow(int_type character) override {
    if (traits_type::eq_int_type(character, traits_type::eof())) return traits_type::not_eof(character);
    char value = traits_type::to_char_type(character);
    return xsputn(&value, 1) == 1 ? character : traits_type::eof();
  }
 private:
  std::string& text_;
};
}
ControlDocument::~ControlDocument() noexcept { delete impl_; }
ControlDocument::ControlDocument(ControlDocument&& other) noexcept : impl_(other.impl_) {
  other.impl_ = nullptr;
}
ControlDocument& ControlDocument::operator=(ControlDocument&& other) noexcept {
  if (this != &other) { delete impl_; impl_ = std::exchange(other.impl_, nullptr); }
  return *this;
}
uint64_t ControlDocument::Epoch() const noexcept { return impl_ ? impl_->epoch : 0; }
uint64_t ControlDocument::ChannelSequence() const noexcept { return impl_ ? impl_->sequence : 0; }
std::string_view ControlDocument::Type() const noexcept {
  if (!impl_) return {};
  const Json::Value& root = impl_->root;
  return Text(root["type"]);
}
Status ParseControlJson(std::string_view json, ControlDocument* result) noexcept {
  if (!result) return Failure(Error::Field);
  Status status = ScanJsonQuota(json);
  if (!status) return status;
  try {
    Json::CharReaderBuilder builder;
    Json::CharReaderBuilder::strictMode(&builder.settings_);
    builder["collectComments"] = false;
    builder["skipBom"] = false;
    // The guard counts containers only; JsonCpp also counts a scalar leaf.
    builder["stackLimit"] = static_cast<Json::UInt>(kJsonDepthLimit + 1);
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    auto next = std::make_unique<ControlDocument::Impl>();
    if (!reader->parse(json.data(), json.data() + json.size(), &next->root, nullptr))
      return Failure(Error::Json);
    status = Envelope(next->root, &next->epoch, &next->sequence);
    if (!status) return status;
    delete result->impl_;
    result->impl_ = next.release();
    return {};
  } catch (const std::bad_alloc&) { return Failure(Error::Memory); }
  catch (...) { return Failure(Error::Json); }
}
Status DecodeControlFrame(std::string_view frame, ControlDocument* result) noexcept {
  if (frame.size() < 5 || frame.size() > kControlLimit + 4) return Failure(Error::Length);
  if (ReadLength(frame.data()) != frame.size() - 4) return Failure(Error::Length);
  return ParseControlJson(frame.substr(4), result);
}
Status EncodeControlFrame(const ControlDocument& document, std::string* result) noexcept {
  if (!result || !document.impl_) return Failure(Error::Envelope);
  try {
    std::string next(4, '\0');
    BoundedBuffer buffer(next);
    std::ostream stream(&buffer);
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    builder["commentStyle"] = "None";
    builder["emitUTF8"] = true;
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    int code = writer->write(document.impl_->root, &stream);
    stream.flush();
    if (buffer.exceeded) return Failure(Error::Length);
    if (code || !stream.good()) return Failure(Error::Memory);
    Status status = ScanJsonQuota(std::string_view(next).substr(4));
    if (!status) return status;
    WriteLength(static_cast<uint32_t>(next.size() - 4), next.data());
    result->swap(next);
    return {};
  } catch (const std::bad_alloc&) { return Failure(Error::Memory); }
  catch (...) { return Failure(Error::Json); }
}
}
