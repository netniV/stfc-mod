# Restore Below Deck Sort Option in Officer Roster & Assignment Views

## Summary

The "Below Deck Ability" (BDA) sort option was removed from the game UI in m93.1.
This patch restores it by hooking `OfficerSortGenerators.InitializeOfficerSorters`
and `InitializeAssignmentSorters` to append a BDA `ProcessingOption` after the
game populates its own sort lists, using the still-present native comparison
functions.

## What was removed in m93.1

- `OfficerSortGenerators._belowDeckAbilitySortId` (roster sort ID field) — removed
- `OfficerSortGenerators._assignBelowDeckAbilityId` (assignment sort ID field) — removed
- String literals `"_below_deck_ability"` and `"below_deck_ability"` — removed from binary
- The `InitializeOfficerSorters()` / `InitializeAssignmentSorters()` methods no longer
  register the below-deck sort

## What still exists in m93.1

- `SortingPredicates.OfficerSortByBelowDeckAbilityAscending` — RVA `0x8047F0`
- `SortingPredicates.OfficerSortByBelowDeckAbilityDescending` — RVA `0x804A20`
- `SortingPredicates.OfficerSortByUnlockedStateAscending` — keeps available officers at top
- `SortingPredicates.OfficerSortByIndexAscending` — tertiary tiebreaker by officer index
- `SortFunction` class — public fields `_displayKey` (string), `_ascending` (Func), `_descending` (Func)
- `SortComparer.ProcessingOption` — nested class with `_displayKey` and `_sortingOption` fields
- `OfficerSortGenerators` — has `_rosterSorters`, `_rosterOptions`, `_assignmentOptions` lists
- Localization key `officer_filter_below_deck_ability` — still in string table

## Key Findings (from disassembly & runtime testing)

### 1. Hook targets: OfficerSortGenerators, not OfficerManager

The original plan assumed we'd hook `OfficerManager.GetRosterSortOptions()` /
`GetAssignmentSortOptions()`. In reality, the sort options are built by
`OfficerSortGenerators.InitializeOfficerSorters()` (roster) and
`InitializeAssignmentSorters()` (assignment), which populate `_rosterOptions`
and `_assignmentOptions` lists. We hook these void instance methods and append
our option after the game fills its lists.

The roster screen also needs the raw `SortFunction` added to `_rosterSorters`
(separate from `_rosterOptions`), because the roster builds its `SortComparer`
from `_rosterSorters`.

### 2. SortComparer.Compare always calls _ascending, negates for descending

**This is the most critical finding.** `SortComparer.Compare` (RVA `0x635690`)
always calls the `_ascending` delegate. For Descending sort direction, it
negates the `_ascending` result (`neg eax`). The `_descending` delegate is
**never called**.

This means all sort logic must live in the `*Ascending` wrappers, designed so
that:
- Direct call = ascending behavior
- Negated call = descending behavior

### 3. Default sort directions differ per screen

- **Roster screen**: defaults to **Ascending** (SortDirection=1)
- **Assignment screen**: defaults to **Descending** (SortDirection=0)

This is why the two screens need different wrapper assignments:
- Roster `_ascending` → `RosterAscendingWrapper` (BDA-first for default Ascending)
- Assignment `_ascending` → `AssignmentAscendingWrapper` (non-BDA-first, negated
  by SortComparer for default Descending = BDA-first)

### 4. Three-level sort order

The game's `GenerateSortFunction` prepends `OfficerSortByUnlockedStateAscending`
to every sort predicate array. We replicate this in C++ with wrapper functions
that chain three levels:

1. **Unlocked state** (`OfficerSortByUnlockedStateAscending`) — available
   officers first, locked at bottom
2. **BDA status** (`OfficerSortByBelowDeckAbilityDescending` for BDA-first, or
   `Ascending` for non-BDA-first) — BDA officers first or non-BDA first
3. **Officer index** (`OfficerSortByIndexAscending`) — newer officers first
   (tiebreaker within the same BDA tier)

### 5. Index tiebreaker asymmetry

Because `SortComparer.Compare` negates the `_ascending` result for Descending:
- **Assignment (Descending)**: calls `_ascending` → `indexAsc`, then negates →
  `indexDesc` = newer officers first ✓
- **Roster (Ascending)**: calls `_ascending` → `indexAsc` directly = older
  officers first ✗

Fix: Negate the index sort in `RosterAscendingWrapper` (`-indexAscPtr(...)`)
so the roster also gets newer officers first. `AssignmentAscendingWrapper`
stays as-is since the SortComparer's negation already produces the correct
result.

### 6. Delegate creation via .ctor invoker_method

`il2cpp_runtime_invoke` fails for delegate `.ctor` on static methods
("Delegate to an instance method cannot have null 'this'"). The workaround is
to call the `.ctor` via its `invoker_method` directly:

```cpp
auto* ctor = il2cpp_class_get_method_from_name(funcClass, ".ctor", 2);
void* ctorArgs[2] = {nullptr, (void*)&sortMethod};
ctor->invoker_method(ctor->methodPointer, ctor, del, ctorArgs, nullptr);
```

