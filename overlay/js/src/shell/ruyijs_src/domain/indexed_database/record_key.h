#pragma once
#include <bit>
#include <cstdint>
#include <string>

namespace ruyijs::domain::indexed_database {
using EncodedKey = std::u16string;
enum KeyType : char16_t { NumberKey=0x10, DateKey=0x20, StringKey=0x30, BinaryKey=0x40, ArrayKey=0x50 };
inline constexpr size_t kKeyUnitLimit = 1024*1024;
inline void AppendNumberKey(EncodedKey& out, double number, KeyType type) {
  uint64_t bits=std::bit_cast<uint64_t>(number==0 ? 0.0 : number);
  bits=(bits>>63) ? ~bits : bits^(uint64_t(1)<<63);
  out.push_back(type);
  for(int shift=56;shift>=0;shift-=8)out.push_back(char16_t((bits>>shift)&255));
}
inline double KeyNumber(const EncodedKey& value,size_t offset) {
  uint64_t bits=0;
  for(size_t i=0;i<8;++i)bits=(bits<<8)|value[offset+i];
  bits=(bits>>63) ? bits^(uint64_t(1)<<63) : ~bits;
  return std::bit_cast<double>(bits);
}
inline void AppendStringKey(EncodedKey& out,const std::u16string& value) {
  out.push_back(StringKey);
  for(char16_t unit:value){out.push_back(1);out.push_back(unit);}
  out.push_back(0);
}
struct KeyRange {
  EncodedKey lower,upper;
  bool lowerOpen=false,upperOpen=false;
  bool Contains(const EncodedKey& key) const {
    return (lower.empty() || (lowerOpen ? key>lower : key>=lower)) &&
           (upper.empty() || (upperOpen ? key<upper : key<=upper));
  }
};
struct Query {
  KeyRange range;
  uint32_t count=0;
  bool reverse=false;
};
}
