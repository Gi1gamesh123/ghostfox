#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace ruyijs::domain::resource_network {
inline std::string CookieLower(std::string text) {
  for (char& c : text) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
  return text;
}
inline std::string CookieTrim(std::string text) {
  auto a = text.find_first_not_of(" \t"), b = text.find_last_not_of(" \t");
  return a == std::string::npos ? "" : text.substr(a, b-a+1);
}
inline bool CookieNumber(const std::string& text, int* out) {
  if (text.empty() || text.size()>4) return false;
  *out=0;
  for (char c:text) { if(c<'0'||c>'9') return false; *out=*out*10+c-'0'; }
  return true;
}
// RFC 6265 date tokens; no platform timezone or second clock.
inline bool CookieDate(const std::string& input, double* seconds) {
  std::vector<std::string> tokens;
  std::string token;
  for (unsigned char c:input) {
    bool delimiter=c==9||(c>=32&&c<=47)||(c>=59&&c<=64)||(c>=91&&c<=96)||(c>=123&&c<=126);
    if (delimiter) { if(!token.empty()){tokens.push_back(token);token.clear();} }
    else token+=char(c);
  }
  if(!token.empty())tokens.push_back(token);
  int day=-1,month=-1,year=-1,hour=-1,minute=-1,second=-1;
  const std::string months="janfebmaraprmayjunjulaugsepoctnovdec";
  for(auto t:tokens) {
    auto a=t.find(':'), b=a==std::string::npos?a:t.find(':',a+1);
    if(hour<0&&b!=std::string::npos) {
      int h,m,s;
      if(CookieNumber(t.substr(0,a),&h)&&CookieNumber(t.substr(a+1,b-a-1),&m)&&
         CookieNumber(t.substr(b+1),&s)){hour=h;minute=m;second=s;continue;}
    }
    int n;
    if(day<0&&t.size()<=2&&CookieNumber(t,&n)){day=n;continue;}
    auto pos=t.size()>=3?months.find(CookieLower(t.substr(0,3))):std::string::npos;
    if(month<0&&pos!=std::string::npos&&pos%3==0){month=int(pos/3)+1;continue;}
    if(year<0&&t.size()>=2&&CookieNumber(t,&n))year=n;
  }
  if(year>=70&&year<=99)year+=1900;else if(year>=0&&year<=69)year+=2000;
  if(year<1601||month<1||day<1||day>31||hour<0||hour>23||minute<0||minute>59||second<0||second>59)return false;
  int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
  if(year%4==0&&(year%100!=0||year%400==0))days[1]=29;
  if(day>days[month-1])return false;
  int y=year-(month<=2),era=y/400,yoe=y-era*400;
  int doy=(153*(month+(month>2?-3:9))+2)/5+day-1;
  int64_t date=int64_t(era)*146097+yoe*365+yoe/4-yoe/100+doy-719468;
  *seconds=double(date)*86400+hour*3600+minute*60+second;
  return true;
}
}