After construction, we replace the delegate's `method_ptr` with our C++ wrapper
functions that chain the three sort levels.

### 7. Discovering Func<object,object,int> at runtime

The `Func<object,object,int>` generic delegate type is inflated at runtime.
Rather than constructing it from type metadata, we discover it by reading an
existing delegate from the game's first sort option:
1. Get the first `ProcessingOption` from the list
2. Read its `_sortingOption` (`SortFunction`)
3. Read the `SortFunction._ascending` field (a `Func<object,object,int>` delegate)
4. Call `il2cpp_object_get_class` on that delegate to get the class

### 8. ProcessingOption is a nested class

`SortComparer.ProcessingOption` is a nested class. The `IL2CppClassHelper`'s
`GetNestedType()` reads the `nestedTypes` array directly and crashes on this
class. The safe approach is to use the `il2cpp_class_get_nested_types` iterator
API instead.

### 9. ProcessingOption fields set directly, not via .ctor

The `ProcessingOption` `.ctor` takes 5 arguments (string, FilterFunction,
SortFunction, string, bool). Rather than calling it via `il2cpp_runtime_invoke`
(which requires constructing all the argument types), we allocate the object
with `il2cpp_object_new` and set the two fields we care about directly:
`_displayKey` and `_sortingOption`. The other fields (`_iconKey`,
`_filteringOption`, `UseAuxiliaryOption`) are left as null/false, which matches
how the game's own BDA option was configured.

### 10. Sort objects received by predicates are InventoryItem, not Officer

The sort delegates receive `InventoryItem` objects, not `Officer` objects
directly. `InventoryItem` has an `_officer` field (offset 0x80) and a `type_`
field (offset 0xF0) which is 9 for `InventoryOfficer`. The native sort methods
handle this internally — we just pass the objects through to them.

## Implementation

### Files modified

| File | Change |
|---|---|
| `mods/src/config.h` | Add `bool installOfficerSortHooks` |
| `mods/src/defaultconfig.h` | Add default `installOfficerSortHooks = true` |
| `mods/src/config.cc` | Load config in both `_MODDBG` and non-`_MODDBG` paths |
| `mods/src/patches/parts/officer_sort.cc` | New patch file (main implementation) |
| `mods/src/patches/patches.cc` | Register the patch |
| `example_community_patch_settings.toml` | Document the setting |

### Sort wrapper logic

```
RosterAscendingWrapper (called directly for Ascending):
  1. unlockedStatePtr(obj1, obj2)  → available first
  2. bdaDescPtr(obj1, obj2)        → BDA-first
  3. -indexAscPtr(obj1, obj2)      → newer-officers-first

RosterDescendingWrapper (never called by SortComparer, kept for safety):
  1. unlockedStatePtr(obj1, obj2)  → available first
  2. bdaAscPtr(obj1, obj2)         → non-BDA-first
  3. -indexAscPtr(obj1, obj2)      → newer-officers-first

AssignmentAscendingWrapper (negated by SortComparer for Descending):
  1. unlockedStatePtr(obj1, obj2)  → available first
  2. bdaAscPtr(obj1, obj2)         → non-BDA-first (negated = BDA-first)
  3. indexAscPtr(obj1, obj2)       → older-officers-first (negated = newer-first)

AssignmentDescendingWrapper (never called by SortComparer, kept for safety):
  1. unlockedStatePtr(obj1, obj2)  → available first
  2. bdaDescPtr(obj1, obj2)        → BDA-first
  3. indexAscPtr(obj1, obj2)       → older-officers-first
```

### Display keys

- Roster: `"below_deck_ability"` (matches m93-release config_key)
- Assignment: `"_below_deck_ability"` (assignment screen uses `_` prefix)

## Reference: Key IL2CPP Classes & Methods (m93.1)

### SortingPredicates (Assembly-CSharp, Digit.Client.Core)
- `OfficerSortByBelowDeckAbilityAscending(object, object)` — RVA 0x8047F0
- `OfficerSortByBelowDeckAbilityDescending(object, object)` — RVA 0x804A20
- `OfficerSortByUnlockedStateAscending(object, object)` — keeps available at top
- `OfficerSortByIndexAscending(object, object)` — tiebreaker by officer index

### SortFunction (Digit.Client.PrimeLib.Runtime, Digit.Client.Core)
- Public fields: `_displayKey` (string), `_ascending` (Func), `_descending` (Func)
- `SortComparer.Compare` always calls `_ascending`; negates for Descending

### SortComparer (Assembly-CSharp, Digit.Client.Sorting)
- `Compare(object, object)` — RVA 0x635690 — calls `_ascending`, negates for descending
- Nested class `ProcessingOption` — fields `_displayKey`, `_sortingOption`

### OfficerSortGenerators (Assembly-CSharp, Digit.Prime.Officers)
- `InitializeOfficerSorters()` — populates `_rosterSorters` and `_rosterOptions`
- `InitializeAssignmentSorters()` — populates `_assignmentOptions`

### SortDirection enum
- `Descending = 0`
- `Ascending = 1`
