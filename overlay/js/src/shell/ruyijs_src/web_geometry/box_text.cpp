#include "web_geometry/box_input.h"
#include "domain/styling/css_numeric_value.h"
#include "web_geometry/font_config.h"
#include <cmath>
#include <cstdlib>
#include <string>

namespace ruyijs::web_geometry {
bool BoxText(State& s, const document_model::LayoutNodeSource& source, box::BoxNode* node) {
  auto text=source.data;
  const auto space=node->style.Get("white-space");
  if(space!="pre" && space!="pre-wrap" && space!="break-spaces") {
    text.clear(); bool previous_space=true;
    for(const auto ch:source.data) {
      const bool current_space=ch==u' '||ch==u'\t'||ch==u'\r'||ch==u'\n'||ch==u'\f';
      if(!current_space || !previous_space) text+=current_space?u' ':ch;
      previous_space=current_space;
    }
    if(!text.empty()&&text.back()==u' ') text.pop_back();
  }
  if(text.empty()) return true;
  node->breakable=space!="nowrap" && space!="pre" && text.find(u' ')!=text.npos;
  FontInput font;
  font.family=node->style.Get("font-family"); font.size=node->style.font_size;
  font.italic=node->style.Get("font-style")!="normal";
  const auto weight=node->style.Get("font-weight");
  if(!weight.empty()) font.weight=unsigned(std::strtoul(weight.c_str(),nullptr,10));
  // System fonts do not change while the font configuration generation holds.
  std::string key=std::to_string(FontConfigGeneration())+'\x1f'+font.family+'\x1f'+std::to_string(font.size)+
      '\x1f'+std::to_string(font.weight)+(font.italic?"i":"n");
  key.append(reinterpret_cast<const char*>(text.data()),text.size()*sizeof(char16_t));
  auto hit=s.textMeasures.find(key);
  if(hit==s.textMeasures.end()) {
    ruyijs::domain::geometry::SvgTextMeasurement measured;
    if(!s.fonts.Measure(text,font,&measured))
      return WebKit::Fail(s.cx,"Element text geometry exceeds supported font resources");
    double content=0;
    State::TextMeasure entry;
    if(!s.fonts.LineMetrics(font,&content,&entry.normal))
      return WebKit::Fail(s.cx,"Element text geometry exceeds supported font resources");
    // An inline box is the primary font's content area: rounded ascent plus
    // descent (nsFontMetrics), not the run's glyph bounds.
    entry.width=measured.length; entry.height=content>0 ? content : measured.box[3];
    if(s.textMeasures.size()>=8192) s.textMeasures.clear();
    hit=s.textMeasures.emplace(std::move(key),entry).first;
  }
  const double normal=hit->second.normal;
  node->text_width=std::round(hit->second.width*60)/60;
  node->text_height=hit->second.height;
  const auto line=box::CssNumericResolve(node->style.Get("line-height"),font.size,node->style.root_font_size,font.size);
  node->line_height=line.kind==box::CssNumber ? line.number*font.size : line.kind ? line.number : normal;
  return true;
}
}
