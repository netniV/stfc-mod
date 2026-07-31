#include "config.h"
#include "errormsg.h"

#include <il2cpp/il2cpp-functions.h>
#include <il2cpp/il2cpp_helper.h>

#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <cstring>
#include <mutex>

// Restore the "Below Deck Ability" (BDA) sort option that was removed from the
// game UI in m93.1.  The underlying comparison functions
// (SortingPredicates.OfficerSortByBelowDeckAbilityAscending/Descending) and
// the SortFunction / SortComparer.ProcessingOption classes still exist, so we
// hook OfficerSortGenerators.InitializeOfficerSorters /
// InitializeAssignmentSorters and append a BDA ProcessingOption after the game
// populates its own sort lists.
//
// Sort order (3 levels):
//   1. Unlocked state  – available officers first, locked at bottom
//   2. BDA status      – BDA officers first (default) or non-BDA first
//   3. Officer index   – newer officers first (tiebreaker)
//
// All IL2CPP classes/methods/fields are resolved by name at runtime, so the
// patch is resilient to minor game updates.  Managed objects created during
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
// Cached IL2CPP resolution state (resolved once during InstallOfficerSortHooks).
// The delegate invoke stub calls method_ptr(obj1, obj2, delegate).
// The sort methods take (obj1, obj2) and ignore the 3rd arg.
// ---------------------------------------------------------------------------
using SortMethodPtr = int (*)(void* obj1, void* obj2, void* delegatePtr);

struct OfficerSortState {
  IL2CppClassHelper* sortingPredicates     = nullptr; // Digit.Client.Core.SortingPredicates
  IL2CppClassHelper* sortFunction          = nullptr; // Digit.Client.Core.SortFunction
  IL2CppClassHelper* processingOption      = nullptr; // Digit.Client.Sorting.SortComparer.ProcessingOption
  IL2CppClassHelper* officerSortGenerators = nullptr; // Digit.Prime.Officers.OfficerSortGenerators

  const MethodInfo* ascendingMethod      = nullptr; // OfficerSortByBelowDeckAbilityAscending
  const MethodInfo* descendingMethod     = nullptr; // OfficerSortByBelowDeckAbilityDescending
  const MethodInfo* unlockedStateMethod  = nullptr; // OfficerSortByUnlockedStateAscending
  const MethodInfo* indexMethod          = nullptr; // OfficerSortByIndexAscending
  const MethodInfo* processingOptionCtor = nullptr; // .ctor(string, FilterFunction, SortFunction, string, bool)

  // Resolved method pointers for C++ wrapper functions.
  SortMethodPtr unlockedStatePtr = nullptr;
  SortMethodPtr bdaAscPtr        = nullptr;
  SortMethodPtr bdaDescPtr       = nullptr;
  SortMethodPtr indexAscPtr      = nullptr;

  IL2CppFieldHelper* sortFunctionDisplayKey  = nullptr;
  IL2CppFieldHelper* sortFunctionAscending   = nullptr;
  IL2CppFieldHelper* sortFunctionDescending  = nullptr;
  IL2CppFieldHelper* rosterSortersField      = nullptr; // _rosterSorters (List<SortFunction>)
  IL2CppFieldHelper* rosterOptionsField      = nullptr; // _rosterOptions
  IL2CppFieldHelper* assignmentOptionsField  = nullptr; // _assignmentOptions
  IL2CppFieldHelper* processingOptionDisplayKey    = nullptr; // _displayKey
  IL2CppFieldHelper* processingOptionSortingOption = nullptr; // _sortingOption

  Il2CppClass* funcClass = nullptr; // Func<object,object,int> (discovered at runtime)

  bool valid = false;
};

OfficerSortState& State()
{
  static OfficerSortState s;
  return s;
}

// ---------------------------------------------------------------------------
// Combined sort wrappers.
//
// The game's GenerateSortFunction prepends OfficerSortByUnlockedStateAscending
// to every sort predicate array, so available officers always sort to the top.
// We replicate this in C++ by creating wrapper functions that first call the
// unlocked-state predicate, then the BDA predicate, then the index tiebreaker.
//
// IMPORTANT: SortComparer.Compare always calls the _ascending delegate.
// For Descending sort direction it negates the _ascending result (neg eax).
// The _descending delegate is never called.  All logic must therefore live in
// the *Ascending wrappers, designed so that:
//   - direct call  = ascending behavior
//   - negated call = descending behavior
// ---------------------------------------------------------------------------

