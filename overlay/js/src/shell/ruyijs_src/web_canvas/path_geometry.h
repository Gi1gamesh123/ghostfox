#ifndef RUYIJS_CANVAS_PATH_GEOMETRY_H
#define RUYIJS_CANVAS_PATH_GEOMETRY_H
#include <vector>
namespace ruyijs::web_canvas {
// Temporary projection of the context's one command list, never persistent state.
bool PathContains(const std::vector<double>& commands, double x, double y, bool evenodd);
}
#endif
