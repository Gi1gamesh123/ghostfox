#pragma once
#include "execution_scheduling/state.h"
#include "execution_scheduling/model_timeline.h"
#include "execution_scheduling/navigation_timing.h"
#include "js/Object.h"
#include <string>
namespace ruyijs::execution_scheduling {
enum PerfSlot { PClock, PTimeline, PEntries, PPrototypes, PException, PTiming, PNavigation,
                PHandler, PAddListener, PRemoveListener, PObserverPrototype, PListPrototype,
                PObservers, PObserverPending, PObserverNext, PCounts, PPage, PEvents,
                // This document's navigation record (domain
                // NavigationTiming). One per Performance owner, so a retired
                // document never reads the next document's values.
                PNavTiming, PerfSlots };
enum EntrySlot { EKind, EName, EStart, EDuration, EDetail, EntrySlots };
enum PerfKind { Entry, Mark, MeasureEntry, Resource, NavigationEntry, Paint, EventTiming,
                LargestPaint, ServerTiming, LegacyTiming, LegacyNavigation, KindCount };
enum PerfOp { Now, Json, Entries, ByType, ByName, ClearResources, BufferSize, AddMark,
              ClearMarks, AddMeasure, ClearMeasures, Origin, TimingObject, NavigationObject, Handler, InteractionCount, EventCountsObject };
extern const JSClass kEntryClass, kLegacyTimingClass, kLegacyNavigationClass;
struct PerfField {const char* name;};
struct PerfSchema {const char* name;int parent;const char* const* fields;unsigned count;bool json;bool worker;};
extern const PerfSchema kPerfSchema[KindCount];
extern const char* const kLegacyTimingFields[21];
bool PerfInstallEntries(State&,JS::HandleObject,JS::HandleObject,PageHelpers&);
bool PerfReceiver(JSContext*,const JS::CallArgs&,JS::MutableHandleObject);
JSObject* PerfFunction(JSContext*,JS::HandleObject,JSNative,const char*,unsigned,int,unsigned=0);
bool PerfFinishInterface(JSContext*,PageHelpers&,const char*,JS::HandleObject,JS::HandleObject,JS::HandleObject);
bool PerfProperty(JSContext*,PageHelpers&,JS::HandleObject,JS::HandleObject,const char*,const char*,JSNative,int,bool=false);
bool PerfMethod(JSContext*,unsigned,JS::Value*);
bool PerformanceGlobal(JSContext*,unsigned,JS::Value*);
bool PerfEntryMember(JSContext*,unsigned,JS::Value*);
bool PerfEntryJson(JSContext*,unsigned,JS::Value*);
bool PerfLegacyMember(JSContext*,unsigned,JS::Value*);
bool PerfMarkConstructor(JSContext*,unsigned,JS::Value*);
bool PerfIllegalConstructor(JSContext*,unsigned,JS::Value*);
bool PerfMark(JSContext*,JS::HandleObject,const JS::CallArgs&,JS::MutableHandleValue,bool=false);
bool PerfMeasure(JSContext*,JS::HandleObject,const JS::CallArgs&,JS::MutableHandleValue);
bool PerfQuery(JSContext*,JS::HandleObject,const JS::CallArgs&,PerfOp);
bool PerfSetHandler(JSContext*,JS::HandleObject,JS::HandleValue);
bool PerfRecord(JSContext*,JS::HandleObject,JS::HandleObject);
bool PerfQueueResourceTask(JSContext*,JS::HandleObject);
const char* PerfEntryType(int);
int PerfTimelineKind(int);
bool PerfResourceField(JSContext*,JS::HandleObject,unsigned,JS::MutableHandleValue);
// A navigation entry's own members (kPerfSchema[NavigationEntry] order), read
// live from its document's record, so a lifecycle milestone the load path
// stamps after the entry exists is visible without rebuilding the entry.
bool PerfNavigationField(JSContext*,JS::HandleObject,unsigned,JS::MutableHandleValue);
bool PerfEntry(JSContext*,JS::HandleObject,int,JS::HandleString,double,double,JS::HandleValue,
               JS::HandleObject,JS::MutableHandleObject);
bool PerfClone(JSContext*,JS::HandleObject,JS::HandleValue,JS::MutableHandleValue);
bool PerfThrow(JSContext*,JS::HandleObject,const char*);
bool PerfString(JSContext*,JS::HandleValue,JS::MutableHandleString);
bool PerfText(JSContext*,JS::HandleString,std::u16string*);
bool PerfNumber(JSContext*,JS::HandleValue,double*);
bool PerfOption(JSContext*,JS::HandleValue,const char*,JS::MutableHandleValue);
bool PerfReserved(JSContext*,JS::HandleString);
bool PerfTimestamp(JSContext*,JS::HandleObject,JS::HandleValue,double*);
double PerfNow(JS::HandleObject);
double PerfTimeOrigin(JS::HandleObject);
// This owner's navigation record, or null before one is published.
inline NavigationTiming* PerfNavigationRecord(JSObject* owner) {
 const JS::Value slot=JS::GetReservedSlot(owner,PNavTiming);
 return slot.isUndefined()?nullptr:static_cast<NavigationTiming*>(slot.toPrivate());
}
int PerfSelector(const JS::CallArgs&);
}
