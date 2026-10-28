#pragma once
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace ruyijs::domain::geometry {
struct SvgPathCommand { char kind; std::array<float, 7> values{}; };
inline bool ParseSvgPath(const std::string& text, std::vector<SvgPathCommand>* out) {
  const char* p = text.c_str(); char command = 0, previous = 0;
  float x = 0, y = 0, startX = 0, startY = 0, controlX = 0, controlY = 0;
  auto skip = [&] { while (*p == ',' || *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p; };
  while (*p) {
    skip(); if (!*p) break;
    if (std::isalpha(static_cast<unsigned char>(*p))) command = *p++;
    const char kind = char(std::toupper(static_cast<unsigned char>(command)));
    const bool relative = command >= 'a' && command <= 'z';
    if (kind == 'Z') {
      out->push_back({'Z'}); x=startX; y=startY; previous=kind; command=0; continue;
    }
    unsigned count = kind == 'H' || kind == 'V' ? 1 : kind == 'C' ? 6 : kind == 'S' || kind == 'Q' ? 4
        : kind == 'A' ? 7 : kind == 'M' || kind == 'L' || kind == 'T' ? 2 : 0;
    if (!count || out->size() >= 65536) return false;
    std::array<float, 7> v{};
    for (unsigned i = 0; i < count; ++i) {
      skip(); char* end = nullptr;
      const double number = std::strtod(p, &end);
      if (end == p || !std::isfinite(number) || !std::isfinite(float(number))) return false;
      v[i] = float(number); p=end;
    }
    if (kind == 'H') v={relative ? x+v[0] : v[0], y};
    else if (kind == 'V') v={x, relative ? y+v[0] : v[0]};
    else if (relative) {
      if (kind == 'A') { v[5]+=x; v[6]+=y; }
      else for (unsigned i = 0; i < count; i+=2) { v[i]+=x; v[i+1]+=y; }
    }
    if (kind == 'M') { startX=v[0]; startY=v[1]; out->push_back({'M',v}); command=relative?'l':'L'; }
    else if (kind == 'C') { out->push_back({'C',v}); controlX=v[2]; controlY=v[3]; }
    else if (kind == 'S') {
      const float cx=previous=='C'||previous=='S'?2*x-controlX:x;
      const float cy=previous=='C'||previous=='S'?2*y-controlY:y;
      out->push_back({'C',{cx,cy,v[0],v[1],v[2],v[3]}}); controlX=v[0]; controlY=v[1];
    } else if (kind == 'Q' || kind == 'T') {
      const float cx=kind=='Q'?v[0]:(previous=='Q'||previous=='T'?2*x-controlX:x);
      const float cy=kind=='Q'?v[1]:(previous=='Q'||previous=='T'?2*y-controlY:y);
      const float ex=kind=='Q'?v[2]:v[0], ey=kind=='Q'?v[3]:v[1];
      out->push_back({'Q',{cx,cy,ex,ey}}); controlX=cx; controlY=cy;
    } else if (kind == 'A') out->push_back({'A',v});
    else out->push_back({'L',v});
    if (kind=='A') { x=v[5]; y=v[6]; }
    else if (kind=='C') { x=v[4]; y=v[5]; }
    else if (kind=='S'||kind=='Q') { x=v[2]; y=v[3]; }
    else { x=v[0]; y=v[1]; }
    previous=kind;
  }
  return out->empty() || out->front().kind == 'M';
}
}
