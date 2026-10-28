#ifndef RUYIJS_DOMAIN_INDEXED_DATABASE_CATALOG_H
#define RUYIJS_DOMAIN_INDEXED_DATABASE_CATALOG_H
#include "domain/indexed_database/records.h"
#include "domain/indexed_database/record_key.h"
#include "domain/indexed_database/index_view.h"
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>
namespace ruyijs::domain::indexed_database {
using Key = std::pair<std::string, std::u16string>;
struct Info { std::u16string name; uint64_t version = 0; };
enum class Code { Ok, Version, Blocked, Quota, Memory };
enum class WriteCode { Ok, Quota, Constraint, Exists };
struct Result { Code code = Code::Ok; uint64_t oldVersion = 0, version = 0; uint32_t connection = 0; };
// Product-authored domain, not an imported ruyijs file. No runtime dependency.
class Catalog {
 public:
  Result Inspect(const Key& key, uint64_t version, bool remove) const {
    auto it = databases_.find(key);
    uint64_t old = it == databases_.end() ? 0 : it->second.version;
    uint64_t next = remove ? 0 : (version ? version : (old ? old : 1));
    Result result{Code::Ok, old, next};
    if (!remove && next < old) result.code = Code::Version;
    else if (remove || next > old) {
      for (const auto& [id, active] : connections_)
        if (active == key) { result.code = Code::Blocked; break; }
    }
    return result;
  }
  Result Apply(const Key& key, uint64_t version, bool remove) {
    Result result = Inspect(key, version, remove);
    if (result.code != Code::Ok) return result;
    if (remove) { databases_.erase(key); return result; }
    if ((databases_.size() >= 1024 && !databases_.count(key)) || connections_.size() >= 4096) {
      result.code = Code::Quota; return result;
    }
    uint32_t id = ++serial_;
    connections_.emplace(id, key);
    try {
      // An upgrade keeps the database it replaces until its transaction ends.
      if (result.version > result.oldVersion) {
        auto it = databases_.find(key);
        upgrades_.emplace(id, std::make_pair(key, it == databases_.end() ? std::optional<Database>() : it->second));
      }
      databases_[key].version = result.version;
    } catch (...) { upgrades_.erase(id); connections_.erase(id); throw; }
    result.connection = id;
    return result;
  }
  // The upgrade of connection `id` aborted: the database returns to its state
  // before the upgrade (none for a new database). Settle keeps the upgrade.
  void Revert(uint32_t id) {
    auto it = upgrades_.find(id);
    if (it == upgrades_.end()) return;
    // The connection may already be closed (close() during the upgrade aborts it).
    auto& [key, saved] = it->second;
    if (saved) databases_[key] = std::move(*saved);
    else databases_.erase(key);
    upgrades_.erase(it);
  }
  void Settle(uint32_t id) { upgrades_.erase(id); }
  bool Close(uint32_t id) { return connections_.erase(id) != 0; }
  bool Active(uint32_t id) const { return connections_.count(id) != 0; }
  std::vector<Info> List(const std::string& origin) const {
    std::vector<Info> result;
    for (const auto& [key, database] : databases_)
      if (key.first == origin) result.push_back({key.second, database.version});
    return result;
  }
  bool CreateStore(const Key& key,const std::u16string& name,const std::u16string& path,bool autoIncrement=false) {
    auto& stores=databases_.at(key).stores;
    if (stores.size()>=64 || stores.count(name)) return false;
    ObjectStore store;store.keyPath=path;store.autoIncrement=autoIncrement;
    stores.emplace(name,std::move(store));return true;
  }
  // Key generator of the transaction's store. NextKey takes the current number
  // (false once it exceeds 2^53, the spec's ConstraintError); Observe raises it
  // past an explicitly stored number key.
  bool NextKey(uint32_t id,double* key) {
    auto& tx=transactions_.at(id);
    double current=tx.generatorChanged?tx.generator:StoreOf(tx).current;
    if(current>9007199254740992.0) return false;
    *key=current;tx.generator=current+1;tx.generatorChanged=true;return true;
  }
  void Observe(uint32_t id,double key) {
    auto& tx=transactions_.at(id);
    if(!StoreOf(tx).autoIncrement) return;
    double current=tx.generatorChanged?tx.generator:StoreOf(tx).current;
    if(key<current) return;
    double next=key>=9007199254740992.0?9007199254740993.0:static_cast<double>(static_cast<int64_t>(key))+1;
    tx.generator=next;tx.generatorChanged=true;
  }
  const std::map<std::u16string,ObjectStore>& Stores(const Key& key) const {
    return databases_.at(key).stores;
  }
  uint32_t Begin(const Key& key,const std::u16string& name,bool write) {
    if (transactions_.size()>=1024 || !Stores(key).count(name)) return 0;
    uint32_t id=++transactionSerial_;
    transactions_.emplace(id,Transaction{key,name,write,{}});return id;
  }
  // put/add with the record's index keys; unique indexes and add() reject duplicates.
  WriteCode Write(uint32_t id,const std::u16string& key,const Bytes& bytes,
                  const std::map<std::u16string,std::vector<EncodedKey>>& indexKeys,bool noOverwrite) {
    auto& tx=transactions_.at(id);
    const auto& store=StoreOf(tx);
    if(noOverwrite && VisibleRecords(store,tx).count(key)) return WriteCode::Exists;
    for(const auto& [name,keys]:indexKeys) {
      const auto& index=store.indexes.at(name);
      if(!index.unique) continue;
      const auto entries=VisibleIndex(store,tx,name);
      for(const auto& indexKey:keys) {
        auto hit=entries.lower_bound({indexKey,EncodedKey()});
        if(hit!=entries.end() && hit->first==indexKey && hit->second!=key) return WriteCode::Constraint;
      }
    }
    if(!Put(id,key,bytes)) return WriteCode::Quota;
    tx.deletes.erase(key);
    tx.indexKeys[key]=indexKeys;
    return WriteCode::Ok;
  }
  // delete(range) / clear(): staged like writes, applied on commit.
  bool Delete(uint32_t id,const KeyRange& range,bool all) {
    auto& tx=transactions_.at(id);
    if(!tx.writable) return false;
    if(all) { tx.cleared=true; tx.writes.clear(); tx.indexKeys.clear(); tx.deletes.clear(); return true; }
    for(const auto& [key,bytes]:VisibleRecords(StoreOf(tx),tx))
      if(range.Contains(key)) { tx.deletes.insert(key); tx.writes.erase(key); tx.indexKeys.erase(key); }
    return tx.deletes.size()<=65536;
  }
  const Transaction& Tx(uint32_t id) const { return transactions_.at(id); }
  const ObjectStore& StoreOf(const Transaction& tx) const { return Stores(tx.database).at(tx.store); }
  bool CreateIndex(const Key& key,const std::u16string& store,const std::u16string& name,Index index) {
    auto& indexes=databases_.at(key).stores.at(store).indexes;
    if(indexes.size()>=64 || indexes.count(name)) return false;
    indexes.emplace(name,std::move(index));return true;
  }
  bool DeleteIndex(const Key& key,const std::u16string& store,const std::u16string& name) {
    return databases_.at(key).stores.at(store).indexes.erase(name)!=0;
  }
  bool RenameStore(const Key& key,const std::u16string& from,const std::u16string& to) {
    auto& stores=databases_.at(key).stores;
    auto it=stores.find(from);
    if(it==stores.end() || stores.count(to)) return false;
    ObjectStore moved=std::move(it->second);stores.erase(it);stores.emplace(to,std::move(moved));return true;
  }
  bool RenameIndex(const Key& key,const std::u16string& store,const std::u16string& from,const std::u16string& to) {
    auto& indexes=databases_.at(key).stores.at(store).indexes;
    auto it=indexes.find(from);
    if(it==indexes.end() || indexes.count(to)) return false;
    Index moved=std::move(it->second);indexes.erase(it);indexes.emplace(to,std::move(moved));return true;
  }
  bool Put(uint32_t id,const std::u16string& key,const Bytes& bytes) {
    auto& tx=transactions_.at(id);
    if (!tx.writable || tx.writes.size()>=4096 || bytes.size()>1024*1024) return false;
    size_t staged=bytes.size();
    for(const auto& [otherId,other]:transactions_)
      for(const auto& [otherKey,value]:other.writes)
        if(otherId!=id || otherKey!=key)staged+=value.size();
    if(staged>16*1024*1024)return false;
    tx.writes[key]=bytes;return true;
  }
  bool Get(uint32_t id,const std::u16string& key,Bytes* bytes) const {
    const auto& tx=transactions_.at(id);
    auto pending=tx.writes.find(key);
    if (pending!=tx.writes.end()) {*bytes=pending->second;return true;}
    if (tx.cleared || tx.deletes.count(key)) return false;
    const auto& records=Stores(tx.database).at(tx.store).records;
    auto entry=records.find(key);if (entry==records.end()) return false;
    *bytes=entry->second;return true;
  }
  bool End(uint32_t id,bool commit) { return EndAll({id},commit); }
  // One IDBTransaction may span several stores, one catalog transaction each:
  // they commit together only if every one fits, otherwise none is applied.
  bool EndAll(const std::vector<uint32_t>& ids,bool commit) {
    for(uint32_t id:ids) if(!transactions_.count(id)) return false;
    if(commit) {
      struct Staged { uint32_t id; std::map<std::u16string,Bytes> records; std::map<std::u16string,Index> indexes; };
      std::vector<Staged> staged;
      size_t total=0;
      for(uint32_t id:ids) {
        auto& tx=transactions_.at(id);auto& target=databases_.at(tx.database).stores.at(tx.store);
        Staged next{id,VisibleRecords(target,tx),target.indexes};
        for(auto& [name,index]:next.indexes) { auto entries=VisibleIndex(target,tx,name); index.entries.swap(entries); }
        size_t size=0;for(const auto& [key,bytes]:next.records) size+=bytes.size();
        if(next.records.size()>4096 || size>8*1024*1024) { for(uint32_t other:ids) transactions_.erase(other); return false; }
        total+=size;staged.push_back(std::move(next));
      }
      for(const auto& [key,database]:databases_)
        for(const auto& [name,store]:database.stores) {
          bool scoped=false;
          for(uint32_t id:ids) { const auto& tx=transactions_.at(id); if(key==tx.database && name==tx.store) scoped=true; }
          if(!scoped) for(const auto& record:store.records) total+=record.second.size();
        }
      if(total>16*1024*1024) { for(uint32_t other:ids) transactions_.erase(other); return false; }
      for(auto& next:staged) {
        auto& tx=transactions_.at(next.id);auto& target=databases_.at(tx.database).stores.at(tx.store);
        target.records.swap(next.records);target.indexes.swap(next.indexes);
        if(tx.generatorChanged) target.current=tx.generator;
      }
    }
    for(uint32_t id:ids) transactions_.erase(id);
    return true;
  }
  std::vector<std::pair<EncodedKey,Bytes>> Select(uint32_t id,const Query& query) const {
    const auto& tx=transactions_.at(id);
    auto visible=VisibleRecords(StoreOf(tx),tx);
    std::vector<std::pair<EncodedKey,Bytes>> rows;
    auto append=[&](const auto& row) {
      if(query.range.Contains(row.first))rows.push_back(row);
      return query.count && rows.size()>=query.count;
    };
    if(query.reverse) {
      for(auto it=visible.rbegin();it!=visible.rend();++it)if(append(*it))break;
    } else for(const auto& row:visible)if(append(row))break;
    return rows;
  }
 private:
  std::map<Key, Database> databases_;
  std::map<uint32_t, Key> connections_;
  std::map<uint32_t, std::pair<Key, std::optional<Database>>> upgrades_;
  std::map<uint32_t,Transaction> transactions_;
  uint32_t serial_ = 0;
  uint32_t transactionSerial_ = 0;
};
}
#endif
