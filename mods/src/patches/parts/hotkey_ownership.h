#pragma once

#include "patches/gamefunctions.h"
#include "patches/mapkey.h"

#include <array>

// Screen navigation and camera shortcuts run from separate Unity update hooks. Give screen navigation ownership of a
// shared chord so the result does not depend on which hook Unity calls first.
inline bool IsScreenNavigationShortcutPressed()
{
  static constexpr std::array screen_navigation_actions{
      GameFunction::ShowAlliance,        GameFunction::ShowAllianceArmada,  GameFunction::ShowAllianceHelp,
      GameFunction::ShowArtifacts,       GameFunction::ShowAwayTeam,        GameFunction::ShowBookmarks,
      GameFunction::ShowCommander,       GameFunction::ShowDaily,           GameFunction::ShowEvents,
      GameFunction::ShowExoComp,         GameFunction::ShowFactions,        GameFunction::ShowGalaxy,
      GameFunction::ShowGalaxyNative,    GameFunction::ShowGifts,           GameFunction::ShowInventory,
      GameFunction::ShowLookup,          GameFunction::ShowMissions,        GameFunction::ShowOfficers,
      GameFunction::ShowQTrials,         GameFunction::ShowRefinery,        GameFunction::ShowResearch,
      GameFunction::ShowScrapYard,       GameFunction::ShowSettings,        GameFunction::ShowShips,
      GameFunction::ShowStationInterior, GameFunction::ShoWStationExterior, GameFunction::ShowSystem,
  };

  for (const auto action : screen_navigation_actions) {
    if (MapKey::IsPressed(action)) {
      return true;
    }
  }
  return false;
}