// Roster _ascending (called directly for Ascending, negated for Descending):
//   Ascending:  available -> BDA-first -> newer-officers-first
//   Descending: negated   -> non-BDA-first -> older-officers-first
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
//   If called directly: available -> non-BDA-first -> newer-officers-first
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
//   Ascending:  available -> non-BDA-first -> older-officers-first
//   Descending: negated   -> BDA-first -> newer-officers-first
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
//   If called directly: available -> BDA-first -> older-officers-first
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
// Resolve all required IL2CPP classes/methods/fields. Called once.
// ---------------------------------------------------------------------------
bool InitializeState()
{
  auto& s = State();

  s.sortingPredicates = new IL2CppClassHelper(
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "SortingPredicates"));
  if (!s.sortingPredicates->get_cls()) {
    ErrorMsg::MissingHelper("Digit.Client.Core", "SortingPredicates");
    return false;
  }

  s.sortFunction = new IL2CppClassHelper(
      il2cpp_get_class_helper("Digit.Client.PrimeLib.Runtime", "Digit.Client.Core", "SortFunction"));
  if (!s.sortFunction->get_cls()) {
    ErrorMsg::MissingHelper("Digit.Client.Core", "SortFunction");
    return false;
  }

  auto sortComparer = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Sorting", "SortComparer");
  if (!sortComparer.get_cls()) {
    ErrorMsg::MissingHelper("Digit.Client.Sorting", "SortComparer");
    return false;
  }

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
  s.processingOption = new IL2CppClassHelper(processingOptionCls);
  if (!s.processingOption->get_cls()) {
    ErrorMsg::MissingHelper("Digit.Client.Sorting", "SortComparer.ProcessingOption");
    return false;
  }

  s.officerSortGenerators = new IL2CppClassHelper(
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Officers", "OfficerSortGenerators"));
  if (!s.officerSortGenerators->get_cls()) {
    ErrorMsg::MissingHelper("Digit.Prime.Officers", "OfficerSortGenerators");
    return false;
  }

  // Resolve the three sort methods.
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

  // Unlocked-state predicate: keeps available officers at the top.
  s.unlockedStateMethod = s.sortingPredicates->GetMethodInfo("OfficerSortByUnlockedStateAscending", 2);
  if (!s.unlockedStateMethod) {
    spdlog::warn("[OfficerSort] OfficerSortByUnlockedStateAscending not found; locked officers may not sort to bottom");
  } else {
    s.unlockedStatePtr = reinterpret_cast<SortMethodPtr>(s.unlockedStateMethod->methodPointer);
  }

  // Index predicate: tertiary tiebreaker so officers within the same BDA tier
  // are sorted by internal officer index.
  s.indexMethod = s.sortingPredicates->GetMethodInfo("OfficerSortByIndexAscending", 2);
  if (!s.indexMethod) {
    spdlog::warn("[OfficerSort] OfficerSortByIndexAscending not found; no index tiebreaker");
  } else {
    s.indexAscPtr = reinterpret_cast<SortMethodPtr>(s.indexMethod->methodPointer);
  }

  s.bdaAscPtr  = reinterpret_cast<SortMethodPtr>(s.ascendingMethod->methodPointer);
  s.bdaDescPtr = reinterpret_cast<SortMethodPtr>(s.descendingMethod->methodPointer);

  s.processingOptionCtor = s.processingOption->GetMethodInfo(".ctor", 5);
  if (!s.processingOptionCtor) {
    ErrorMsg::MissingMethod("SortComparer.ProcessingOption", ".ctor(5)");
    return false;
  }

  // SortFunction fields (public, should always resolve).
  s.sortFunctionDisplayKey  = new IL2CppFieldHelper(s.sortFunction->GetField("_displayKey"));
  s.sortFunctionAscending   = new IL2CppFieldHelper(s.sortFunction->GetField("_ascending"));
  s.sortFunctionDescending  = new IL2CppFieldHelper(s.sortFunction->GetField("_descending"));

  // OfficerSortGenerators fields.
  s.rosterSortersField     = new IL2CppFieldHelper(s.officerSortGenerators->GetField("_rosterSorters"));
  s.rosterOptionsField     = new IL2CppFieldHelper(s.officerSortGenerators->GetField("_rosterOptions"));
  s.assignmentOptionsField = new IL2CppFieldHelper(s.officerSortGenerators->GetField("_assignmentOptions"));
  if (!s.rosterOptionsField->isValidHelper() || !s.assignmentOptionsField->isValidHelper()) {
    spdlog::error("[OfficerSort] OfficerSortGenerators field layout changed");
    return false;
  }

  // ProcessingOption fields.
  s.processingOptionDisplayKey    = new IL2CppFieldHelper(s.processingOption->GetField("_displayKey"));
  s.processingOptionSortingOption = new IL2CppFieldHelper(s.processingOption->GetField("_sortingOption"));
  if (!s.processingOptionDisplayKey->isValidHelper() || !s.processingOptionSortingOption->isValidHelper()) {
    spdlog::error("[OfficerSort] ProcessingOption field layout changed");
    return false;
  }

  s.valid = true;
  return true;
}

