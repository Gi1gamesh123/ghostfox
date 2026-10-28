#ifndef RUYIJS_WEB_MEDIA_AUDIO_DATA_H
#define RUYIJS_WEB_MEDIA_AUDIO_DATA_H
#include "js/Class.h"
namespace ruyijs::web_media {
struct State;
extern const JSClass kAudioDataClass;
// WebCodecs AudioData (audio_data.cpp), installed with VideoFrame on Window and DedicatedWorker.
bool InstallAudioData(State&);
}
#endif
