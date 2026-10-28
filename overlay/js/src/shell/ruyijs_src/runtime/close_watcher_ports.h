#pragma once
namespace ruyijs {
struct BrowsingContext;
void BindCloseWatcher(BrowsingContext&);
void UnbindCloseWatcher(BrowsingContext&);
}
