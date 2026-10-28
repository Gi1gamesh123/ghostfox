#ifndef RUYIJS_DOMAIN_WEB_MEDIA_CONSTRAINTS_H
#define RUYIJS_DOMAIN_WEB_MEDIA_CONSTRAINTS_H
namespace ruyijs::domain::web_media {
enum class ConstraintKind { LongRange, DoubleRange, BooleanRange, StringRange,
                            LongLong, Boolean, String };
struct ConstraintField { const char* name; ConstraintKind kind; };
// Firefox MediaTrackConstraintSet dictionary order. The default Firefox profile
// enables resizeMode; every listed field is supported. Unsupported WebIDL fields
// such as aspectRatio/sampleRate/volume are absent from the returned dictionary.
inline constexpr ConstraintField kConstraintFields[] = {
  {"autoGainControl", ConstraintKind::BooleanRange},
  {"browserWindow", ConstraintKind::LongLong},
  {"channelCount", ConstraintKind::LongRange},
  {"deviceId", ConstraintKind::StringRange},
  {"echoCancellation", ConstraintKind::BooleanRange},
  {"facingMode", ConstraintKind::StringRange},
  {"frameRate", ConstraintKind::DoubleRange},
  {"groupId", ConstraintKind::StringRange},
  {"height", ConstraintKind::LongRange},
  {"mediaSource", ConstraintKind::String},
  {"noiseSuppression", ConstraintKind::BooleanRange},
  {"resizeMode", ConstraintKind::StringRange},
  {"scrollWithPage", ConstraintKind::Boolean},
  {"viewportHeight", ConstraintKind::LongRange},
  {"viewportOffsetX", ConstraintKind::LongRange},
  {"viewportOffsetY", ConstraintKind::LongRange},
  {"viewportWidth", ConstraintKind::LongRange},
  {"width", ConstraintKind::LongRange},
};
}
#endif
