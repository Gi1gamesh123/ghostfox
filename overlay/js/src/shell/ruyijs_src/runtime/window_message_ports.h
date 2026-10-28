#ifndef RUYIJS_RUNTIME_WINDOW_MESSAGE_PORTS_H
#define RUYIJS_RUNTIME_WINDOW_MESSAGE_PORTS_H
namespace ruyijs {
struct BrowsingContext;
namespace execution_scheduling { class Realm; }
void BindWindowMessages(BrowsingContext&, execution_scheduling::Realm&);
}
#endif
