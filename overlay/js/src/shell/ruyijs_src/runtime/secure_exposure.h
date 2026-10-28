#ifndef RUYIJS_RUNTIME_SECURE_EXPOSURE_H
#define RUYIJS_RUNTIME_SECURE_EXPOSURE_H
namespace ruyijs {
struct BrowsingContext;
// The one place [SecureContext] exposure is applied for a realm: after its
// installers ran, when a profile configures the URL, and when a navigation
// commits. Each owner removes or restores its own members (Crypto, Geolocation,
// Web Locks, Navigator media/protocol members); the Navigator key order is
// then restored by its owner.
bool ConfigureSecureExposure(BrowsingContext& context);
}
#endif
