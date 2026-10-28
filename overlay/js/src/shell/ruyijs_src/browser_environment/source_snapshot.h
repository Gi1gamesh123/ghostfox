#pragma once
#include "domain/browser_environment/origin_value.h"

namespace ruyijs::browser_environment {
using OriginValue = domain::browser_environment::OriginValue;
using OriginKind = domain::browser_environment::OriginKind;
// One staged read from the original origin and secure-context owners.
struct SourceSnapshot {
  OriginValue origin;
  bool secure = false;
};
}
