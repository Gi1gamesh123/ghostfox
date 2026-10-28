#pragma once
#include "resource_network/model_webtransport.h"
#include "resource_network/necko_helper.h"
#include <cstring>
#include <stdexcept>
#include <string_view>

namespace ruyijs::resource_network::webtransport_wire {
constexpr uint32_t kFrameLimit=2*1024*1024;
struct Writer {
  std::string bytes;
  template<class T> void Number(T value) { bytes.append(reinterpret_cast<const char*>(&value),sizeof(value)); }
  void Text(std::string_view value) {
    if(value.size()>kFrameLimit) throw std::length_error("WebTransport field limit");
    Number(uint32_t(value.size()));bytes.append(value);
  }
};
struct Reader {
  std::string_view bytes;
  template<class T> T Number() {
    if(bytes.size()<sizeof(T))throw std::runtime_error("Truncated WebTransport frame");
    T value;std::memcpy(&value,bytes.data(),sizeof(T));bytes.remove_prefix(sizeof(T));return value;
  }
  std::string Text() {
    const auto size=Number<uint32_t>();
    if(size>bytes.size())throw std::runtime_error("Truncated WebTransport field");
    std::string value(bytes.substr(0,size));bytes.remove_prefix(size);return value;
  }
};
bool Exchange(NeckoPipe&,NeckoPipe&,const Writer&,std::vector<WebTransportEvent>*);
Writer Begin(const WebTransportOptions&);
Writer Command(uint64_t,const WebTransportCommand&);
Writer Poll();
}
