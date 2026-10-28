// aaa_walk_edges: the general walk of Skia's analytic anti-aliasing, used for any
// path that is not convex (the even-odd double ring of two arcs among them). It
// keeps the active edge list sorted by x, adds fractional scanlines at edge
// crossings and flushes one accumulated coverage row at a time.
#include "web_canvas/raster_trapezoid.h"
#include "web_canvas/raster_walk.h"

namespace ruyijs::web_canvas::raster {
namespace {
int Abs32(int32_t v) { return v < 0 ? -v : v; }
Fixed MinF(Fixed a, Fixed b) { return a < b ? a : b; }
Fixed MaxF(Fixed a, Fixed b) { return a > b ? a : b; }

void RemoveEdge(Edge* edge) {
  edge->prev->next = edge->next;
  edge->next->prev = edge->prev;
}

void InsertEdgeAfter(Edge* edge, Edge* after) {
  edge->prev = after;
  edge->next = after->next;
  after->next->prev = edge;
  after->next = edge;
}

void BackwardInsertEdgeBasedOnX(Edge* edge) {
  const Fixed x = edge->x;
  Edge* prev = edge->prev;
  while (prev->prev && prev->x > x) prev = prev->prev;
  if (prev->next != edge) {
    RemoveEdge(edge);
    InsertEdgeAfter(edge, prev);
  }
}

Edge* BackwardInsertStart(Edge* prev, Fixed x) {
  while (prev->prev && prev->x > x) prev = prev->prev;
  return prev;
}

void UpdateNextNextY(Fixed y, Fixed nextY, Fixed* nextNextY) {
  *nextNextY = y > nextY && y < *nextNextY ? y : *nextNextY;
}

void CheckIntersection(const Edge* edge, Fixed nextY, Fixed* nextNextY) {
  if (edge->prev->prev && edge->prev->x + edge->prev->dx > edge->x + edge->dx) {
    *nextNextY = nextY + (kFixed1 >> 2);
  }
}

void CheckIntersectionFwd(const Edge* edge, Fixed nextY, Fixed* nextNextY) {
  if (edge->next->next && edge->x + edge->dx > edge->next->x + edge->next->dx) {
    *nextNextY = nextY + (kFixed1 >> 2);
  }
}

// True when the two edges are within one pixel of each other on this row.
bool EdgesTooClose(const Edge* prev, const Edge* next, Fixed lowerY) {
  return next && prev && next->upperY < lowerY && prev->x + kFixed1 >= next->x - Abs32(next->dx);
}

bool EdgesTooClose(int prevRite, Fixed ul, Fixed ll) {
  return prevRite > FloorToInt(ul) || prevRite > FloorToInt(ll);
}

void InsertNewEdges(Edge* newEdge, Fixed y, Fixed* nextNextY) {
  if (newEdge->upperY > y) {
    UpdateNextNextY(newEdge->upperY, y, nextNextY);
    return;
  }
  const Edge* prev = newEdge->prev;
  if (prev->x <= newEdge->x) {
    while (newEdge->upperY <= y) {
      CheckIntersection(newEdge, y, nextNextY);
      UpdateNextNextY(newEdge->lowerY, y, nextNextY);
      newEdge = newEdge->next;
    }
    UpdateNextNextY(newEdge->upperY, y, nextNextY);
    return;
  }
  Edge* start = BackwardInsertStart(newEdge->prev, newEdge->x);
  do {
    Edge* next = newEdge->next;
    for (;;) {
      if (start->next == newEdge) break;
      Edge* after = start->next;
      if (after->x >= newEdge->x) {
        RemoveEdge(newEdge);
        InsertEdgeAfter(newEdge, start);
        break;
      }
      start = after;
    }
    CheckIntersection(newEdge, y, nextNextY);
    CheckIntersectionFwd(newEdge, y, nextNextY);
    UpdateNextNextY(newEdge->lowerY, y, nextNextY);
    start = newEdge;
    newEdge = next;
  } while (newEdge->upperY <= y);
  UpdateNextNextY(newEdge->upperY, y, nextNextY);
}
}  // namespace

void WalkEdges(Edge* head, Edge* tail, bool evenOdd, AdditiveBlitter& blitter, int startY,
               int stopY, Fixed leftClip, Fixed rightClip, bool forceRLE, bool skipIntersect) {
  head->x = head->upperX = leftClip;
  tail->x = tail->upperX = rightClip;
  Fixed y = MaxF(head->next->upperY, IntToFixed(startY));
  Fixed nextNextY = kMaxS32;
  for (Edge* edge = head->next; edge->upperY <= y; edge = edge->next) {
    edge->GoY(y);
    UpdateNextNextY(edge->lowerY, y, &nextNextY);
  }
  UpdateNextNextY(head->next->upperY, y, &nextNextY);
  const int windingMask = evenOdd ? 1 : -1;
  for (;;) {
    int winding = 0;
    bool inInterval = false;
    Fixed prevX = head->x;
    Fixed nextY = MinF(nextNextY, CeilToFixed(y + 1));
    Edge* currE = head->next;
    Edge* leftE = head;
    Fixed left = leftClip;
    Fixed leftDy = 0;
    int prevRite = FloorToInt(leftClip);
    nextNextY = kMaxS32;
    int yShift = 0;
    if ((nextY - y) & (kFixed1 >> 2)) {
      yShift = 2;
      nextY = y + (kFixed1 >> 2);
    } else if ((nextY - y) & (kFixed1 >> 1)) {
      yShift = 1;
    }
    const uint8_t fullAlpha = GetPartialAlpha(nextY - y);
    const bool noRealBlitter = forceRLE;
    while (currE->upperY <= y) {
      winding += currE->winding;
      const bool wasInInterval = inInterval;
      inInterval = (winding & windingMask) != 0;
      if (inInterval && !wasInInterval) {
        left = MaxF(currE->x, leftClip);
        leftDy = currE->dy;
        leftE = currE;
        currE->GoY(nextY, yShift);
      } else if (!inInterval && wasInInterval) {
        Fixed rite = currE->x;
        currE->GoY(nextY, yShift);
        const Fixed nextLeft = MaxF(leftClip, leftE->x);
        rite = MinF(rightClip, rite);
        const Fixed nextRite = MinF(rightClip, currE->x);
        BlitTrapezoid(blitter, y >> 16, left, rite, nextLeft, nextRite, leftDy, currE->dy,
                      fullAlpha, noRealBlitter ||
                                     (fullAlpha == 0xff &&
                                      (EdgesTooClose(prevRite, left, leftE->x) ||
                                       EdgesTooClose(currE, currE->next, nextY))));
        prevRite = CeilToInt(MaxF(rite, currE->x));
      } else {
        currE->GoY(nextY, yShift);
      }
      Edge* next = currE->next;
      while (currE->lowerY <= nextY) {
        if (currE->curveCount < 0) {
          currE->KeepContinuous();
          if (!UpdateCubic(*currE)) break;
        } else if (currE->curveCount > 0) {
          currE->KeepContinuous();
          if (!UpdateQuadratic(*currE)) break;
        } else {
          break;
        }
      }
      if (currE->lowerY <= nextY) {
        RemoveEdge(currE);
      } else {
        UpdateNextNextY(currE->lowerY, nextY, &nextNextY);
        const Fixed newX = currE->x;
        if (newX < prevX) {
          BackwardInsertEdgeBasedOnX(currE);
        } else {
          prevX = newX;
        }
        if (!skipIntersect) CheckIntersection(currE, nextY, &nextNextY);
      }
      currE = next;
    }
    if (inInterval) {
      BlitTrapezoid(blitter, y >> 16, left, rightClip, MaxF(leftClip, leftE->x), rightClip, leftDy,
                    0, fullAlpha, noRealBlitter ||
                                   (fullAlpha == 0xff &&
                                    EdgesTooClose(leftE->prev, leftE, nextY)));
    }
    if (forceRLE) blitter.FlushIfYChanged(y, nextY);
    y = nextY;
    if (y >= IntToFixed(stopY)) break;
    InsertNewEdges(currE, y, &nextNextY);
  }
}
}  // namespace ruyijs::web_canvas::raster
