/* Disabled for the ghostfox standalone build: the forced-hidden pragma
 * collides with the JS_PUBLIC_API (default visibility) declarations in the
 * js/public headers, and a static js binary needs no symbol visibility. */
