#ifndef RUYIJS_WEB_CANVAS_RASTER_COMMANDS_H
#define RUYIJS_WEB_CANVAS_RASTER_COMMANDS_H
// The recorded canvas command list becomes the rasterizer's path.
#include <cstdint>

#include "web_canvas/raster_path.h"

namespace ruyijs::web_canvas::raster {
// ArcToBezier (gfx/2d/PathHelpers.h) with the transform of the recording command.
void ArcToBezier(Path* sink, double cx, double cy, double radiusX, double radiusY,
                 float startAngle, float endAngle, bool anticlockwise, float rotation,
                 const double matrix[6]);
// One recorded fill path: [8, matrix..., opcode, values...] records.
void BuildPathFromCommands(const double* commands, uint32_t count, bool evenOdd, Path* path);
}  // namespace ruyijs::web_canvas::raster
#endif
