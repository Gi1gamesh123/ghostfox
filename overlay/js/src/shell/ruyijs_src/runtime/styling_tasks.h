#pragma once
#include "web_styling/realm.h"
#include "execution_scheduling/realm.h"
namespace ruyijs {
void BindStylingTasks(web_styling::Realm&, execution_scheduling::Realm&);
class StylingTaskBinding {
 public:
  StylingTaskBinding(web_styling::Realm& styling, execution_scheduling::Realm& scheduling)
      : styling_(styling) { BindStylingTasks(styling, scheduling); }
  ~StylingTaskBinding() { styling_.SetTaskPort(nullptr, nullptr); }
 private:
  web_styling::Realm& styling_;
};
}
