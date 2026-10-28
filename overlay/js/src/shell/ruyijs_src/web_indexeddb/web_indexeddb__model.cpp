#include "web_indexeddb/model.h"
#include "web_indexeddb/model_impl.h"
#include "domain/browser_environment/url_value.h"
namespace ruyijs::web_indexeddb {
bool OriginOf(const std::string& url, std::string* origin) noexcept {
  try {
    *origin=ruyijs::domain::browser_environment::UrlValue(url).origin();
    return true;
  } catch (...) { return false; }
}
Model::Model() : impl_(std::make_unique<Impl>()) {}
Model::~Model() = default;
static Result Convert(domain::indexed_database::Result r) {
  return {static_cast<Code>(r.code), r.oldVersion, r.version, r.connection};
}
Result Model::Inspect(const Key& key, uint64_t version, bool remove) const noexcept {
  return Convert(impl_->catalog.Inspect(key, version, remove));
}
Result Model::Apply(const Key& key, uint64_t version, bool remove) noexcept {
  try { return Convert(impl_->catalog.Apply(key, version, remove)); }
  catch (...) { return {Code::Memory}; }
}
bool Model::Close(uint32_t id) noexcept { return impl_->catalog.Close(id); }
void Model::Revert(uint32_t id) noexcept { try { impl_->catalog.Revert(id); } catch (...) {} }
void Model::Settle(uint32_t id) noexcept { impl_->catalog.Settle(id); }
bool Model::Active(uint32_t id) const noexcept { return impl_->catalog.Active(id); }
bool Model::List(const std::string& origin, std::vector<Info>* out) const noexcept {
  try {
    for (const auto& row : impl_->catalog.List(origin)) out->push_back({row.name, row.version});
    return true;
  } catch (...) { return false; }
}
bool Model::CreateStore(const Key& key,const std::u16string& name,const std::u16string& path,bool autoIncrement) noexcept {
  try{return impl_->catalog.CreateStore(key,name,path,autoIncrement);}catch(...){return false;}
}
bool Model::AutoIncrement(const Key& key,const std::u16string& name,bool* value) const noexcept {
  try{*value=impl_->catalog.Stores(key).at(name).autoIncrement;return true;}catch(...){return false;}
}
bool Model::NextKey(uint32_t id,double* key) noexcept {
  try{return impl_->catalog.NextKey(id,key);}catch(...){return false;}
}
void Model::Observe(uint32_t id,double key) noexcept {
  try{impl_->catalog.Observe(id,key);}catch(...){}
}
bool Model::Stores(const Key& key,std::vector<std::u16string>* names) const noexcept {
  try{for(const auto& row:impl_->catalog.Stores(key))names->push_back(row.first);return true;}
  catch(...){return false;}
}
bool Model::Path(const Key& key,const std::u16string& name,std::u16string* path) const noexcept {
  try{*path=impl_->catalog.Stores(key).at(name).keyPath;return true;}catch(...){return false;}
}
uint32_t Model::Begin(const Key& key,const std::u16string& name,bool write) noexcept {
  try{return impl_->catalog.Begin(key,name,write);}catch(...){return 0;}
}
bool Model::Put(uint32_t id,const std::u16string& key,const std::vector<uint8_t>& data) noexcept {
  try{return impl_->catalog.Put(id,key,data);}catch(...){return false;}
}
bool Model::Get(uint32_t id,const std::u16string& key,std::vector<uint8_t>* data,bool* found) const noexcept {
  try{*found=impl_->catalog.Get(id,key,data);return true;}catch(...){return false;}
}
bool Model::End(uint32_t id,bool commit) noexcept {
  try{return impl_->catalog.End(id,commit);}catch(...){return false;}
}
bool Model::EndAll(const std::vector<uint32_t>& ids,bool commit) noexcept {
  try{return impl_->catalog.EndAll(ids,commit);}catch(...){return false;}
}
bool Model::Select(uint32_t id,const Query& query,std::vector<Record>* rows) const noexcept {
  try{*rows=impl_->catalog.Select(id,query);return true;}catch(...){return false;}
}
}
