#pragma once

#include <cstdint>

struct FleetPlayerData;

void fleet_watch_init();
void fleet_watch_tick();
void fleet_watch_observe_widget(FleetPlayerData* fleet);
void fleet_watch_observe_node_depleted(int64_t fleet_id);

bool fleet_watch_uses_state_observation();
bool fleet_watch_uses_node_depleted_hook();
