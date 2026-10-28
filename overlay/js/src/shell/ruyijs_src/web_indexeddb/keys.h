#pragma once
#include "web_indexeddb/state.h"
namespace ruyijs::web_indexeddb {
using EncodedKey=domain::indexed_database::EncodedKey;
using KeyRange=domain::indexed_database::KeyRange;
bool KeyText(JSContext*,JS::HandleString,std::u16string*);
bool KeyType(JSContext*,JS::HandleValue,bool*);
bool EncodeKey(State&,JS::HandleValue,EncodedKey*);
bool DecodeKey(State&,const EncodedKey&,JS::MutableHandleValue);
bool ReadRange(State&,JS::HandleValue,KeyRange*,bool optional);
bool PrepareQuery(JSContext*,const JS::CallArgs&,int,Query*,JS::MutableHandleValue);
bool ReadQuery(State&,JS::HandleValue,int,Query*);
bool KeyRangeCreate(JSContext*,unsigned,JS::Value*);
bool KeyRangeGet(JSContext*,unsigned,JS::Value*);
bool KeyRangeIncludes(JSContext*,unsigned,JS::Value*);
bool CompareKeys(JSContext*,unsigned,JS::Value*);
bool InstallKeys(State&);
bool QueryResult(Job&,JS::MutableHandleValue,const char**);
JSObject* NewRecord(State&,const Record&,const std::u16string* primary=nullptr);
bool RecordGet(JSContext*,unsigned,JS::Value*);
extern const JSClass kKeyRangeClass,kRecordClass;
enum { RangeOwner,RangeLower,RangeUpper,RangeFlags,RangeLowerCache,RangeUpperCache };
enum DataKind { Get=1,Put=2,CompleteTransaction=3,GetKey=4,GetAll=5,GetAllKeys=6,Count=7,GetAllRecords=8,
                Add=10,Delete=11,Clear=12,CursorStepKind=13 };
}
