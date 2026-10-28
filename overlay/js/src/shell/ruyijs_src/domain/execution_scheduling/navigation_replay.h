#pragma once
#include <optional>
#include <string>
#include <vector>
#include <cstdint>
namespace ruyijs::domain::execution_scheduling {
struct NavigationReplayFields {
  std::optional<double> request_start, response_start, response_end, load_event_end;
  std::optional<uint64_t> transfer_size, encoded_size, decoded_size;
  unsigned Count() const {
    return unsigned(request_start.has_value()) + unsigned(response_start.has_value()) +
        unsigned(response_end.has_value()) + unsigned(load_event_end.has_value()) +
        unsigned(transfer_size.has_value()) + unsigned(encoded_size.has_value()) +
        unsigned(decoded_size.has_value());
  }
};
struct NavigationReplayDocument {
  std::string url;
  NavigationReplayFields fields;
  bool bound = false;
};
using NavigationReplayDocuments = std::vector<NavigationReplayDocument>;
}
