#ifndef RUYIJS_RUNTIME_TOP_LEVEL_ASSEMBLY_H
#define RUYIJS_RUNTIME_TOP_LEVEL_ASSEMBLY_H
namespace ruyijs {
struct BrowsingContext;
class Frames;
class PageLoad;
class ApiMonitor;
namespace execution_scheduling { class Realm; }
// The ports a top-level document has beyond an iframe's, bound for the first
// document by runtime.cpp and for every later one by TopLevel
// (docs/owners/TOP_LEVEL.md): Location navigation through the one PageLoad,
// popstate/hashchange/reportError on the document's own dispatch tree.
void BindDocumentEvents(BrowsingContext& target, PageLoad& pages);
// The owners a later top-level document's ports reach. Declared in Run after
// all of them and destroyed before any of them.
struct TopLevelAssembly {
  execution_scheduling::Realm& scheduling;
  PageLoad& pages;
  ApiMonitor& monitor;
  Frames& frames;
};
// TopLevel document port: document events, the Web Audio clock and, in debug
// mode, the monitor ports of the new document's owners.
bool BindTopLevelDocument(void* assembly, BrowsingContext& target);
// TopLevel retire port: drops the retired document's scheduled work (and its
// frames'), stops its workers and messages and clears every port bound above.
void RetireTopLevelDocument(void* assembly, BrowsingContext& target);
}
#endif
