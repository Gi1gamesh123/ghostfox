#pragma once
#include "domain/document_model/dom_string.h"

namespace ruyijs::domain::document_model {
// Owned by the original Element, never by a JS facade or an adapter cache.
struct TextareaState {
  DomString value;
  uint32_t start = 0, end = 0;
  enum Direction { None, Forward, Backward } direction = None;
  static Direction parse_direction(DomStringView value) {
    return value == u"forward" ? Forward : value == u"backward" ? Backward : None;
  }
  DomStringView direction_name() const {
    return direction == Forward ? u"forward" : direction == Backward ? u"backward" : u"none";
  }
};
}
