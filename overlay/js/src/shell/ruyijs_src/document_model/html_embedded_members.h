#pragma once

namespace ruyijs::document_model::embedded {
enum Kind { Text, Plain, Boolean, Unsigned, Loop, Dormant };
struct Member { const char* iface; const char* name; const char16_t* attr; Kind kind; bool set = true; };
inline const Member kMembers[] = {
  {"HTMLEmbedElement","src",u"",Dormant}, {"HTMLEmbedElement","type",u"",Dormant},
  {"HTMLEmbedElement","width",u"",Dormant}, {"HTMLEmbedElement","height",u"",Dormant},
  {"HTMLEmbedElement","align",u"",Dormant}, {"HTMLEmbedElement","name",u"",Dormant},
  {"HTMLFrameElement","name",u"",Dormant}, {"HTMLFrameElement","scrolling",u"",Dormant},
  {"HTMLFrameElement","src",u"",Dormant}, {"HTMLFrameElement","frameBorder",u"frameborder",Text},
  {"HTMLFrameElement","longDesc",u"",Dormant}, {"HTMLFrameElement","noResize",u"",Dormant},
  {"HTMLFrameElement","marginHeight",u"",Dormant}, {"HTMLFrameElement","marginWidth",u"",Dormant},
  {"HTMLFrameElement","contentDocument",u"",Dormant,false}, {"HTMLFrameElement","contentWindow",u"",Dormant,false},
  {"HTMLMarqueeElement","behavior",u"behavior",Plain}, {"HTMLMarqueeElement","bgColor",u"bgcolor",Plain},
  {"HTMLMarqueeElement","direction",u"direction",Plain}, {"HTMLMarqueeElement","height",u"height",Plain},
  {"HTMLMarqueeElement","hspace",u"hspace",Unsigned}, {"HTMLMarqueeElement","loop",u"loop",Loop},
  {"HTMLMarqueeElement","scrollAmount",u"scrollamount",Unsigned}, {"HTMLMarqueeElement","scrollDelay",u"scrolldelay",Unsigned},
  {"HTMLMarqueeElement","trueSpeed",u"truespeed",Boolean}, {"HTMLMarqueeElement","vspace",u"vspace",Unsigned},
  {"HTMLMarqueeElement","width",u"width",Plain},
  {"HTMLSourceElement","src",u"src",Text}, {"HTMLSourceElement","type",u"type",Text},
  {"HTMLSourceElement","srcset",u"srcset",Text}, {"HTMLSourceElement","sizes",u"sizes",Text},
  {"HTMLSourceElement","media",u"media",Text}, {"HTMLSourceElement","width",u"width",Unsigned},
  {"HTMLSourceElement","height",u"height",Unsigned},
  {"HTMLTrackElement","src",u"",Dormant}, {"HTMLTrackElement","srclang",u"",Dormant},
  {"HTMLTrackElement","label",u"",Dormant}, {"HTMLTrackElement","kind",u"",Dormant},
  {"HTMLTrackElement","default",u"",Dormant}, {"HTMLTrackElement","readyState",u"",Dormant,false}
};
}
