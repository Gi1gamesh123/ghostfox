/* Wrapper: compile brotli's common/context.c here so the file keeps its
 * basename-independent object name (ctx_alias.obj) while its quoted
 * includes resolve from this directory. */
#include "context.c"
