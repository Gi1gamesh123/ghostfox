#ifndef RUYIJS_WEB_CANVAS_RASTER_BUILD_H
#define RUYIJS_WEB_CANVAS_RASTER_BUILD_H
// Path to analytic edges, and the convexity question that picks the walker.
#include <memory>
#include <vector>

#include "web_canvas/raster_clip.h"
#include "web_canvas/raster_edge.h"
#include "web_canvas/raster_path.h"

namespace ruyijs::web_canvas::raster {
// The edge storage of one fill: edges are stable in memory, like Skia's arena.
struct EdgePool {
  std::vector<std::unique_ptr<Edge>> storage;
  std::vector<Edge*> edges;
  Edge* New() {
    storage.push_back(std::make_unique<Edge>());
    return storage.back().get();
  }
  void Push(Edge* edge) { edges.push_back(edge); }
};

// SkEdgeBuilder::buildEdges: |clip| is only passed when the path is not contained
// in the clip, and then every segment is clipped first (SkEdgeClipper).
void BuildEdges(const Path& path, EdgePool* pool, const ClipRect* clip = nullptr);
// SkPathPriv::ComputeConvexity, reduced to the answer the fill dispatch needs.
bool IsConvexPath(const Path& path);
}  // namespace ruyijs::web_canvas::raster
#endif
