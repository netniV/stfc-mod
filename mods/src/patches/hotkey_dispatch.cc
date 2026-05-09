#include "patches/hotkey_dispatch.h"

#include "config.h"

#include "prime/BookmarksManager.h"
#include "prime/FleetBarViewController.h"
#include "prime/Hub.h"

#include <spdlog/spdlog.h>

void GotoSection(SectionID sectionID, void* screen_data = nullptr);
void ChangeNavigationSection(SectionID sectionID);

static DispatchDecision HandleShowQTrials()
{
  GotoSection(SectionID::ChallengeSelection);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowBookmarks()
{
  auto bookmark_manager = BookmarksManager::Instance();
  if (bookmark_manager) {
    bookmark_manager->ViewBookmarks();
  } else {
    GotoSection(SectionID::Bookmarks_Main);
  }
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowLookup()
{
  GotoSection(SectionID::Bookmarks_Search_Coordinates);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowRefinery()
{
  GotoSection(SectionID::Shop_Refining_List);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowFactions()
{
  GotoSection(SectionID::Shop_MainFactions);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowStationExterior()
{
  GotoSection(SectionID::Starbase_Exterior);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowGalaxy()
{
  ChangeNavigationSection(SectionID::Navigation_Galaxy);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowStationInterior()
{
  GotoSection(SectionID::Starbase_Interior);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowSystem()
{
  ChangeNavigationSection(SectionID::Navigation_System);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowArtifacts()
{
  GotoSection(SectionID::ArtifactHall_Inventory);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowInventory()
{
  GotoSection(SectionID::InventoryList);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowMissions()
{
  GotoSection(SectionID::Missions_AcceptedList);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowResearch()
{
  GotoSection(SectionID::Research_LandingPage);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowScrapYard()
{
  GotoSection(SectionID::ShipScrapping_List);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowOfficers()
{
  GotoSection(SectionID::OfficerInventory);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowCommander()
{
  GotoSection(SectionID::FleetCommander_Management);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowAwayTeam()
{
  GotoSection(SectionID::Missions_AwayTeamsList);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowEvents()
{
  GotoSection(SectionID::Tournament_Group_Selection);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowExoComp()
{
  GotoSection(SectionID::Consumables);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowDaily()
{
  GotoSection(SectionID::Missions_DailyGoals);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowGifts()
{
  GotoSection(SectionID::Shop_List);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowAlliance()
{
  GotoSection(SectionID::Alliance_Main);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowAllianceHelp()
{
  GotoSection(SectionID::Alliance_Help);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowAllianceArmada()
{
  GotoSection(SectionID::Alliance_Armadas);
  return DispatchDecision::HandledStop;
}

static DispatchDecision HandleShowSettings()
{
  GotoSection(SectionID::GameSettings);
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