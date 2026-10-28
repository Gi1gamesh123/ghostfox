#ifndef RUYIJS_INDEXED_DATABASE_MODEL_H
#define RUYIJS_INDEXED_DATABASE_MODEL_H
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "domain/indexed_database/record_key.h"
#include "domain/indexed_database/index_view.h"
#include <map>
#include <optional>
namespace ruyijs::web_indexeddb {
using Key = std::pair<std::string, std::u16string>;
using Query = domain::indexed_database::Query;
using Record = std::pair<std::u16string,std::vector<uint8_t>>;
enum class Code { Ok, Version, Blocked, Quota, Memory };
struct Result { Code code = Code::Ok; uint64_t oldVersion = 0, version = 0; uint32_t connection = 0; };
struct Info { std::u16string name; uint64_t version = 0; };
using Direction = domain::indexed_database::Direction;
using CursorRow = domain::indexed_database::CursorRow;
using CursorPosition = domain::indexed_database::CursorPosition;
using CursorStep = domain::indexed_database::CursorStep;
using IndexKeys = std::map<std::u16string,std::vector<std::u16string>>;
enum class WriteCode { Ok, Quota, Constraint, Exists };
struct IndexInfo { std::u16string name; std::vector<std::u16string> paths; bool array=false, unique=false, multiEntry=false; };
bool OriginOf(const std::string& url, std::string* origin) noexcept;
class Model {
 public:
  Model();
  ~Model();
  Result Inspect(const Key&, uint64_t, bool) const noexcept;
  Result Apply(const Key&, uint64_t, bool) noexcept;
  bool Close(uint32_t) noexcept;
  // The end of an upgrade: Revert restores the database it replaced, Settle keeps it.
  void Revert(uint32_t) noexcept;
  void Settle(uint32_t) noexcept;
  bool Active(uint32_t) const noexcept;
  bool List(const std::string&, std::vector<Info>*) const noexcept;
  bool CreateStore(const Key&,const std::u16string&,const std::u16string&,bool autoIncrement=false) noexcept;
  bool AutoIncrement(const Key&,const std::u16string&,bool*) const noexcept;
  // The transaction store's key generator (catalog.h NextKey / Observe).
  bool NextKey(uint32_t,double*) noexcept;
  void Observe(uint32_t,double) noexcept;
  bool Stores(const Key&,std::vector<std::u16string>*) const noexcept;
  bool Path(const Key&,const std::u16string&,std::u16string*) const noexcept;
  uint32_t Begin(const Key&,const std::u16string&,bool) noexcept;
  bool Put(uint32_t,const std::u16string&,const std::vector<uint8_t>&) noexcept;
  bool Get(uint32_t,const std::u16string&,std::vector<uint8_t>*,bool*) const noexcept;
  bool Select(uint32_t,const Query&,std::vector<Record>*) const noexcept;
  bool End(uint32_t,bool) noexcept;
  bool EndAll(const std::vector<uint32_t>&,bool) noexcept;
  // Indexes, deletes and cursors (model_index.cpp).
  WriteCode Write(uint32_t,const std::u16string&,const std::vector<uint8_t>&,const IndexKeys&,bool noOverwrite) noexcept;
  bool Delete(uint32_t,const domain::indexed_database::KeyRange&,bool all) noexcept;
  bool Indexes(const Key&,const std::u16string& store,std::vector<IndexInfo>*) const noexcept;
  bool Records(const Key&,const std::u16string& store,std::vector<Record>*) const noexcept;
  bool CreateIndex(const Key&,const std::u16string& store,const IndexInfo&,
                   const std::vector<std::pair<std::u16string,std::u16string>>& entries) noexcept;
  bool DeleteIndex(const Key&,const std::u16string& store,const std::u16string& name) noexcept;
  bool RenameStore(const Key&,const std::u16string& from,const std::u16string& to) noexcept;
  bool RenameIndex(const Key&,const std::u16string& store,const std::u16string& from,const std::u16string& to) noexcept;
  // Rows of a store (index null) or index in a range and direction; Advance picks the next one.
  bool Rows(uint32_t,const std::u16string* index,const domain::indexed_database::KeyRange&,Direction,
            std::vector<CursorRow>*) const noexcept;
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}
#endif
