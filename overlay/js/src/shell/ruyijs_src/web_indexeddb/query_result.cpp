#include "web_indexeddb/keys.h"
#include "js/Array.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
namespace ruyijs::web_indexeddb {
bool QueryResult(Job& job,JS::MutableHandleValue out,const char** error) {
  State& s=*job.owner;JSContext* cx=s.cx;
  JS::RootedObject tx(cx,&JS::GetReservedSlot(job.request,TransactionSlot).toObject());
  std::vector<Record> records;
  std::vector<std::u16string> primaries;  // index queries: primary key of each row
  const uint32_t id=job.txId;
  if(job.hasIndex) {
    std::vector<CursorRow> rows;
    if(!s.store->model.Rows(id,&job.index,job.query.range,job.query.reverse?Direction::Prev:Direction::Next,&rows)) {
      *error="UnknownError";return true;
    }
    for(auto& row:rows) {
      if(job.query.count && records.size()>=job.query.count)break;
      records.push_back({row.key,std::move(row.value)});primaries.push_back(std::move(row.primary));
    }
  } else if(!s.store->model.Select(id,job.query,&records)) {
    *error="UnknownError";return true;
  }
  if(job.kind==Count){out.setNumber(double(records.size()));return true;}
  if(job.kind==Get || job.kind==GetKey) {
    if(records.empty()){out.setUndefined();return true;}
    if(job.kind==Get)return CloneRead(s,records.front().second,out);
    return DecodeKey(s,job.hasIndex?primaries.front():records.front().first,out);
  }
  JS::RootedObject array(cx,JS::NewArrayObject(cx,records.size()));
  if(!array)return false;
  for(uint32_t i=0;i<records.size();++i) {
    JS::RootedValue value(cx);
    if(job.kind==GetAllKeys) {if(!DecodeKey(s,job.hasIndex?primaries[i]:records[i].first,&value))return false;}
    else if(job.kind==GetAllRecords) {
      JS::RootedObject record(cx,NewRecord(s,records[i],job.hasIndex?&primaries[i]:nullptr));
      if(!record)return false;value.setObject(*record);
    } else if(!CloneRead(s,records[i].second,&value))return false;
    if(!JS_SetElement(cx,array,i,value))return false;
  }
  out.setObject(*array);return true;
}
}
