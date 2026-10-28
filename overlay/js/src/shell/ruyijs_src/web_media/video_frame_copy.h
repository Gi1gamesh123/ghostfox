#pragma once
#include "web_media/image_decoder.h"
namespace ruyijs::web_media {
struct FrameLayout { uint32_t x=0,y=0,width=0,height=0,offset=0,stride=0,size=0;std::string format; };
bool ReadFrameLayout(JSContext*,JS::HandleObject,JS::HandleValue,bool,FrameLayout*);
bool FrameCopyBytes(JSContext*,JS::HandleObject,JS::HandleValue,const FrameLayout&);
}
