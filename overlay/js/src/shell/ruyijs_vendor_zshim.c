/* Symbol aliases: web_platform/compression_zlib.cpp declares the zlib
 * entry points under the RUYIJS_CS_Z_ prefix; the shell links Firefox's
 * own zlib (modules/zlib, MOZ_Z_ symbol prefix), so these shims bridge
 * the two. */
#include "compression-native/zlib-headers/zlib.h"

#ifdef _WIN32
#ifndef MOZ_Z_deflate
#define MOZ_Z_deflate deflate
#define MOZ_Z_inflate inflate
#define MOZ_Z_deflateInit2_ deflateInit2_
#define MOZ_Z_inflateInit2_ inflateInit2_
#define MOZ_Z_deflateEnd deflateEnd
#define MOZ_Z_inflateEnd inflateEnd
#endif
#else
/* POSIX links the system zlib (unprefixed symbols). The include paths pull a
 * zconf that renames the plain names to MOZ_Z_*; drop both directions so the
 * shim calls deflate/inflate for real. */
#undef deflate
#undef inflate
#undef deflateInit2_
#undef inflateInit2_
#undef deflateEnd
#undef inflateEnd
#undef MOZ_Z_deflate
#undef MOZ_Z_inflate
#undef MOZ_Z_deflateInit2_
#undef MOZ_Z_inflateInit2_
#undef MOZ_Z_deflateEnd
#undef MOZ_Z_inflateEnd
#define MOZ_Z_deflate deflate
#define MOZ_Z_inflate inflate
#define MOZ_Z_deflateInit2_ deflateInit2_
#define MOZ_Z_inflateInit2_ inflateInit2_
#define MOZ_Z_deflateEnd deflateEnd
#define MOZ_Z_inflateEnd inflateEnd
/* The zconf renamed even zlib.h's prototypes; re-declare the plain ABI. */
extern int deflateInit2_(z_streamp, int, int, int, int, int, const char*, int);
extern int inflateInit2_(z_streamp, int, const char*, int);
extern int deflate(z_streamp, int);
extern int inflate(z_streamp, int);
extern int deflateEnd(z_streamp);
extern int inflateEnd(z_streamp);
#endif

int RUYIJS_CS_Z_deflateInit2_(z_streamp strm, int level, int method,
                              int windowBits, int memLevel, int strategy,
                              const char* version, int stream_size) {
  return MOZ_Z_deflateInit2_(strm, level, method, windowBits, memLevel,
                             strategy, version, stream_size);
}
int RUYIJS_CS_Z_inflateInit2_(z_streamp strm, int windowBits,
                              const char* version, int stream_size) {
  return MOZ_Z_inflateInit2_(strm, windowBits, version, stream_size);
}
int RUYIJS_CS_Z_deflate(z_streamp strm, int flush) {
  return MOZ_Z_deflate(strm, flush);
}
int RUYIJS_CS_Z_inflate(z_streamp strm, int flush) {
  return MOZ_Z_inflate(strm, flush);
}
int RUYIJS_CS_Z_deflateEnd(z_streamp strm) {
  return MOZ_Z_deflateEnd(strm);
}
int RUYIJS_CS_Z_inflateEnd(z_streamp strm) {
  return MOZ_Z_inflateEnd(strm);
}
