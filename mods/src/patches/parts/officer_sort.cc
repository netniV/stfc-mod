#include "config.h"
#include "errormsg.h"

#include <il2cpp/il2cpp-functions.h>
#include <il2cpp/il2cpp_helper.h>

#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <cstring>
#include <mutex>

// Restore the "Below Deck Ability" sort option that was removed from the game
// in m93.1. The game's OfficerSortGenerators no longer register a below-deck
// sorter, but the underlying comparison functions
// (SortingPredicates.OfficerSortByBelowDeckAbilityAscending/Descending) and
// the SortFunction / SortComparer.ProcessingOption classes still exist.
//
// We hook OfficerManager.GetRosterSortOptions() and
// OfficerManager.GetAssignmentSortOptions() and append a ProcessingOption
// built from a SortFunction whose _ascending/_descending delegates point at
// the game's own below-deck predicates. The display key reuses the existing
// "officer_filter_below_deck_ability" localization entry.
//
// All IL2CPP classes/methods/fields are resolved by name at runtime, so the
// patch is resilient to minor game updates. Managed objects created during
// construction are rooted with strong GC handles until they are stored on
// their owning managed object, after which the managed graph keeps them alive.

namespace {

// Sort ID used by the game for the below-deck ability sort option.
// Verified from m93-release: "below_deck_ability" (config_key category).
constexpr char kBelowDeckKey[] = "below_deck_ability";

// Assignment screen uses _ prefix for display keys (_strength, _class, etc.)
// The localization key is constructed as "officer_assignment_sorting" + key.
constexpr char kBelowDeckAssignmentKey[] = "_below_deck_ability";

// ---------------------------------------------------------------------------
// Cached IL2CPP resolution state (resolved once on first hook fire).
// Helpers have no default constructor, so they are held by pointer and
// constructed during InitializeState().
// ---------------------------------------------------------------------------
// The delegate invoke stub calls method_ptr(obj1, obj2, delegate).
// The sort methods take (obj1, obj2) and ignore the 3rd arg.
using SortMethodPtr = int (*)(void* obj1, void* obj2, void* delegatePtr);

struct OfficerSortState {
  IL2CppClassHelper* sortingPredicates   = nullptr; // Digit.Client.Core.SortingPredicates
  IL2CppClassHelper* sortFunction        = nullptr; // Digit.Client.Core.SortFunction
  IL2CppClassHelper* processingOption    = nullptr; // Digit.Client.Sorting.SortComparer.ProcessingOption
  IL2CppClassHelper* officerSortGenerators = nullptr; // Digit.Prime.Officers.OfficerSortGenerators

  const MethodInfo* ascendingMethod      = nullptr; // OfficerSortByBelowDeckAbilityAscending
  const MethodInfo* descendingMethod     = nullptr; // OfficerSortByBelowDeckAbilityDescending
  const MethodInfo* unlockedStateMethod  = nullptr; // OfficerSortByUnlockedStateAscending
  const MethodInfo* indexMethod          = nullptr; // OfficerSortByIndexAscending
  const MethodInfo* processingOptionCtor = nullptr; // .ctor(string, FilterFunction, SortFunction, string, bool)

  // Resolved method pointers for C++ wrapper functions.
  SortMethodPtr unlockedStatePtr = nullptr; // OfficerSortByUnlockedStateAscending
  SortMethodPtr bdaAscPtr        = nullptr; // OfficerSortByBelowDeckAbilityAscending
  SortMethodPtr bdaDescPtr       = nullptr; // OfficerSortByBelowDeckAbilityDescending
  SortMethodPtr indexAscPtr      = nullptr; // OfficerSortByIndexAscending

  IL2CppFieldHelper* sortFunctionDisplayKey  = nullptr;
  IL2CppFieldHelper* sortFunctionAscending   = nullptr;
  IL2CppFieldHelper* sortFunctionDescending  = nullptr;
  IL2CppFieldHelper* rosterSortersField      = nullptr; // _rosterSorters (List<SortFunction>)
  IL2CppFieldHelper* rosterOptionsField      = nullptr; // _rosterOptions
  IL2CppFieldHelper* assignmentOptionsField  = nullptr; // _assignmentOptions
  IL2CppFieldHelper* processingOptionDisplayKey     = nullptr; // _displayKey
  IL2CppFieldHelper* processingOptionIconKey        = nullptr; // _iconKey
  IL2CppFieldHelper* processingOptionFilteringOption = nullptr; // _filteringOption
  IL2CppFieldHelper* processingOptionSortingOption  = nullptr; // _sortingOption
  IL2CppFieldHelper* processingOptionUseAux         = nullptr; // _003CUseAuxiliaryOption_003Ek__BackingField

  Il2CppClass*      funcClass = nullptr; // Func<object,object,int> (discovered at runtime)
  const MethodInfo* funcCtor  = nullptr; // delegate .ctor(object, IntPtr)

