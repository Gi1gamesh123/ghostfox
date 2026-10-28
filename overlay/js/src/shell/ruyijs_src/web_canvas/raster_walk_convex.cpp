// aaa_walk_convex_edges: the fast path Skia takes for a convex path, which walks
// only the left and right edge of each scanline band and snaps x to a 1/16 pixel
// before computing coverage. The x snapping is part of the result, not an
// optimisation we may drop.
#include "web_canvas/raster_trapezoid.h"
#include "web_canvas/raster_walk.h"

namespace ruyijs::web_canvas::raster {
namespace {
int Abs32(int32_t v) { return v < 0 ? -v : v; }
Fixed MinF(Fixed a, Fixed b) { return a < b ? a : b; }
Fixed MaxF(Fixed a, Fixed b) { return a > b ? a : b; }

// is_smooth_enough: true when the edge pair changes slowly enough that Skia may
// jump straight to the next whole y instead of stopping at every curve sample.
bool SmoothEdge(const Edge* thisEdge, const Edge* nextEdge) {
  if (thisEdge->curveCount < 0) {
    const int shift = thisEdge->curveShift;
    return Abs32(thisEdge->cdx) >> 1 >= Abs32(thisEdge->cddx) >> shift &&
           Abs32(thisEdge->cdy) >> 1 >= Abs32(thisEdge->cddy) >> shift &&
           (thisEdge->cdy - (thisEdge->cddy >> shift)) >> thisEdge->cubicDShift >= kFixed1;
  }
  if (thisEdge->curveCount > 0) {
    return Abs32(thisEdge->qdx) >> 1 >= Abs32(thisEdge->qddx) &&
           Abs32(thisEdge->qdy) >> 1 >= Abs32(thisEdge->qddy) &&
           (thisEdge->qdy - thisEdge->qddy) >> thisEdge->curveShift >= kFixed1;
  }
  return Abs32(SatSub(nextEdge->dx, thisEdge->dx)) <= kFixed1 &&
         nextEdge->lowerY - nextEdge->upperY >= kFixed1;
}

bool SmoothEnough(const Edge* leftE, const Edge* riteE, const Edge* currE, int stopY) {
  if (currE->upperY >= Fixed(stopY) << 16) return false;
  if (leftE->lowerY + kFixed1 < riteE->lowerY) return SmoothEdge(leftE, currE);
  if (leftE->lowerY > riteE->lowerY + kFixed1) return SmoothEdge(riteE, currE);
  const Edge* nextCurrE = currE->next;
  if (nextCurrE->upperY >= Fixed(stopY) << 16) return false;
  if (nextCurrE->upperX < currE->upperX) {
    const Edge* swap = currE;
    currE = nextCurrE;
    nextCurrE = swap;
  }
  return SmoothEdge(leftE, currE) && SmoothEdge(riteE, nextCurrE);
}
}  // namespace

void WalkConvexEdges(Edge* head, AdditiveBlitter& blitter, int stopY, Fixed leftBound,
                     Fixed rightBound) {
  Edge* leftE = head;
  Edge* riteE = leftE->next;
  Edge* currE = riteE->next;
  Fixed y = MaxF(leftE->upperY, riteE->upperY);
  for (;;) {
    while (leftE->lowerY <= y) {
      if (!leftE->Update(y)) {
        if (FloorToInt(currE->upperY) >= stopY) return;
        leftE = currE;
        currE = currE->next;
      }
    }
    while (riteE->lowerY <= y) {
      if (!riteE->Update(y)) {
        if (FloorToInt(currE->upperY) >= stopY) return;
        riteE = currE;
        currE = currE->next;
      }
    }
    if (FloorToInt(y) >= stopY) break;
    leftE->GoY(y);
    riteE->GoY(y);
    if (leftE->x > riteE->x || (leftE->x == riteE->x && leftE->dx > riteE->dx)) {
      Edge* swap = leftE;
      leftE = riteE;
      riteE = swap;
    }
    Fixed localBot = MinF(leftE->lowerY, riteE->lowerY);
    if (SmoothEnough(leftE, riteE, currE, stopY)) localBot = CeilToFixed(localBot);
    localBot = MinF(localBot, IntToFixed(stopY));
    Fixed left = MaxF(leftBound, leftE->x);
    const Fixed dLeft = leftE->dx;
    Fixed rite = MinF(rightBound, riteE->x);
    const Fixed dRite = riteE->dx;
    if (0 == (dLeft | dRite)) {
      const int fullLeft = CeilToInt(left);
      const int fullRite = FloorToInt(rite);
      const Fixed partialLeft = IntToFixed(fullLeft) - left;
      const Fixed partialRite = rite - IntToFixed(fullRite);
      const int fullTop = CeilToInt(y);
      const int fullBot = FloorToInt(localBot);
      Fixed partialTop = IntToFixed(fullTop) - y;
      Fixed partialBot = localBot - IntToFixed(fullBot);
      if (fullTop > fullBot) {
        partialTop -= (kFixed1 - partialBot);
        partialBot = 0;
      }
      if (fullRite >= fullLeft) {
        if (partialTop > 0) {
          if (partialLeft > 0) {
            blitter.BlitAntiH(fullLeft - 1, fullTop - 1,
                              FixedToAlpha(FixedMul(partialTop, partialLeft)));
          }
          blitter.BlitAntiH(fullLeft, fullTop - 1, fullRite - fullLeft,
                            FixedToAlpha(partialTop));
          if (partialRite > 0) {
            blitter.BlitAntiH(fullRite, fullTop - 1,
                              FixedToAlpha(FixedMul(partialTop, partialRite)));
          }
          blitter.FlushIfYChanged(y, y + partialTop);
        }
        if (fullBot > fullTop && (fullRite > fullLeft || FixedToAlpha(partialLeft) > 0 ||
                                  FixedToAlpha(partialRite) > 0)) {
          blitter.BlitAntiRect(fullLeft - 1, fullTop, fullRite - fullLeft,
                                      fullBot - fullTop, FixedToAlpha(partialLeft),
                                      FixedToAlpha(partialRite));
        }
        if (partialBot > 0) {
          if (partialLeft > 0) {
            blitter.BlitAntiH(fullLeft - 1, fullBot,
                              FixedToAlpha(FixedMul(partialBot, partialLeft)));
          }
          blitter.BlitAntiH(fullLeft, fullBot, fullRite - fullLeft, FixedToAlpha(partialBot));
          if (partialRite > 0) {
            blitter.BlitAntiH(fullRite, fullBot, FixedToAlpha(FixedMul(partialBot, partialRite)));
          }
        }
      } else {
        const Fixed width = rite - left;
        if (width > 0) {
          if (partialTop > 0) {
            blitter.BlitAntiH(fullLeft - 1, fullTop - 1, 1,
                              FixedToAlpha(FixedMul(partialTop, width)));
            blitter.FlushIfYChanged(y, y + partialTop);
          }
          if (fullBot > fullTop) {
            blitter.BlitV(fullLeft - 1, fullTop, fullBot - fullTop, FixedToAlpha(width));
          }
          if (partialBot > 0) {
            blitter.BlitAntiH(fullLeft - 1, fullBot, 1, FixedToAlpha(FixedMul(partialBot, width)));
          }
        }
      }
      y = localBot;
    } else {
      const Fixed kSnapDigit = kFixed1 >> 4;
      const Fixed kSnapHalf = kSnapDigit >> 1;
      const Fixed kSnapMask = (-1 ^ (kSnapDigit - 1));
      left += kSnapHalf;
      rite += kSnapHalf;
      int count = CeilToInt(localBot) - FloorToInt(y);
      if (count > 1) {
        if ((int)(y & 0xffff0000) != y) {
          --count;
          const Fixed nextY = CeilToFixed(y + 1);
          const Fixed dY = nextY - y;
          const Fixed nextLeft = left + FixedMul(dLeft, dY);
          const Fixed nextRite = rite + FixedMul(dRite, dY);
          BlitTrapezoid(blitter, y >> 16, left & kSnapMask, rite & kSnapMask, nextLeft & kSnapMask,
                        nextRite & kSnapMask, leftE->dy, riteE->dy, GetPartialAlpha(dY), false);
          blitter.FlushIfYChanged(y, nextY);
          left = nextLeft;
          rite = nextRite;
          y = nextY;
        }
        while (count > 1) {
          --count;
          const Fixed nextY = y + kFixed1;
          const Fixed nextLeft = left + dLeft;
          const Fixed nextRite = rite + dRite;
          BlitTrapezoid(blitter, y >> 16, left & kSnapMask, rite & kSnapMask, nextLeft & kSnapMask,
                        nextRite & kSnapMask, leftE->dy, riteE->dy, 0xff, false);
          blitter.FlushIfYChanged(y, nextY);
          left = nextLeft;
          rite = nextRite;
          y = nextY;
        }
      }
      const Fixed dY = localBot - y;
      const Fixed nextLeft = MaxF(left + FixedMul(dLeft, dY), leftBound + kSnapHalf);
      const Fixed nextRite = MinF(rite + FixedMul(dRite, dY), rightBound + kSnapHalf);
      BlitTrapezoid(blitter, y >> 16, left & kSnapMask, rite & kSnapMask, nextLeft & kSnapMask,
                    nextRite & kSnapMask, leftE->dy, riteE->dy, GetPartialAlpha(dY), false);
      blitter.FlushIfYChanged(y, localBot);
      left = nextLeft;
      rite = nextRite;
      y = localBot;
      left -= kSnapHalf;
      rite -= kSnapHalf;
    }
    leftE->x = left;
    riteE->x = rite;
    leftE->y = riteE->y = y;
  }
}
}  // namespace ruyijs::web_canvas::raster
