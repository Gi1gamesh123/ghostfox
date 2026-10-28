#ifndef RUYIJS_DOMAIN_INDEXED_DATABASE_RECORDS_H
#define RUYIJS_DOMAIN_INDEXED_DATABASE_RECORDS_H
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
namespace ruyijs::domain::indexed_database {
using Key = std::pair<std::string, std::u16string>;
using Bytes = std::vector<uint8_t>;
// One index: key path(s), flags and the ordered (index key, primary key) entries.
struct Index {
  std::vector<std::u16string> paths;  // one entry for a string key path
  bool array=false, unique=false, multiEntry=false;
  std::set<std::pair<std::u16string,std::u16string>> entries;
};
struct ObjectStore {
  std::u16string keyPath;
  // Key generator (IndexedDB "current number"); present only for autoIncrement.
  bool autoIncrement=false;
  double current=1;
  std::map<std::u16string,Bytes> records;
  std::map<std::u16string,Index> indexes;
};
struct Database { uint64_t version=0; std::map<std::u16string,ObjectStore> stores; };
struct Transaction {
  Key database;
  std::u16string store;
  bool writable=false;
  std::map<std::u16string,Bytes> writes;
  // Index keys of each written record, removed records and a pending clear.
  std::map<std::u16string,std::map<std::u16string,std::vector<std::u16string>>> indexKeys;
  std::set<std::u16string> deletes;
  bool cleared=false;
  // The store's key generator as this transaction changed it; applied on
  // commit, dropped on abort (the generator reverts with the transaction).
  bool generatorChanged=false;
  double generator=1;
};
}
#endif
