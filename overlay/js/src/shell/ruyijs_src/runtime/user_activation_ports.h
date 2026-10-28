#pragma once
namespace ruyijs {
struct BrowsingContext;
namespace execution_scheduling { class Realm; }
void BindUserActivation(BrowsingContext&, execution_scheduling::Realm&);
void UnbindUserActivation(BrowsingContext&);
}
