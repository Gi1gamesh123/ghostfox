#ifndef RUYIJS_RESOURCE_TRANSFORM_STREAM_H
#define RUYIJS_RESOURCE_TRANSFORM_STREAM_H
namespace ruyijs { class PageHelpers; }
namespace ruyijs::resource_network {
struct State;
bool InstallTransformStreams(State&, ::ruyijs::PageHelpers&);
}
#endif