  bool valid = false;
};

OfficerSortState& State()
{
  static OfficerSortState s;
  return s;
}

// ---------------------------------------------------------------------------
// Combined sort wrappers. The game's GenerateSortFunction prepends
// OfficerSortByUnlockedStateAscending to every sort predicate array, so
// available officers always sort to the top. We replicate this in C++ by
// creating wrapper functions that first call the unlocked-state predicate,
// then the BDA predicate.
//
// The delegate invoke stub calls method_ptr(obj1, obj2, delegate).
// The original sort methods take (obj1, obj2) and ignore the 3rd arg.
// ---------------------------------------------------------------------------

// IMPORTANT: SortComparer.Compare always calls the _ascending delegate.
// For Descending sort direction, it negates the _ascending result.
// The _descending delegate is never called. So all logic must be in the
// *Ascending wrappers, designed so that direct call = ascending behavior
// and negated call = descending behavior.

// Roster _ascending (called directly for Ascending, negated for Descending):
//   Ascending:  available → BDA-first → newer-officers-first (index descending)
//   Descending: negated → non-BDA-first → older-officers-first (index ascending)
int RosterAscendingWrapper(void* obj1, void* obj2, void* /*delegatePtr*/)
{
  auto& s = State();
  if (s.unlockedStatePtr) {
    int r = s.unlockedStatePtr(obj1, obj2, nullptr);
    if (r != 0) return r;
  }
  if (s.bdaDescPtr) {
    int r = s.bdaDescPtr(obj1, obj2, nullptr);
    if (r != 0) return r;
  }
  // Negate index ascending to get index descending (newer officers first).
  return s.indexAscPtr ? -s.indexAscPtr(obj1, obj2, nullptr) : 0;
}

// Roster _descending (never called by SortComparer, kept for safety):
//   If called directly: available → non-BDA-first → newer-officers-first
int RosterDescendingWrapper(void* obj1, void* obj2, void* /*delegatePtr*/)
{
  auto& s = State();
  if (s.unlockedStatePtr) {
    int r = s.unlockedStatePtr(obj1, obj2, nullptr);
    if (r != 0) return r;
  }
  if (s.bdaAscPtr) {
    int r = s.bdaAscPtr(obj1, obj2, nullptr);
    if (r != 0) return r;
  }
  return s.indexAscPtr ? -s.indexAscPtr(obj1, obj2, nullptr) : 0;
}

// Assignment _ascending (called directly for Ascending, negated for Descending):
//   Ascending:  available → non-BDA-first → older-officers-first (index ascending)
//   Descending: negated → BDA-first → newer-officers-first (index descending)
int AssignmentAscendingWrapper(void* obj1, void* obj2, void* /*delegatePtr*/)
{
  auto& s = State();
  if (s.unlockedStatePtr) {
    int r = s.unlockedStatePtr(obj1, obj2, nullptr);
    if (r != 0) return r;
  }
  if (s.bdaAscPtr) {
    int r = s.bdaAscPtr(obj1, obj2, nullptr);
    if (r != 0) return r;
  }
  return s.indexAscPtr ? s.indexAscPtr(obj1, obj2, nullptr) : 0;
}

// Assignment _descending (never called by SortComparer, kept for safety):
//   If called directly: available → BDA-first → older-officers-first
int AssignmentDescendingWrapper(void* obj1, void* obj2, void* /*delegatePtr*/)
{
  auto& s = State();
  if (s.unlockedStatePtr) {
    int r = s.unlockedStatePtr(obj1, obj2, nullptr);
    if (r != 0) return r;
  }
  if (s.bdaDescPtr) {
    int r = s.bdaDescPtr(obj1, obj2, nullptr);
    if (r != 0) return r;
  }
  return s.indexAscPtr ? s.indexAscPtr(obj1, obj2, nullptr) : 0;
}

// ---------------------------------------------------------------------------
// Small invoke helpers (exceptions logged, not thrown).
// ---------------------------------------------------------------------------
Il2CppObject* Invoke(const MethodInfo* method, void* target, void** args, const char* tag)
{
  if (!method) return nullptr;
  Il2CppException* exc = nullptr;
  auto             result = il2cpp_runtime_invoke(method, target, args, &exc);
  if (exc) {
    char msg[512] = {0};
    il2cpp_format_exception(exc, msg, sizeof(msg));
    if (msg[0]) {
      spdlog::warn("[OfficerSort] {} raised: {}", tag, msg);
    } else {
      spdlog::warn("[OfficerSort] {} invocation raised an exception", tag);
    }
    return nullptr;
  }
  return result;
}

// ---------------------------------------------------------------------------
// Resolve all required IL2CPP classes/methods/fields. Called once.
// ---------------------------------------------------------------------------
bool InitializeState()
{
  auto& s = State();

  spdlog::info("[OfficerSort] InitializeState: resolving SortingPredicates");
  s.sortingPredicates = new IL2CppClassHelper(
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "SortingPredicates"));
  if (!s.sortingPredicates->get_cls()) {
    ErrorMsg::MissingHelper("Digit.Client.Core", "SortingPredicates");
    return false;
  }

