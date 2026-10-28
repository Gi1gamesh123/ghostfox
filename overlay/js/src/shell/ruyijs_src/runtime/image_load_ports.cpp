#include "runtime/image_load_ports.h"
#include "runtime/browsing_context.h"
#include "resource_network/async_http.h"
#include "document_model/realm.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string_view>

namespace ruyijs {
namespace {
uint32_t BigEndian32(const unsigned char* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
         (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}
uint16_t LittleEndian16(const unsigned char* p) {
  return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}
void ImageSize(const std::string& body, uint32_t* width, uint32_t* height) {
  const auto* p = reinterpret_cast<const unsigned char*>(body.data());
  const size_t n = body.size();
  if (n >= 24 && std::memcmp(p, "\x89PNG\r\n\x1a\n", 8) == 0 &&
      std::memcmp(p + 12, "IHDR", 4) == 0) {
    *width = BigEndian32(p + 16); *height = BigEndian32(p + 20); return;
  }
  if (n >= 10 && (!std::memcmp(p, "GIF87a", 6) || !std::memcmp(p, "GIF89a", 6))) {
    *width = LittleEndian16(p + 6); *height = LittleEndian16(p + 8); return;
  }
  if (n >= 30 && std::memcmp(p, "RIFF", 4) == 0 && std::memcmp(p + 8, "WEBP", 4) == 0) {
    if (std::memcmp(p + 12, "VP8X", 4) == 0 && n >= 30) {
      *width = 1 + (uint32_t(p[24]) | (uint32_t(p[25]) << 8) |
                    (uint32_t(p[26]) << 16));
      *height = 1 + (uint32_t(p[27]) | (uint32_t(p[28]) << 8) |
                     (uint32_t(p[29]) << 16));
    } else if (std::memcmp(p + 12, "VP8 ", 4) == 0 && n >= 30 && p[23] == 0x9d && p[24] == 0x01 && p[25] == 0x2a) {
      *width = LittleEndian16(p + 26) & 0x3fff; *height = LittleEndian16(p + 28) & 0x3fff;
    }
    return;
  }
  // Cloudflare's challenge image endpoint may return an SVG with an
  // explicit intrinsic size. Firefox exposes that size through
  // naturalWidth/naturalHeight even though there is no raster header.
  const std::string_view text(body);
  const size_t svg = text.find("<svg");
  if (svg != std::string_view::npos) {
    auto read = [&](std::string_view name, uint32_t* out) {
      const size_t start = text.find(name, svg + 4);
      if (start == std::string_view::npos) return;
      size_t at = start + name.size();
      while (at < text.size() && (text[at] == ' ' || text[at] == '\t' ||
                                   text[at] == '\r' || text[at] == '\n')) ++at;
      if (at >= text.size() || text[at] != '=') return;
      ++at;
      while (at < text.size() && (text[at] == ' ' || text[at] == '\t' ||
                                   text[at] == '\r' || text[at] == '\n')) ++at;
      if (at < text.size() && (text[at] == '\"' || text[at] == '\'')) ++at;
      char* end = nullptr;
      const unsigned long value = std::strtoul(text.data() + at, &end, 10);
      if (end != text.data() + at && value <= UINT32_MAX) *out = uint32_t(value);
    };
    read("width", width);
    read("height", height);
    if (*width && *height) return;
  }
  if (n < 4 || p[0] != 0xff || p[1] != 0xd8) return;
  size_t i = 2;
  while (i + 9 < n) {
    while (i < n && p[i] == 0xff) ++i;
    if (i >= n) break;
    const unsigned marker = p[i++];
    if (marker == 0xd8 || marker == 0xd9) continue;
    if (i + 2 > n) break;
    const size_t length = (size_t(p[i]) << 8) | p[i + 1];
    if (length < 7 || i + length > n) break;
    const bool sof = (marker >= 0xc0 && marker <= 0xc3) ||
                     (marker >= 0xc5 && marker <= 0xc7) ||
                     (marker >= 0xc9 && marker <= 0xcb) ||
                     (marker >= 0xcd && marker <= 0xcf);
    if (sof) { *height = (uint32_t(p[i + 3]) << 8) | p[i + 4]; *width = (uint32_t(p[i + 5]) << 8) | p[i + 6]; return; }
    i += length;
  }
}
void Loaded(const std::string& url, bool ok, const resource_network::HttpResult& result,
            document_model::ImageLoadOutcome* outcome) {
  outcome->loaded = ok && result.status >= 200 && result.status < 300;
  if (outcome->loaded) {
    ImageSize(result.body, &outcome->width, &outcome->height);
    outcome->bytes = result.body;
    for (const auto& field : result.headers) {
      std::string name = field.first;
      for (char& c : name) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
      if (name == "content-type") {
        outcome->mime = field.second;
        const size_t semi = outcome->mime.find(';');
        if (semi != std::string::npos) outcome->mime.resize(semi);
        for (char& c : outcome->mime) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        break;
      }
    }
    if (outcome->mime.empty()) outcome->mime = "application/octet-stream";
  }
}
// The fetch runs through the Context's asynchronous owner; `done` runs at a
// later task boundary on this thread with the HTTP success decision and size.
void LoadImage(void* context, const std::string& url, const std::string& referer,
               document_model::ImageLoadDone done, void* request) {
  auto& http = *static_cast<resource_network::AsyncHttp*>(context);
  resource_network::HttpOptions options;options.initiator="img";
  options.source=referer;
  http.Start("GET", url, {}, "", resource_network::RedirectMode::Follow, options,
             [url, done, request](bool ok, resource_network::HttpResult&& result) {
    document_model::ImageLoadOutcome outcome;
    Loaded(url, ok, result, &outcome);
    done(request, outcome);
  });
}
}
void BindImageLoads(BrowsingContext& owner, resource_network::AsyncHttp& http) {
  owner.document->SetImageLoadPort(&LoadImage, &http);
}
}
