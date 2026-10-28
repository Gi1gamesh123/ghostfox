#ifndef RUYIJS_WEB_MEDIA_TEXT_TRACK_H
#define RUYIJS_WEB_MEDIA_TEXT_TRACK_H

#include "runtime/web_kit.h"

namespace ruyijs::web_media {
struct State;

extern const JSClass kTextTrackClass, kTextTrackCueClass, kTextTrackCueListClass,
    kTextTrackListClass, kVTTCueClass, kVTTRegionClass;

enum TextTrackSlot {
  TrackKind, TrackLabel, TrackLanguage, TrackId, TrackCues, TrackActiveCues,
  TrackDispatchType, TrackMode, TrackCueChange, TrackOwner, TrackSlotCount
};
enum TextCueSlot {
  CueTrack, CueId, CueStart, CueEnd, CuePause, CueEnter, CueExit,
  CueOwner, CueSlotCount
};
enum CueListSlot { CueListItems, CueListOwner, CueListSlotCount };
enum TrackListSlot {
  TrackListItems, TrackListChange, TrackListAdd, TrackListRemove,
  TrackListOwner, TrackListSlotCount
};
enum VttCueSlot {
  VttRegion = CueSlotCount, VttVertical, VttSnap, VttLine, VttLineAlign, VttPosition,
  VttPositionAlign, VttSize, VttAlign, VttText, VttOwner, VttCueSlotCount
};

// VTTRegion attributes, in Firefox's prototype order (vtt_region.cpp).
enum VttRegionSlot {
  RegionId, RegionWidth, RegionLines, RegionAnchorX, RegionAnchorY, RegionViewportX,
  RegionViewportY, RegionScroll, RegionSlotCount
};
bool InstallTextTracks(State&);
bool InstallVTTRegion(State&);
bool IsVTTRegion(JSObject* object);
bool TextTrackConstructor(JSContext*, unsigned, JS::Value*);
bool TextTrackMember(JSContext*, unsigned, JS::Value*);
bool TextTrackSet(JSContext*, unsigned, JS::Value*);
bool TextCueConstructor(JSContext*, unsigned, JS::Value*);
bool TextCueMember(JSContext*, unsigned, JS::Value*);
bool TextCueSet(JSContext*, unsigned, JS::Value*);
bool CueListMember(JSContext*, unsigned, JS::Value*);
bool TrackListMember(JSContext*, unsigned, JS::Value*);
bool TrackListSet(JSContext*, unsigned, JS::Value*);
bool TextTrackListIterator(JSContext*, unsigned, JS::Value*);
bool TextCueListIterator(JSContext*, unsigned, JS::Value*);
bool VTTCueConstructor(JSContext*, unsigned, JS::Value*);
bool VTTCueMember(JSContext*, unsigned, JS::Value*);
bool VTTCueSet(JSContext*, unsigned, JS::Value*);

JSObject* NewTextTrack(State&, const char*, const char*, const char*);
JSObject* NewTextTrackList(State&);

}  // namespace ruyijs::web_media
#endif