  spdlog::info("[OfficerSort] InitializeState: resolving SortFunction");
  s.sortFunction = new IL2CppClassHelper(
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.Client.Core", "SortFunction"));
  if (!s.sortFunction->get_cls()) {
    ErrorMsg::MissingHelper("Digit.Client.Core", "SortFunction");
    return false;
  }

  spdlog::info("[OfficerSort] InitializeState: resolving SortComparer");
  auto sortComparer = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Sorting", "SortComparer");
  if (!sortComparer.get_cls()) {
    ErrorMsg::MissingHelper("Digit.Client.Sorting", "SortComparer");
    return false;
  }
  spdlog::info("[OfficerSort] InitializeState: resolving ProcessingOption via nested-types iterator");
  // Resolve the nested ProcessingOption class via the safe
  // il2cpp_class_get_nested_types iterator API. The helper's GetNestedType()
  // reads the nestedTypes array directly and crashes on this class.
  Il2CppClass* processingOptionCls = nullptr;
  void*        iter                = nullptr;
  while (auto* nested = il2cpp_class_get_nested_types(sortComparer.get_cls(), &iter)) {
    if (nested->name && std::strcmp(nested->name, "ProcessingOption") == 0) {
      processingOptionCls = nested;
      break;
    }
  }
  spdlog::info("[OfficerSort] InitializeState: ProcessingOption = {}", static_cast<const void*>(processingOptionCls));
  s.processingOption = new IL2CppClassHelper(processingOptionCls);
  if (!s.processingOption->get_cls()) {
    ErrorMsg::MissingHelper("Digit.Client.Sorting", "SortComparer.ProcessingOption");
    return false;
  }

  spdlog::info("[OfficerSort] InitializeState: resolving OfficerSortGenerators");
  s.officerSortGenerators = new IL2CppClassHelper(
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Officers", "OfficerSortGenerators"));
  if (!s.officerSortGenerators->get_cls()) {
    ErrorMsg::MissingHelper("Digit.Prime.Officers", "OfficerSortGenerators");
    return false;
  }

  spdlog::info("[OfficerSort] InitializeState: resolving sort methods");
  s.ascendingMethod = s.sortingPredicates->GetMethodInfo("OfficerSortByBelowDeckAbilityAscending", 2);
  if (!s.ascendingMethod) {
    ErrorMsg::MissingStaticMethod("SortingPredicates", "OfficerSortByBelowDeckAbilityAscending");
    return false;
  }

  s.descendingMethod = s.sortingPredicates->GetMethodInfo("OfficerSortByBelowDeckAbilityDescending", 2);
  if (!s.descendingMethod) {
    ErrorMsg::MissingStaticMethod("SortingPredicates", "OfficerSortByBelowDeckAbilityDescending");
    return false;
  }

  // Resolve the unlocked-state predicate (prepended by GenerateSortFunction
  // to every roster sort, keeping available officers at the top).
  s.unlockedStateMethod = s.sortingPredicates->GetMethodInfo("OfficerSortByUnlockedStateAscending", 2);
  if (!s.unlockedStateMethod) {
    spdlog::warn("[OfficerSort] OfficerSortByUnlockedStateAscending not found; locked officers may not sort to bottom");
  } else {
    s.unlockedStatePtr = reinterpret_cast<SortMethodPtr>(s.unlockedStateMethod->methodPointer);
    spdlog::info("[OfficerSort] InitializeState: unlockedStatePtr = {}",
                 static_cast<const void*>(s.unlockedStatePtr));
  }
  // Resolve the index predicate (used as a tertiary tiebreaker so officers
  // within the same BDA tier are sorted by internal officer index).
  s.indexMethod = s.sortingPredicates->GetMethodInfo("OfficerSortByIndexAscending", 2);
  if (!s.indexMethod) {
    spdlog::warn("[OfficerSort] OfficerSortByIndexAscending not found; no index tiebreaker");
  } else {
    s.indexAscPtr = reinterpret_cast<SortMethodPtr>(s.indexMethod->methodPointer);
    spdlog::info("[OfficerSort] InitializeState: indexAscPtr = {}",
                 static_cast<const void*>(s.indexAscPtr));
  }

  s.bdaAscPtr  = reinterpret_cast<SortMethodPtr>(s.ascendingMethod->methodPointer);
  s.bdaDescPtr = reinterpret_cast<SortMethodPtr>(s.descendingMethod->methodPointer);

  spdlog::info("[OfficerSort] InitializeState: resolving ProcessingOption .ctor");
  s.processingOptionCtor = s.processingOption->GetMethodInfo(".ctor", 5);
  if (!s.processingOptionCtor) {
    ErrorMsg::MissingMethod("SortComparer.ProcessingOption", ".ctor(5)");
    return false;
  }

  spdlog::info("[OfficerSort] InitializeState: resolving SortFunction fields");
  s.sortFunctionDisplayKey = new IL2CppFieldHelper(s.sortFunction->GetField("_displayKey"));
  s.sortFunctionAscending  = new IL2CppFieldHelper(s.sortFunction->GetField("_ascending"));
  s.sortFunctionDescending = new IL2CppFieldHelper(s.sortFunction->GetField("_descending"));
  // Note: isValidHelper() always returns true in DEBUG, so we can't rely on it.
  // The fields are public on SortFunction so they should always resolve.
  spdlog::info("[OfficerSort] InitializeState: SortFunction fields resolved");

  spdlog::info("[OfficerSort] InitializeState: resolving OfficerSortGenerators fields");
  s.rosterSortersField     = new IL2CppFieldHelper(s.officerSortGenerators->GetField("_rosterSorters"));
  s.rosterOptionsField     = new IL2CppFieldHelper(s.officerSortGenerators->GetField("_rosterOptions"));
  s.assignmentOptionsField = new IL2CppFieldHelper(s.officerSortGenerators->GetField("_assignmentOptions"));
  if (!s.rosterOptionsField->isValidHelper() || !s.assignmentOptionsField->isValidHelper()) {
    spdlog::error("[OfficerSort] OfficerSortGenerators field layout changed; cannot find _rosterOptions/_assignmentOptions");
    return false;
  }
  spdlog::info("[OfficerSort] InitializeState: _rosterSorters offset={}", s.rosterSortersField->offset());
  spdlog::info("[OfficerSort] InitializeState: _rosterOptions offset={}", s.rosterOptionsField->offset());
  spdlog::info("[OfficerSort] InitializeState: _assignmentOptions offset={}", s.assignmentOptionsField->offset());

  spdlog::info("[OfficerSort] InitializeState: resolving ProcessingOption fields");
  s.processingOptionDisplayKey      = new IL2CppFieldHelper(s.processingOption->GetField("_displayKey"));
  spdlog::info("[OfficerSort] InitializeState: _displayKey resolved, offset={}", s.processingOptionDisplayKey->offset());
  s.processingOptionSortingOption   = new IL2CppFieldHelper(s.processingOption->GetField("_sortingOption"));
  spdlog::info("[OfficerSort] InitializeState: _sortingOption resolved, offset={}", s.processingOptionSortingOption->offset());
  if (!s.processingOptionDisplayKey->isValidHelper() || !s.processingOptionSortingOption->isValidHelper()) {
    spdlog::error("[OfficerSort] ProcessingOption field layout changed; cannot find _displayKey/_sortingOption");
    return false;
  }
  // Optional fields — check fieldInfo directly (isValidHelper() always returns
  // true in DEBUG builds, which would crash on offset() if fieldInfo is null).
  auto iconField   = s.processingOption->GetField("_iconKey");
  auto filterField = s.processingOption->GetField("_filteringOption");
  // The backing field name uses literal angle brackets in IL2CPP, not _003C encoding.
  auto useAuxField = s.processingOption->GetField("<UseAuxiliaryOption>k__BackingField");
  s.processingOptionIconKey         = new IL2CppFieldHelper(iconField);
  s.processingOptionFilteringOption = new IL2CppFieldHelper(filterField);
  s.processingOptionUseAux          = new IL2CppFieldHelper(useAuxField);
  spdlog::info("[OfficerSort] InitializeState: ProcessingOption optional fields ok");

  spdlog::info("[OfficerSort] InitializeState: done");
  s.valid = true;
  return true;
}

// ---------------------------------------------------------------------------
// Discover the Func<object,object,int> delegate class and a template delegate
// from an existing SortFunction in the game's sort option list. We clone this
// delegate and swap its method pointer, which is more reliable than calling
// the delegate .ctor (which fails for static methods in IL2CPP).
// ---------------------------------------------------------------------------
Il2CppClass* DiscoverFuncClassFromList(void* existingList, Il2CppObject*& outTemplateDelegate)
{
  outTemplateDelegate = nullptr;
  if (!existingList) return nullptr;

  auto* listClass = il2cpp_object_get_class(reinterpret_cast<Il2CppObject*>(existingList));
  if (!listClass) return nullptr;

  auto* getItem = il2cpp_class_get_method_from_name(listClass, "get_Item", 1);
  if (!getItem) return nullptr;

  Il2CppException* exc = nullptr;
  int32_t          index = 0;
  void*            idxArgs[1] = {&index};
  auto* firstOption = il2cpp_runtime_invoke(getItem, existingList, idxArgs, &exc);
  if (exc || !firstOption) return nullptr;

  auto& s = State();
  auto  sortingOptionField = s.processingOption->GetField("_sortingOption");
  if (!sortingOptionField.isValidHelper()) return nullptr;

  auto* sortFn = *reinterpret_cast<void**>(reinterpret_cast<char*>(firstOption) + sortingOptionField.offset());
  if (!sortFn) return nullptr;

  auto* ascendingDelegate =
      *reinterpret_cast<Il2CppObject**>(reinterpret_cast<char*>(sortFn) + s.sortFunctionAscending->offset());
  if (!ascendingDelegate) return nullptr;

  outTemplateDelegate = ascendingDelegate;
  return il2cpp_object_get_class(ascendingDelegate);
}

Il2CppClass* EnsureFuncClass(void* existingList)
{
  auto& s = State();
  if (s.funcClass) return s.funcClass;

  Il2CppObject* templateDel = nullptr;
  s.funcClass = DiscoverFuncClassFromList(existingList, templateDel);
  if (s.funcClass) {
    s.funcCtor = il2cpp_class_get_method_from_name(s.funcClass, ".ctor", 2);
    spdlog::info("[OfficerSort] Discovered Func<object,object,int> class from existing delegate");
  }
  return s.funcClass;
}

// ---------------------------------------------------------------------------
// Create a Func<object,object,int> delegate by cloning an existing working
// delegate and swapping its method_ptr/invoke_impl/method to point at the
// target sort method. This avoids the delegate .ctor which fails for static
// methods in IL2CPP ("Delegate to an instance method cannot have null 'this'").
// ---------------------------------------------------------------------------
Il2CppObject* CreateSortDelegate(const MethodInfo* sortMethod, void* existingList, Il2CppGCHandle& outHandle)
{
  auto& s = State();
  if (!s.funcClass || !sortMethod || !existingList) return nullptr;

  // Get a template delegate from the existing list.
  Il2CppObject* templateDel = nullptr;
  DiscoverFuncClassFromList(existingList, templateDel);
  if (!templateDel) {
    spdlog::warn("[OfficerSort] no template delegate available");
    return nullptr;
  }

  // Allocate a new delegate of the same class.
  auto* del = il2cpp_object_new(s.funcClass);
  if (!del) {
    spdlog::warn("[OfficerSort] il2cpp_object_new failed for sort delegate");
    return nullptr;
  }

  outHandle = il2cpp_gchandle_new(del, false);
  if (!outHandle) {
    spdlog::warn("[OfficerSort] gchandle_new failed for sort delegate");
    return nullptr;
  }

  // The template delegate is a closed delegate (invoke_impl == method_ptr,
  // invoke_impl_this == target). Closed delegates call invoke_impl(invoke_impl_this, arg1, arg2),
  // passing the target as the first arg. But our sort methods are static and take (arg1, arg2)
  // with NO hidden MethodInfo* parameter (confirmed by disassembly: mov rdi,rcx / mov rsi,rdx).
  // So we can't use the closed delegate pattern.
  //
  // Instead, call the delegate .ctor via its invoker_method (bypassing
  // il2cpp_runtime_invoke which fails for static methods). The .ctor signature
  // is (Il2CppDelegate* this, Il2CppObject* target, intptr_t method, MethodInfo* hidden).
  // For a static method, target=null and method=MethodInfo*.
  auto* ctor = il2cpp_class_get_method_from_name(s.funcClass, ".ctor", 2);
  if (ctor && ctor->invoker_method) {
    spdlog::info("[OfficerSort] calling delegate .ctor via invoker_method, target=null, method={}",
                 static_cast<const void*>(sortMethod));
    void* ctorArgs[2] = {nullptr, (void*)&sortMethod};
    ctor->invoker_method(ctor->methodPointer, ctor, del, ctorArgs, nullptr);
    spdlog::info("[OfficerSort] delegate .ctor returned, method_ptr={}, invoke_impl={}, invoke_impl_this={}",
                 static_cast<const void*>(reinterpret_cast<Il2CppDelegate*>(del)->method_ptr),
                 static_cast<const void*>(reinterpret_cast<Il2CppDelegate*>(del)->invoke_impl),
                 static_cast<const void*>(reinterpret_cast<Il2CppDelegate*>(del)->invoke_impl_this));

    // Dump the invoke stub's first 32 bytes to understand the calling convention.
    auto* invokeStub = reinterpret_cast<const uint8_t*>(reinterpret_cast<Il2CppDelegate*>(del)->invoke_impl);
    if (invokeStub) {
      spdlog::info("[OfficerSort] invoke stub at {}: {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x}",
                   static_cast<const void*>(invokeStub),
                   invokeStub[0], invokeStub[1], invokeStub[2], invokeStub[3], invokeStub[4], invokeStub[5], invokeStub[6], invokeStub[7],
                   invokeStub[8], invokeStub[9], invokeStub[10], invokeStub[11], invokeStub[12], invokeStub[13], invokeStub[14], invokeStub[15],
                   invokeStub[16], invokeStub[17], invokeStub[18], invokeStub[19], invokeStub[20], invokeStub[21], invokeStub[22], invokeStub[23],
                   invokeStub[24], invokeStub[25], invokeStub[26], invokeStub[27], invokeStub[28], invokeStub[29], invokeStub[30], invokeStub[31]);
    }
    return del;
  }

  spdlog::warn("[OfficerSort] no .ctor on Func class, falling back to clone");
  auto* src = reinterpret_cast<Il2CppDelegate*>(templateDel);
  auto* dst = reinterpret_cast<Il2CppDelegate*>(del);
  *dst = *src;
  dst->method_ptr        = sortMethod->methodPointer;
  dst->method            = sortMethod;
  dst->target            = nullptr;
  dst->method_is_virtual = false;
  dst->invoke_impl_this  = del;

  spdlog::info("[OfficerSort] CreateSortDelegate: cloned delegate, method_ptr={}",
               static_cast<const void*>(sortMethod->methodPointer));
  return del;
}

// ---------------------------------------------------------------------------
// Build a SortFunction for the below-deck sort.
// forRoster=true: roster screen defaults to Ascending, so _ascending must
//   produce BDA-first (use RosterAscendingWrapper/DescendingWrapper).
// forRoster=false: assignment screen defaults to Descending, so _descending
//   must produce BDA-first (use AssignmentAscendingWrapper/DescendingWrapper).
// ---------------------------------------------------------------------------
Il2CppObject* CreateBelowDeckSortFunction(void* existingList, const char* displayKey,
                                          bool forRoster, Il2CppGCHandle& outHandle)
{
  auto& s = State();

  // Create two delegates — one for _ascending, one for _descending.
  // We create them via the .ctor invoker (same as before), then replace
  // their method_ptr with our C++ wrapper functions that combine the
  // unlocked-state predicate with the BDA predicate.
  spdlog::info("[OfficerSort] CreateBelowDeckSortFunction: creating delegates (forRoster={})", forRoster);
  Il2CppGCHandle ascHandle  = 0;
  Il2CppGCHandle descHandle = 0;
  auto*           ascDelegate  = CreateSortDelegate(s.ascendingMethod, existingList, ascHandle);
  auto*           descDelegate = CreateSortDelegate(s.descendingMethod, existingList, descHandle);
  if (!ascDelegate || !descDelegate) {
    if (ascHandle)  il2cpp_gchandle_free(ascHandle);
    if (descHandle) il2cpp_gchandle_free(descHandle);
    spdlog::warn("[OfficerSort] failed to create below-deck sort delegates");
    return nullptr;
  }

  // Replace method_ptr with our combined wrappers.
  // The delegate invoke stub calls method_ptr(obj1, obj2, delegate).
  auto* ascDel  = reinterpret_cast<Il2CppDelegate*>(ascDelegate);
  auto* descDel = reinterpret_cast<Il2CppDelegate*>(descDelegate);
  if (forRoster) {
    ascDel->method_ptr  = reinterpret_cast<Il2CppMethodPointer>(&RosterAscendingWrapper);
    descDel->method_ptr = reinterpret_cast<Il2CppMethodPointer>(&RosterDescendingWrapper);
  } else {
    ascDel->method_ptr  = reinterpret_cast<Il2CppMethodPointer>(&AssignmentAscendingWrapper);
    descDel->method_ptr = reinterpret_cast<Il2CppMethodPointer>(&AssignmentDescendingWrapper);
  }
  spdlog::info("[OfficerSort] CreateBelowDeckSortFunction: wrappers installed");

  spdlog::info("[OfficerSort] CreateBelowDeckSortFunction: allocating SortFunction");
  auto* sortFn = il2cpp_object_new(s.sortFunction->get_cls());
  if (!sortFn) {
    il2cpp_gchandle_free(ascHandle);
    il2cpp_gchandle_free(descHandle);
    spdlog::warn("[OfficerSort] il2cpp_object_new failed for SortFunction");
    return nullptr;
  }

  outHandle = il2cpp_gchandle_new(sortFn, false);
  if (!outHandle) {
    il2cpp_gchandle_free(ascHandle);
    il2cpp_gchandle_free(descHandle);
    return nullptr;
  }

  spdlog::info("[OfficerSort] CreateBelowDeckSortFunction: setting fields");
  auto* displayKeyStr = il2cpp_string_new(displayKey);
  *reinterpret_cast<void**>(reinterpret_cast<char*>(sortFn) + s.sortFunctionDisplayKey->offset()) = displayKeyStr;
  *reinterpret_cast<void**>(reinterpret_cast<char*>(sortFn) + s.sortFunctionAscending->offset())  = ascDelegate;
  *reinterpret_cast<void**>(reinterpret_cast<char*>(sortFn) + s.sortFunctionDescending->offset()) = descDelegate;
  spdlog::info("[OfficerSort] CreateBelowDeckSortFunction: done");

  // The SortFunction now references the delegates; release our local roots.
  if (ascHandle)  il2cpp_gchandle_free(ascHandle);
  if (descHandle) il2cpp_gchandle_free(descHandle);

  return sortFn;
}

// ---------------------------------------------------------------------------
// Check whether the list already contains a below-deck option (defensive
// double-insert guard). Reads each option's _displayKey and compares.
// ---------------------------------------------------------------------------
bool ListAlreadyHasBelowDeck(void* list)
{
  if (!list) return false;
  auto* listClass = il2cpp_object_get_class(reinterpret_cast<Il2CppObject*>(list));
  if (!listClass) return false;

  auto* getCount = il2cpp_class_get_method_from_name(listClass, "get_Count", 0);
  auto* getItem  = il2cpp_class_get_method_from_name(listClass, "get_Item", 1);
  if (!getCount || !getItem) return false;

  Il2CppException* exc = nullptr;
  auto* countObj = il2cpp_runtime_invoke(getCount, list, nullptr, &exc);
  if (exc || !countObj) return false;
  auto count = *reinterpret_cast<int32_t*>(il2cpp_object_unbox(countObj));

  auto& s = State();
  auto  displayKeyField = s.processingOption->GetField("_displayKey");
  if (!displayKeyField.isValidHelper()) return false;

  // Log existing display keys for debugging.
  for (int32_t i = 0; i < count; ++i) {
    void* idxArgs[1] = {&i};
    exc = nullptr;
    auto* option = il2cpp_runtime_invoke(getItem, list, idxArgs, &exc);
    if (exc || !option) continue;
    auto* keyPtr =
        *reinterpret_cast<Il2CppString**>(reinterpret_cast<char*>(option) + displayKeyField.offset());
    if (!keyPtr) continue;
    // Convert UTF-16 to ASCII for logging.
    char buf[256] = {0};
    auto len = keyPtr->length;
    if (len > 255) len = 255;
    for (int32_t j = 0; j < len; ++j) buf[j] = (char)keyPtr->chars[j];
    spdlog::info("[OfficerSort] existing option[{}] displayKey = '{}'", i, buf);
  }

  for (int32_t i = 0; i < count; ++i) {
    void* idxArgs[1] = {&i};
    exc = nullptr;
    auto* option = il2cpp_runtime_invoke(getItem, list, idxArgs, &exc);
    if (exc || !option) continue;

    auto* keyPtr =
        *reinterpret_cast<Il2CppString**>(reinterpret_cast<char*>(option) + displayKeyField.offset());
    if (!keyPtr) continue;

    // Compare UTF-16 chars against both possible keys.
    auto len = keyPtr->length;
    auto matchesKey = [&](const char* key) {
      auto keyLen = (int32_t)strlen(key);
      if (len != keyLen) return false;
      for (int32_t j = 0; j < len; ++j) {
        if ((char)keyPtr->chars[j] != key[j]) return false;
      }
      return true;
    };
    if (matchesKey(kBelowDeckKey) || matchesKey(kBelowDeckAssignmentKey)) return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// Append the below-deck ProcessingOption to the returned list.
// If sortersList is non-null, also adds the raw SortFunction to that list
// (needed for the roster screen which builds its SortComparer from
// _rosterSorters, not _rosterOptions).
// ---------------------------------------------------------------------------
void AppendBelowDeckOption(void* list, const char* displayKey, bool forRoster, void* sortersList = nullptr)
{
  if (!list) {
    spdlog::warn("[OfficerSort] AppendBelowDeckOption: list is null");
    return;
  }

  auto& s = State();
  if (!s.valid) return;

  spdlog::info("[OfficerSort] AppendBelowDeckOption: discovering Func class, displayKey={}", displayKey);
  // Lazily discover the Func<object,object,int> class from the existing list.
  if (!s.funcClass) {
    EnsureFuncClass(list);
    if (!s.funcClass) {
      spdlog::warn("[OfficerSort] could not discover Func<object,object,int> class; skipping below-deck option");
      return;
    }
  }

  if (ListAlreadyHasBelowDeck(list)) {
    spdlog::info("[OfficerSort] AppendBelowDeckOption: already has below-deck option, skipping");
    return;
  }

  // Create the SortFunction first, so we can also add it to _rosterSorters.
  spdlog::info("[OfficerSort] AppendBelowDeckOption: creating SortFunction");
  Il2CppGCHandle sortFnHandle = 0;
  auto* sortFn = CreateBelowDeckSortFunction(list, displayKey, forRoster, sortFnHandle);
  if (!sortFn) {
    if (sortFnHandle) il2cpp_gchandle_free(sortFnHandle);
    return;
  }

  // If a sorters list was provided, add the SortFunction to it.
  if (sortersList) {
    spdlog::info("[OfficerSort] AppendBelowDeckOption: adding SortFunction to sorters list");
    auto* sortersListClass = il2cpp_object_get_class(reinterpret_cast<Il2CppObject*>(sortersList));
    auto* sortersAddMethod = sortersListClass ? il2cpp_class_get_method_from_name(sortersListClass, "Add", 1) : nullptr;
    if (sortersAddMethod) {
      void* sortersAddArgs[1] = {sortFn};
      Il2CppException* sortersExc = nullptr;
      il2cpp_runtime_invoke(sortersAddMethod, sortersList, sortersAddArgs, &sortersExc);
      if (sortersExc) {
        spdlog::warn("[OfficerSort] sorters list.Add raised an exception");
      } else {
        spdlog::info("[OfficerSort] AppendBelowDeckOption: successfully added SortFunction to sorters list");
      }
    } else {
      spdlog::warn("[OfficerSort] sorters list has no Add method");
    }
  }

  // Now wrap the SortFunction in a ProcessingOption.
  spdlog::info("[OfficerSort] AppendBelowDeckOption: creating ProcessingOption");
  auto& s2 = State();
  auto* option = il2cpp_object_new(s2.processingOption->get_cls());
  if (!option) {
    spdlog::warn("[OfficerSort] il2cpp_object_new failed for ProcessingOption");
    il2cpp_gchandle_free(sortFnHandle);
    return;
  }

  Il2CppGCHandle optionHandle = il2cpp_gchandle_new(option, false);
  if (!optionHandle) {
    il2cpp_gchandle_free(sortFnHandle);
    return;
  }

  spdlog::info("[OfficerSort] AppendBelowDeckOption: setting ProcessingOption fields, displayKey={}", displayKey);
  auto* idStr = il2cpp_string_new(displayKey);
  *reinterpret_cast<void**>(reinterpret_cast<char*>(option) + s2.processingOptionDisplayKey->offset())    = idStr;
  *reinterpret_cast<void**>(reinterpret_cast<char*>(option) + s2.processingOptionSortingOption->offset()) = sortFn;
  spdlog::info("[OfficerSort] AppendBelowDeckOption: ProcessingOption fields set");

  spdlog::info("[OfficerSort] AppendBelowDeckOption: adding ProcessingOption to list");
  auto* listClass = il2cpp_object_get_class(reinterpret_cast<Il2CppObject*>(list));
  auto* addMethod = listClass ? il2cpp_class_get_method_from_name(listClass, "Add", 1) : nullptr;
  if (!addMethod) {
    spdlog::warn("[OfficerSort] list has no Add method; cannot insert below-deck option");
    il2cpp_gchandle_free(optionHandle);
    il2cpp_gchandle_free(sortFnHandle);
    return;
  }

  void* addArgs[1] = {option};
  Il2CppException* exc = nullptr;
  il2cpp_runtime_invoke(addMethod, list, addArgs, &exc);
  if (exc) {
    spdlog::warn("[OfficerSort] list.Add raised an exception; below-deck option not inserted");
  } else {
    spdlog::info("[OfficerSort] AppendBelowDeckOption: successfully added to list");
  }

  il2cpp_gchandle_free(optionHandle);
  il2cpp_gchandle_free(sortFnHandle);
}

// ---------------------------------------------------------------------------
// Hooks. After the game populates its sort option lists, we append ours.
// InitializeOfficerSorters/InitializeAssignmentSorters are void instance methods
// on OfficerSortGenerators that fill _rosterOptions/_assignmentOptions.
// ---------------------------------------------------------------------------
void InitializeOfficerSorters_Hook(auto original, void* _this)
{
  static std::once_flag once;
  std::call_once(once, [] { spdlog::info("[OfficerSort] InitializeOfficerSorters hook fired"); });
  original(_this);
  auto& s = State();
  auto* optionsList = *reinterpret_cast<void**>(reinterpret_cast<char*>(_this) + s.rosterOptionsField->offset());
  auto* sortersList = *reinterpret_cast<void**>(reinterpret_cast<char*>(_this) + s.rosterSortersField->offset());
  AppendBelowDeckOption(optionsList, kBelowDeckKey, true, sortersList);
}

void InitializeAssignmentSorters_Hook(auto original, void* _this)
{
  static std::once_flag once;
  std::call_once(once, [] { spdlog::info("[OfficerSort] InitializeAssignmentSorters hook fired"); });
  original(_this);
  auto& s = State();
  auto* list = *reinterpret_cast<void**>(reinterpret_cast<char*>(_this) + s.assignmentOptionsField->offset());
  AppendBelowDeckOption(list, kBelowDeckAssignmentKey, false);
}

} // namespace

// ---------------------------------------------------------------------------
// Installation entry point.
// ---------------------------------------------------------------------------
void InstallOfficerSortHooks()
{
  if (!InitializeState()) {
    spdlog::error("[OfficerSort] initialization failed; hooks not installed");
    return;
  }

  auto& s = State();

  spdlog::info("[OfficerSort] resolving InitializeOfficerSorters");
  auto rosterInitPtr = s.officerSortGenerators->GetMethod("InitializeOfficerSorters", 0);
  if (!rosterInitPtr) {
    ErrorMsg::MissingMethod("OfficerSortGenerators", "InitializeOfficerSorters");
    return;
  }
  spdlog::info("[OfficerSort] InitializeOfficerSorters ptr = {}", static_cast<const void*>(rosterInitPtr));

  spdlog::info("[OfficerSort] resolving InitializeAssignmentSorters");
  auto assignmentInitPtr = s.officerSortGenerators->GetMethod("InitializeAssignmentSorters", 0);
  if (!assignmentInitPtr) {
    ErrorMsg::MissingMethod("OfficerSortGenerators", "InitializeAssignmentSorters");
    return;
  }
  spdlog::info("[OfficerSort] InitializeAssignmentSorters ptr = {}", static_cast<const void*>(assignmentInitPtr));

  spdlog::info("[OfficerSort] installing detours");
  try {
    SPUD_STATIC_DETOUR(rosterInitPtr, InitializeOfficerSorters_Hook);
    SPUD_STATIC_DETOUR(assignmentInitPtr, InitializeAssignmentSorters_Hook);
  } catch (const std::exception& e) {
    spdlog::error("[OfficerSort] detour installation failed: {}", e.what());
    return;
  } catch (...) {
    spdlog::error("[OfficerSort] detour installation failed (unknown exception)");
    return;
  }

  spdlog::info("Officer sort: restored Below Deck Ability sort option");
}
