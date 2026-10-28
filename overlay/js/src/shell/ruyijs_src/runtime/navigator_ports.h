#ifndef RUYIJS_RUNTIME_NAVIGATOR_PORTS_H
#define RUYIJS_RUNTIME_NAVIGATOR_PORTS_H
#include <string>
namespace ruyijs::resource_network { class Realm; }
namespace ruyijs {
struct BrowsingContext;
void BindNavigatorPorts(BrowsingContext&, resource_network::Realm&);
// The creating document's base URL (and its own URL, inherited for about:blank).
bool DocumentBase(BrowsingContext&, std::string* base, std::string* source);
// Worker script URLs resolve against the document base; same-origin only.
void BindWorkerScriptUrls(BrowsingContext&);
}
#endif
