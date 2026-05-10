/**
 * @file fleet_notifications.h
 * @brief Fleet notification runtime logic independent from hook installation.
 *
 * The hook layer captures live fleet-bar and mining-viewer events, then hands
 * those observations to this module. This file contains the notification state
 * machine and message formatting, while the `parts/` layer stays limited to
 * IL2CPP method discovery and hook injection.
 */
#pragma once

#include <cstdint>

struct FleetPlayerData;

void fleet_notifications_init();
void fleet_notifications_observe_fleet_bar(FleetPlayerData* fleet);
void fleet_notifications_observe_node_depleted(int64_t fleetId);
void fleet_notifications_observe_mining_timer(FleetPlayerData* selectedFleet, int64_t remainingTicks);