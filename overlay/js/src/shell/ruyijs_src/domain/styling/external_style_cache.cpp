#include "domain/styling/external_style_cache.h"
#include <algorithm>
namespace ruyijs::domain::styling {
namespace {
bool Same(const ExternalStyleResource& a,const std::string& url,const std::string& document,
            const std::string& origin,const std::string& mode) {
  return a.url==url && a.documentURL==document && a.origin==origin && a.mode==mode;
}
size_t Charge(const ExternalStyleResource& value) {
  return value.text.size()+value.url.size()+value.documentURL.size()+value.origin.size()+value.mode.size();
}
}
std::shared_ptr<const ExternalStyleResource> ExternalStyleCache::Find(const std::string& url,
    const std::string& document,const std::string& origin,const std::string& mode) {
  for (auto it=values_.begin();it!=values_.end();++it) {
    if (!Same(**it,url,document,origin,mode)) continue;
    auto result=*it;
    std::rotate(it,it+1,values_.end());
    return result;
  }
  return {};
}
void ExternalStyleCache::Put(std::shared_ptr<const ExternalStyleResource> value) {
  if (!value || !value->success || !entryLimit_) return;
  const size_t charge=Charge(*value);
  if (charge>byteLimit_) return;
  for (auto it=values_.begin();it!=values_.end();) {
    if (!Same(**it,value->url,value->documentURL,value->origin,value->mode)) { ++it;continue; }
    bytes_-=Charge(**it);it=values_.erase(it);
  }
  while (!values_.empty() && (values_.size()>=entryLimit_ || charge>byteLimit_-bytes_)) {
    bytes_-=Charge(*values_.front());values_.erase(values_.begin());
  }
  values_.push_back(std::move(value));bytes_+=charge;
}
}
