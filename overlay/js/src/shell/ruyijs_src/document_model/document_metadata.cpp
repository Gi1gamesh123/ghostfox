#include "document_model/state.h"
#include "browser_environment/environment.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include <cstdio>
#include <ctime>

namespace ruyijs::document_model {
namespace {
enum Field { Referrer, Modified, Compat, Domain, Design, Direction, Color };
struct Property { const char* name; Field field; bool writable; };
constexpr Property properties[] = {
    {"referrer", Referrer, false}, {"lastModified", Modified, false}, {"compatMode", Compat, false},
    {"domain", Domain, true}, {"designMode", Design, true}, {"dir", Direction, true},
    {"alinkColor", Color, true}, {"bgColor", Color, true}, {"fgColor", Color, true},
    {"linkColor", Color, true}, {"vlinkColor", Color, true}};
DocumentMetadataField MetadataField(Field field) {
  return field == Domain ? DocumentMetadataField::Domain
      : field == Direction ? DocumentMetadataField::Direction : DocumentMetadataField::DesignMode;
}
bool Read(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& property = properties[FunctionSlot(args)];
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  auto* entry = DocumentReceiver(cx, object, &args);
  if (!entry) return false;
  State& owner = *entry->owner;
  DomString text;
  {
    JSAutoRealm realm(cx, owner.page);
    owner.Observe("Document", property.name, int(ApiAccess::Read));
    const auto field = property.field;
    if (field == Referrer) {
      if (entry->key.document == owner.main.document && entry->key.node == owner.main.node) text = owner.referrer;
    }
    else if (field == Modified) {
      // Without a Last-Modified header Firefox reports the current local time.
      std::time_t now = std::time(nullptr);
      std::tm local{};
#ifdef _WIN32
      localtime_s(&local, &now);
#else
      localtime_r(&now, &local);
#endif
      char buffer[32];
      std::snprintf(buffer, sizeof(buffer), "%02d/%02d/%04d %02d:%02d:%02d", local.tm_mon + 1,
                    local.tm_mday, local.tm_year + 1900, local.tm_hour, local.tm_min, local.tm_sec);
      for (const char* c = buffer; *c; ++c) text.push_back(char16_t(*c));
    }
    else if (field == Compat) {
      // A document without a doctype is in quirks mode (about:blank, document.write);
      // an XML-parsed document never is (Firefox).
      DocumentSnapshot snapshot;
      if (!owner.Check(owner.model.InspectDocument(entry->key, &snapshot))) return false;
      JS::RootedValue doctype(cx);
      if (!snapshot.xml_parsed && !JS_GetProperty(cx, object, "doctype", &doctype)) return false;
      text = !snapshot.xml_parsed && doctype.isNull() ? u"BackCompat" : u"CSS1Compat";
    }
    else if (field == Domain || field == Design || field == Direction) {
      DomString url;
      if (field == Domain) {
        DocumentSnapshot snapshot;
        if (!owner.Check(owner.model.InspectDocument(entry->key, &snapshot))) return false;
        if (snapshot.url) url = *snapshot.url;
        else {
          JS::RootedValue value(cx);
          if (!owner.environment.URL(&value) || !String(cx, value, &url)) return false;
        }
      }
      if (!owner.Check(owner.model.ReadDocumentMetadata(entry->key, MetadataField(field), url, &text))) return false;
      if (field == Domain && text.empty()) {
        JS::RootedValue value(cx);
        DomString origin;
        if (!owner.environment.Origin(&value) || !String(cx, value, &origin) ||
            !owner.Check(owner.model.ReadDocumentMetadata(entry->key, DocumentMetadataField::Domain,
                                                         origin, &text))) return false;
      }
    }
  }
  return Value(cx, DomStringView(text), args.rval());
}
bool Write(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& property = properties[FunctionSlot(args)];
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  auto* entry = DocumentReceiver(cx, object, &args);
  if (!entry) return false;
  if (!argc) return TypeError(cx, "Document setter requires one argument");
  State& owner = *entry->owner;
  owner.Observe("Document", property.name, int(ApiAccess::Write));
  if (property.field != Color) {
    DomString text;
    JS::RootedString converted(cx, JS::ToString(cx, args[0]));
    if (!converted) {
      if (!JS_IsExceptionPending(cx) || JS_IsThrowingOutOfMemory(cx)) return false;
      JS_ClearPendingException(cx);
    } else {
      JS::RootedValue value(cx, JS::StringValue(converted));
      if (!String(cx, value, &text)) return false;
    }
    JSAutoRealm realm(cx, owner.page);
    if (!owner.Check(owner.model.WriteDocumentMetadata(entry->key, MetadataField(property.field), text))) return false;
  }
  args.rval().setUndefined(); return true;
}
}
bool InstallDocumentMetadata(State& state) {
  for (size_t i = 0; i < std::size(properties); ++i)
    if (!state.Accessor("Document", properties[i].name, Read, int32_t(i),
                        properties[i].writable ? Write : nullptr)) return false;
  return true;
}
}
