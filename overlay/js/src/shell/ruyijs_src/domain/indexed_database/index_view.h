#ifndef RUYIJS_DOMAIN_INDEXED_DATABASE_INDEX_VIEW_H
#define RUYIJS_DOMAIN_INDEXED_DATABASE_INDEX_VIEW_H
// The records and index entries a transaction sees (committed state, then its
// clear, deletes and writes) and the cursor iteration order over them.
#include "domain/indexed_database/record_key.h"
#include "domain/indexed_database/records.h"
#include <optional>
namespace ruyijs::domain::indexed_database {
using IndexEntry = std::pair<EncodedKey,EncodedKey>;  // (index key, primary key)
inline std::map<EncodedKey,Bytes> VisibleRecords(const ObjectStore& store,const Transaction& tx) {
  std::map<EncodedKey,Bytes> visible;
  if(!tx.cleared) visible=store.records;
  for(const auto& key:tx.deletes) visible.erase(key);
  for(const auto& [key,bytes]:tx.writes) visible[key]=bytes;
  return visible;
}
inline std::set<IndexEntry> VisibleIndex(const ObjectStore& store,const Transaction& tx,const std::u16string& name) {
  std::set<IndexEntry> entries;
  const auto& index=store.indexes.at(name);
  if(!tx.cleared)
    for(const auto& entry:index.entries)
      if(!tx.deletes.count(entry.second) && !tx.writes.count(entry.second)) entries.insert(entry);
  for(const auto& [primary,byIndex]:tx.indexKeys) {
    auto keys=byIndex.find(name);
    if(keys!=byIndex.end()) for(const auto& key:keys->second) entries.insert({key,primary});
  }
  return entries;
}
enum class Direction { Next, NextUnique, Prev, PrevUnique };
// One iteration row: key (index key or primary key), primary key and value.
struct CursorRow { EncodedKey key, primary; Bytes value; };
struct CursorPosition { bool started=false; EncodedKey key, primary; };
struct CursorStep {
  std::optional<EncodedKey> key, primary;  // continue(key) / continuePrimaryKey(key, primaryKey)
  uint32_t count=1;                         // advance(count)
};
// Rows in iteration order; unique directions keep the lowest primary key per key.
inline std::vector<CursorRow> CursorRows(const ObjectStore& store,const Transaction& tx,const std::u16string* index,
                                         const KeyRange& range,Direction direction) {
  const auto records=VisibleRecords(store,tx);
  std::vector<CursorRow> rows;
  if(!index) {
    for(const auto& [key,bytes]:records) if(range.Contains(key)) rows.push_back({key,key,bytes});
  } else {
    for(const auto& [key,primary]:VisibleIndex(store,tx,*index)) {
      if(!range.Contains(key)) continue;
      if((direction==Direction::NextUnique || direction==Direction::PrevUnique) && !rows.empty() && rows.back().key==key)
        continue;
      auto record=records.find(primary);
      rows.push_back({key,primary,record==records.end() ? Bytes{} : record->second});
    }
  }
  if(direction==Direction::Prev || direction==Direction::PrevUnique) {
    if(direction==Direction::PrevUnique || !index) return std::vector<CursorRow>(rows.rbegin(),rows.rend());
    std::vector<CursorRow> reversed(rows.rbegin(),rows.rend());
    return reversed;
  }
  return rows;
}
// The next row after `position` honouring continue/continuePrimaryKey/advance.
inline std::optional<CursorRow> CursorAdvance(const std::vector<CursorRow>& rows,const CursorPosition& position,
                                              Direction direction,const CursorStep& step) {
  const bool forward=direction==Direction::Next || direction==Direction::NextUnique;
  const bool unique=direction==Direction::NextUnique || direction==Direction::PrevUnique;
  uint32_t remaining=step.count ? step.count : 1;
  for(const auto& row:rows) {
    if(position.started) {
      const auto here=std::make_pair(row.key,row.primary),at=std::make_pair(position.key,position.primary);
      if(unique ? (forward ? row.key<=position.key : row.key>=position.key) : (forward ? here<=at : here>=at)) continue;
    }
    if(step.key && step.primary) {
      const auto here=std::make_pair(row.key,row.primary),target=std::make_pair(*step.key,*step.primary);
      if(forward ? here<target : here>target) continue;
    } else if(step.key && (forward ? row.key<*step.key : row.key>*step.key)) continue;
    if(--remaining==0) return row;
  }
  return std::nullopt;
}
}
#endif
