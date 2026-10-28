// IndexedDB indexes, deletes and cursor rows over the one Catalog
// (docs/owners/INDEXED_DB.md, 2026-10-21). Engine-free.
#include "web_indexeddb/model.h"
#include "web_indexeddb/model_impl.h"
namespace ruyijs::web_indexeddb {
namespace idb = ruyijs::domain::indexed_database;
WriteCode Model::Write(uint32_t id,const std::u16string& key,const std::vector<uint8_t>& bytes,
                       const IndexKeys& keys,bool noOverwrite) noexcept {
  try { return static_cast<WriteCode>(impl_->catalog.Write(id,key,bytes,keys,noOverwrite)); }
  catch (...) { return WriteCode::Quota; }
}
bool Model::Delete(uint32_t id,const idb::KeyRange& range,bool all) noexcept {
  try { return impl_->catalog.Delete(id,range,all); } catch (...) { return false; }
}
bool Model::Indexes(const Key& key,const std::u16string& store,std::vector<IndexInfo>* out) const noexcept {
  try {
    out->clear();
    for (const auto& [name,index]:impl_->catalog.Stores(key).at(store).indexes)
      out->push_back({name,index.paths,index.array,index.unique,index.multiEntry});
    return true;
  } catch (...) { return false; }
}
bool Model::Records(const Key& key,const std::u16string& store,std::vector<Record>* out) const noexcept {
  try {
    out->clear();
    for (const auto& row:impl_->catalog.Stores(key).at(store).records) out->push_back(row);
    return true;
  } catch (...) { return false; }
}
bool Model::CreateIndex(const Key& key,const std::u16string& store,const IndexInfo& info,
                        const std::vector<std::pair<std::u16string,std::u16string>>& entries) noexcept {
  try {
    idb::Index index;
    index.paths=info.paths;index.array=info.array;index.unique=info.unique;index.multiEntry=info.multiEntry;
    index.entries.insert(entries.begin(),entries.end());
    return impl_->catalog.CreateIndex(key,store,info.name,std::move(index));
  } catch (...) { return false; }
}
bool Model::DeleteIndex(const Key& key,const std::u16string& store,const std::u16string& name) noexcept {
  try { return impl_->catalog.DeleteIndex(key,store,name); } catch (...) { return false; }
}
bool Model::RenameStore(const Key& key,const std::u16string& from,const std::u16string& to) noexcept {
  try { return impl_->catalog.RenameStore(key,from,to); } catch (...) { return false; }
}
bool Model::RenameIndex(const Key& key,const std::u16string& store,const std::u16string& from,
                        const std::u16string& to) noexcept {
  try { return impl_->catalog.RenameIndex(key,store,from,to); } catch (...) { return false; }
}
bool Model::Rows(uint32_t id,const std::u16string* index,const idb::KeyRange& range,Direction direction,
                 std::vector<CursorRow>* out) const noexcept {
  try {
    const auto& tx=impl_->catalog.Tx(id);
    *out=idb::CursorRows(impl_->catalog.StoreOf(tx),tx,index,range,direction);
    return true;
  } catch (...) { return false; }
}
}
