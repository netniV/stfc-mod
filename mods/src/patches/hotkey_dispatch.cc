#include "patches/hotkey_dispatch.h"
#include "patches/hotkey_router.h"

#include "config.h"

#include "prime/BookmarksManager.h"
#include "prime/FleetBarViewController.h"
#include "prime/Hub.h"

#include <spdlog/spdlog.h>

static DispatchDecision HandleShowQTrials()
{
  hotkey_router_goto_section(SectionID::ChallengeSelection);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowBookmarks()
{
  auto bookmark_manager = BookmarksManager::Instance();
  if (bookmark_manager) {
    bookmark_manager->ViewBookmarks();
  } else {
    hotkey_router_goto_section(SectionID::Bookmarks_Main);
  }
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowLookup()
{
  hotkey_router_goto_section(SectionID::Bookmarks_Search_Coordinates);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowRefinery()
{
  hotkey_router_goto_section(SectionID::Shop_Refining_List);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowFactions()
{
  hotkey_router_goto_section(SectionID::Shop_MainFactions);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowStationExterior()
{
  hotkey_router_goto_section(SectionID::Starbase_Exterior);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowGalaxy()
{
  hotkey_router_change_navigation_section(SectionID::Navigation_Galaxy);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowStationInterior()
{
  hotkey_router_goto_section(SectionID::Starbase_Interior);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowSystem()
{
  hotkey_router_change_navigation_section(SectionID::Navigation_System);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowArtifacts()
{
  hotkey_router_goto_section(SectionID::ArtifactHall_Inventory);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowInventory()
{
  hotkey_router_goto_section(SectionID::InventoryList);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowMissions()
{
  hotkey_router_goto_section(SectionID::Missions_AcceptedList);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowResearch()
{
  hotkey_router_goto_section(SectionID::Research_LandingPage);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowScrapYard()
{
  hotkey_router_goto_section(SectionID::ShipScrapping_List);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowOfficers()
{
  hotkey_router_goto_section(SectionID::OfficerInventory);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowCommander()
{
  hotkey_router_goto_section(SectionID::FleetCommander_Management);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowAwayTeam()
{
  hotkey_router_goto_section(SectionID::Missions_AwayTeamsList);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowEvents()
{
  hotkey_router_goto_section(SectionID::Tournament_Group_Selection);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowExoComp()
{
  hotkey_router_goto_section(SectionID::Consumables);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowDaily()
{
  hotkey_router_goto_section(SectionID::Missions_DailyGoals);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowGifts()
{
  hotkey_router_goto_section(SectionID::Shop_List);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowAlliance()
{
  hotkey_router_goto_section(SectionID::Alliance_Main);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowAllianceHelp()
{
  hotkey_router_goto_section(SectionID::Alliance_Help);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowAllianceArmada()
{
  hotkey_router_goto_section(SectionID::Alliance_Armadas);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowSettings()
{
  hotkey_router_goto_section(SectionID::GameSettings);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleUiScaleUp()
{
  Config::Get().AdjustUiScale(true);
  return DispatchDecision::HandledAllowOriginal;
}

static DispatchDecision HandleUiScaleDown()
{
  Config::Get().AdjustUiScale(false);
  return DispatchDecision::HandledAllowOriginal;
}

static DispatchDecision HandleUiViewerScaleUp()
{
  Config::Get().AdjustUiViewerScale(true);
  return DispatchDecision::HandledAllowOriginal;
}

static DispatchDecision HandleUiViewerScaleDown()
{
  Config::Get().AdjustUiViewerScale(false);
  return DispatchDecision::HandledAllowOriginal;
}

static DispatchDecision HandleTogglePreviewLocate()
{
  Config::Get().disable_preview_locate = !Config::Get().disable_preview_locate;
  return DispatchDecision::HandledAllowOriginal;
}

static DispatchDecision HandleTogglePreviewRecall()
{
  Config::Get().disable_preview_recall = !Config::Get().disable_preview_recall;
  return DispatchDecision::HandledAllowOriginal;
}

static DispatchDecision HandleToggleCargoDefault()
{
  Config::Get().show_cargo_default = !Config::Get().show_cargo_default;
  return DispatchDecision::HandledAllowOriginal;
}

static DispatchDecision HandleToggleCargoPlayer()
{
  Config::Get().show_player_cargo = !Config::Get().show_player_cargo;
  return DispatchDecision::HandledAllowOriginal;
}

static DispatchDecision HandleToggleCargoStation()
{
  Config::Get().show_station_cargo = !Config::Get().show_station_cargo;
  return DispatchDecision::HandledAllowOriginal;
}

static DispatchDecision HandleToggleCargoHostile()
{
  Config::Get().show_hostile_cargo = !Config::Get().show_hostile_cargo;
  return DispatchDecision::HandledAllowOriginal;
}

static DispatchDecision HandleToggleCargoArmada()
{
  Config::Get().show_armada_cargo = !Config::Get().show_armada_cargo;
  return DispatchDecision::HandledAllowOriginal;
}

static DispatchDecision HandleLogLevelOff()
{
  spdlog::set_level(spdlog::level::off);
  spdlog::flush_on(spdlog::level::off);
  return DispatchDecision::HandledAllowOriginal;
}

static DispatchDecision HandleLogLevelError()
{
  spdlog::set_level(spdlog::level::err);
  spdlog::flush_on(spdlog::level::err);
  return DispatchDecision::HandledAllowOriginal;
}

static DispatchDecision HandleLogLevelWarn()
{
  spdlog::set_level(spdlog::level::warn);
  spdlog::flush_on(spdlog::level::warn);
  return DispatchDecision::HandledAllowOriginal;
}

static DispatchDecision HandleLogLevelInfo()
{
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);
  return DispatchDecision::HandledAllowOriginal;
}

static DispatchDecision HandleLogLevelDebug()
{
  spdlog::set_level(spdlog::level::debug);
  spdlog::flush_on(spdlog::level::debug);
  return DispatchDecision::HandledAllowOriginal;
}

static DispatchDecision HandleLogLevelTrace()
{
  spdlog::set_level(spdlog::level::trace);
  spdlog::flush_on(spdlog::level::trace);
  return DispatchDecision::HandledAllowOriginal;
}

static DispatchDecision HandleShowShips()
{
  auto fleet_bar = ObjectFinder<FleetBarViewController>::Get();
  if (fleet_bar) {
    auto fleet_controller = fleet_bar->_fleetPanelController;
    auto fleet            = fleet_bar->_fleetPanelController->fleet;
    if (fleet) {
      fleet_controller->RequestAction(fleet, ActionType::Manage, 0, ActionBehaviour::Default);
    }
  }
  return DispatchDecision::HandledAllowOriginal;
}

static constexpr HotkeyEntry g_dispatch_table[] = {
    {GameFunction::ShowQTrials, HandleShowQTrials},
    {GameFunction::ShowBookmarks, HandleShowBookmarks},
    {GameFunction::ShowLookup, HandleShowLookup},
    {GameFunction::ShowRefinery, HandleShowRefinery},
    {GameFunction::ShowFactions, HandleShowFactions},
    {GameFunction::ShoWStationExterior, HandleShowStationExterior},
    {GameFunction::ShowGalaxy, HandleShowGalaxy},
    {GameFunction::ShowStationInterior, HandleShowStationInterior},
    {GameFunction::ShowSystem, HandleShowSystem},
    {GameFunction::ShowArtifacts, HandleShowArtifacts},
    {GameFunction::ShowInventory, HandleShowInventory},
    {GameFunction::ShowMissions, HandleShowMissions},
    {GameFunction::ShowResearch, HandleShowResearch},
    {GameFunction::ShowScrapYard, HandleShowScrapYard},
    {GameFunction::ShowOfficers, HandleShowOfficers},
    {GameFunction::ShowCommander, HandleShowCommander},
    {GameFunction::ShowAwayTeam, HandleShowAwayTeam},
    {GameFunction::ShowEvents, HandleShowEvents},
    {GameFunction::ShowExoComp, HandleShowExoComp},
    {GameFunction::ShowDaily, HandleShowDaily},
    {GameFunction::ShowGifts, HandleShowGifts},
    {GameFunction::ShowAlliance, HandleShowAlliance},
    {GameFunction::ShowAllianceHelp, HandleShowAllianceHelp},
    {GameFunction::ShowAllianceArmada, HandleShowAllianceArmada},
    {GameFunction::ShowSettings, HandleShowSettings},
    {GameFunction::UiScaleUp, HandleUiScaleUp, InputMode::Pressed},
    {GameFunction::UiScaleDown, HandleUiScaleDown, InputMode::Pressed},
    {GameFunction::UiViewerScaleUp, HandleUiViewerScaleUp, InputMode::Pressed},
    {GameFunction::UiViewerScaleDown, HandleUiViewerScaleDown, InputMode::Pressed},
    {GameFunction::TogglePreviewLocate, HandleTogglePreviewLocate},
    {GameFunction::TogglePreviewRecall, HandleTogglePreviewRecall},
    {GameFunction::ToggleCargoDefault, HandleToggleCargoDefault},
    {GameFunction::ToggleCargoPlayer, HandleToggleCargoPlayer},
    {GameFunction::ToggleCargoStation, HandleToggleCargoStation},
    {GameFunction::ToggleCargoHostile, HandleToggleCargoHostile},
    {GameFunction::ToggleCargoArmada, HandleToggleCargoArmada},
    {GameFunction::LogLevelOff, HandleLogLevelOff},
    {GameFunction::LogLevelError, HandleLogLevelError},
    {GameFunction::LogLevelWarn, HandleLogLevelWarn},
    {GameFunction::LogLevelInfo, HandleLogLevelInfo},
    {GameFunction::LogLevelDebug, HandleLogLevelDebug},
    {GameFunction::LogLevelTrace, HandleLogLevelTrace},
    {GameFunction::ShowShips, HandleShowShips},
};

std::span<const HotkeyEntry> GetHotkeyDispatchTable()
{
  return g_dispatch_table;
}