// ---------------------------------------------------------------------------
// Discover the Func<object,object,int> delegate class from an existing
// SortFunction in the game's sort option list. We need the class to allocate
// new delegates of the same type.
// ---------------------------------------------------------------------------
Il2CppClass* DiscoverFuncClassFromList(void* existingList)
{
  if (!existingList) return nullptr;

  auto* listClass = il2cpp_object_get_class(reinterpret_cast<Il2CppObject*>(existingList));
  if (!listClass) return nullptr;

  auto* getItem = il2cpp_class_get_method_from_name(listClass, "get_Item", 1);
  if (!getItem) return nullptr;

  Il2CppException* exc     = nullptr;
  int32_t          index   = 0;
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

  return il2cpp_object_get_class(ascendingDelegate);
}

// ---------------------------------------------------------------------------
// Create a Func<object,object,int> delegate pointing at the given static sort
// method. The delegate .ctor is called via its invoker_method to bypass
// il2cpp_runtime_invoke, which fails for static methods in IL2CPP
// ("Delegate to an instance method cannot have null 'this'").
// ---------------------------------------------------------------------------
Il2CppObject* CreateSortDelegate(const MethodInfo* sortMethod, Il2CppGCHandle& outHandle)
{
  auto& s = State();
  if (!s.funcClass || !sortMethod) return nullptr;

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

  // .ctor signature: (Il2CppDelegate* this, Il2CppObject* target, intptr_t method, MethodInfo* hidden).
  // For a static method, target=null and method=MethodInfo*.
  auto* ctor = il2cpp_class_get_method_from_name(s.funcClass, ".ctor", 2);
  if (ctor && ctor->invoker_method) {
    void* ctorArgs[2] = {nullptr, (void*)&sortMethod};
    ctor->invoker_method(ctor->methodPointer, ctor, del, ctorArgs, nullptr);
    return del;
  }

  spdlog::warn("[OfficerSort] no .ctor on Func class; cannot create delegate");
  return nullptr;
}

