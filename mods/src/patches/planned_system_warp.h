#pragma once

struct FleetPlayerData;
struct NavigationInteractionUIContext;

// Resume the selected fleet's unused ordinary system course after its popup has closed.
// The native movement handler retains ownership of confirmations and network callbacks.
bool TryRequestPlannedSystemWarp(FleetPlayerData* fleet, NavigationInteractionUIContext* context);