// ---------------------------------------------------------------------------
// Build a SortFunction for the below-deck sort.
//
// forRoster=true:  roster screen defaults to Ascending, so _ascending must
//                  produce BDA-first (use RosterAscendingWrapper).
// forRoster=false: assignment screen defaults to Descending, so _ascending
//                  must produce non-BDA-first (SortComparer negates it for
//                  the default Descending direction, yielding BDA-first).
// ---------------------------------------------------------------------------
Il2CppObject* CreateBelowDeckSortFunction(void* existingList, const char* displayKey,
                                          bool forRoster, Il2CppGCHandle& outHandle)
{
  auto& s = State();

  // Create two delegates, then replace their method_ptr with our C++ wrappers.
  Il2CppGCHandle ascHandle  = 0;
  Il2CppGCHandle descHandle = 0;
  auto* ascDelegate  = CreateSortDelegate(s.ascendingMethod, ascHandle);
  auto* descDelegate = CreateSortDelegate(s.descendingMethod, descHandle);
  if (!ascDelegate || !descDelegate) {
    if (ascHandle)  il2cpp_gchandle_free(ascHandle);
    if (descHandle) il2cpp_gchandle_free(descHandle);
    spdlog::warn("[OfficerSort] failed to create below-deck sort delegates");
    return nullptr;
  }

  auto* ascDel  = reinterpret_cast<Il2CppDelegate*>(ascDelegate);
  auto* descDel = reinterpret_cast<Il2CppDelegate*>(descDelegate);
  if (forRoster) {
    ascDel->method_ptr  = reinterpret_cast<Il2CppMethodPointer>(&RosterAscendingWrapper);
    descDel->method_ptr = reinterpret_cast<Il2CppMethodPointer>(&RosterDescendingWrapper);
  } else {
    ascDel->method_ptr  = reinterpret_cast<Il2CppMethodPointer>(&AssignmentAscendingWrapper);
    descDel->method_ptr = reinterpret_cast<Il2CppMethodPointer>(&AssignmentDescendingWrapper);
  }

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

  auto* displayKeyStr = il2cpp_string_new(displayKey);
  *reinterpret_cast<void**>(reinterpret_cast<char*>(sortFn) + s.sortFunctionDisplayKey->offset()) = displayKeyStr;
  *reinterpret_cast<void**>(reinterpret_cast<char*>(sortFn) + s.sortFunctionAscending->offset())  = ascDelegate;
  *reinterpret_cast<void**>(reinterpret_cast<char*>(sortFn) + s.sortFunctionDescending->offset()) = descDelegate;

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

  // Lazily discover the Func<object,object,int> class from the existing list.
  if (!s.funcClass) {
    s.funcClass = DiscoverFuncClassFromList(list);
    if (!s.funcClass) {
      spdlog::warn("[OfficerSort] could not discover Func<object,object,int> class; skipping below-deck option");
      return;
    }
  }

  if (ListAlreadyHasBelowDeck(list)) return;

  // Create the SortFunction first, so we can also add it to _rosterSorters.
  Il2CppGCHandle sortFnHandle = 0;
  auto* sortFn = CreateBelowDeckSortFunction(list, displayKey, forRoster, sortFnHandle);
  if (!sortFn) {
    if (sortFnHandle) il2cpp_gchandle_free(sortFnHandle);
    return;
  }

  // If a sorters list was provided, add the SortFunction to it.
  if (sortersList) {
    auto* sortersListClass = il2cpp_object_get_class(reinterpret_cast<Il2CppObject*>(sortersList));
    auto* sortersAddMethod = sortersListClass ? il2cpp_class_get_method_from_name(sortersListClass, "Add", 1) : nullptr;
    if (sortersAddMethod) {
      void* sortersAddArgs[1] = {sortFn};
      Il2CppException* sortersExc = nullptr;
      il2cpp_runtime_invoke(sortersAddMethod, sortersList, sortersAddArgs, &sortersExc);
      if (sortersExc) spdlog::warn("[OfficerSort] sorters list.Add raised an exception");
    }
  }

  // Now wrap the SortFunction in a ProcessingOption.
  auto* option = il2cpp_object_new(s.processingOption->get_cls());
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

  auto* idStr = il2cpp_string_new(displayKey);
  *reinterpret_cast<void**>(reinterpret_cast<char*>(option) + s.processingOptionDisplayKey->offset())    = idStr;
  *reinterpret_cast<void**>(reinterpret_cast<char*>(option) + s.processingOptionSortingOption->offset()) = sortFn;

  // Add the ProcessingOption to the list.
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
  if (exc) spdlog::warn("[OfficerSort] list.Add raised an exception; below-deck option not inserted");

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
  original(_this);
  auto& s = State();
  auto* optionsList = *reinterpret_cast<void**>(reinterpret_cast<char*>(_this) + s.rosterOptionsField->offset());
  auto* sortersList = *reinterpret_cast<void**>(reinterpret_cast<char*>(_this) + s.rosterSortersField->offset());
  AppendBelowDeckOption(optionsList, kBelowDeckKey, true, sortersList);
}

void InitializeAssignmentSorters_Hook(auto original, void* _this)
{
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

  auto rosterInitPtr = s.officerSortGenerators->GetMethod("InitializeOfficerSorters", 0);
  if (!rosterInitPtr) {
    ErrorMsg::MissingMethod("OfficerSortGenerators", "InitializeOfficerSorters");
    return;
  }

  auto assignmentInitPtr = s.officerSortGenerators->GetMethod("InitializeAssignmentSorters", 0);
  if (!assignmentInitPtr) {
    ErrorMsg::MissingMethod("OfficerSortGenerators", "InitializeAssignmentSorters");
    return;
  }

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
