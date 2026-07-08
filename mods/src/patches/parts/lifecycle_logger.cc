#include "config.h"
#include "errormsg.h"

#include <il2cpp/il2cpp_helper.h>

#include <spdlog/spdlog.h>
#include <spud/detour.h>

#if _WIN32
#include <Windows.h>
#else
#include <chrono>
#endif

// =============================================================================
// LIFECYCLE LOGGER — Diagnostic patch that logs every step of the game's
// launch and reload lifecycle. See docs/LAUNCH_AND_RELOAD_LIFECYCLE.md for
// the full lifecycle analysis.
//
// All hooks are pass-through (call original, log, return). No behavior changes.
// =============================================================================

static uint64_t GetTickMs()
{
#if _WIN32
  return GetTickCount64();
#else
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
#endif
}

static const char* StateName(int32_t state)
{
  switch (state) {
    case 0: return "Showing";
    case 1: return "Shown";
    case 2: return "Hidden";
    default: return "Unknown";
  }
}

static const char* CheckFeedbackName(int32_t result)
{
  switch (result) {
    case 0: return "Ignore";
    case 1: return "NotOk";
    case 2: return "Ok";
    case 3: return "Dropped";
    default: return "Unknown";
  }
}

static const char* LoginStageName(int32_t stage)
{
  switch (stage) {
    case 0: return "LoadingEndpoints";
    case 1: return "Localization";
    case 2: return "Environment";
    case 3: return "LoadingAssets";
    case 4: return "LoginDetails";
    case 5: return "LoadingGame";
    case 6: return "Login2_0";
    case 7: return "CheckMaintenance";
    case 8: return "CloudStorageCheck";
    default: return "Unknown";
  }
}

static const char* TransitionTypeName(int32_t type)
{
  switch (type) {
    case -1: return "None";
    case 0: return "SceneLoad";
    case 1: return "DirectorNotDownloaded";
    case 2: return "DirectorNotInstantiated";
    case 3: return "DirectorNotActive";
    default: return "Unknown";
  }
}

static const char* SectionEnterPhaseName(int32_t phase)
{
  switch (phase) {
    case 0: return "PrepareSectionActivation";
    case 1: return "CheckIfAllReadyForActivation";
    case 2: return "ActivateSection";
    case 3: return "SectionActivated";
    default: return "Unknown";
  }
}

static const char* SectionLeavePhaseName(int32_t phase)
{
  switch (phase) {
    case 0: return "CheckingIfOkToLeave";
    case 1: return "WaitUntilReadyToDeactivate";
    case 2: return "Deactivate";
    case 3: return "CheckIfDeactivationIsCompleted";
    case 4: return "SectionWasDeactivated";
    default: return "Unknown";
  }
}

// ---------------------------------------------------------------------------
// Helper: log Hub state (SectionManager, UI, App pointers)
// ---------------------------------------------------------------------------
static void LogHubState(const char* context)
{
  auto hub_h = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "Hub");
  if (!hub_h.isValidHelper()) {
    spdlog::warn("[LC] {}: Hub class helper invalid", context);
    return;
  }
  void* hubSM  = nullptr;
  void* hubUI  = nullptr;
  void* hubApp = nullptr;
  try {
    auto smField = hub_h.GetStaticField("<SectionManager>k__BackingField");
    hubSM        = smField.Get<void*>();
  } catch (...) {}
  try {
    auto uiField = hub_h.GetStaticField("<UI>k__BackingField");
    hubUI        = uiField.Get<void*>();
  } catch (...) {}
  try {
    auto appField = hub_h.GetStaticField("<App>k__BackingField");
    hubApp        = appField.Get<void*>();
  } catch (...) {}
  spdlog::info("[LC] {}: Hub.SectionManager={:x} Hub.UI={:x} Hub.App={:x}",
               context, (uintptr_t)hubSM, (uintptr_t)hubUI, (uintptr_t)hubApp);
}

// ---------------------------------------------------------------------------
// Helper: log TransitionManager state (currentState, canvasController, blurController)
// ---------------------------------------------------------------------------
static void LogTMState(const char* context, void* tm)
{
  if (!tm) {
    spdlog::info("[LC] {}: TransitionManager is null", context);
    return;
  }
  int32_t state  = *reinterpret_cast<int32_t*>((char*)tm + 0x58);
  void*   cc     = *reinterpret_cast<void**>((char*)tm + 0x50);
  void*   blur   = *reinterpret_cast<void**>((char*)tm + 0x60);
  void*   ccNative = cc ? *reinterpret_cast<void**>((char*)cc + 0x10) : nullptr;
  int32_t ccState = cc ? *reinterpret_cast<int32_t*>((char*)cc + 0x78) : -1;

  spdlog::info("[LC] {}: TM={:x} state={} cc={:x} ccNative={:x} ccState={} blur={:x}",
               context, (uintptr_t)tm, StateName(state),
               (uintptr_t)cc, (uintptr_t)ccNative, ccState, (uintptr_t)blur);

  if (blur) {
    int32_t blurTarget   = *reinterpret_cast<int32_t*>((char*)blur + 0x18);
    float   tweenVal     = *reinterpret_cast<float*>((char*)blur + 0x20);
    float   tweenTgt     = *reinterpret_cast<float*>((char*)blur + 0x24);
    float   blurTime     = *reinterpret_cast<float*>((char*)blur + 0x40);
    spdlog::info("[LC] {}:   BlurController: blurTarget={} tweenVal={:.4f} tweenTgt={:.4f} blurTime={:.4f}",
                 context, blurTarget, tweenVal, tweenTgt, blurTime);
  }
}

// ---------------------------------------------------------------------------
// Helper: log StreamingLoadingScreenManager state
// ---------------------------------------------------------------------------
static void LogStreamingState(const char* context)
{
  auto slsm_h = il2cpp_get_class_helper("Digit.Engine.AssetBundle.Runtime", "Digit.Client.SceneManagement",
                                       "StreamingLoadingScreenManager");
  if (!slsm_h.isValidHelper()) {
    return;
  }
  auto cls = slsm_h.get_cls();
  if (!cls || !cls->static_fields) {
    return;
  }
  char* sf = (char*)cls->static_fields;
  int32_t predAdded      = *reinterpret_cast<int32_t*>(sf + 0x30);
  int32_t predCompleted  = *reinterpret_cast<int32_t*>(sf + 0x34);
  int32_t reqAdded       = *reinterpret_cast<int32_t*>(sf + 0x38);
  int32_t reqCompleted   = *reinterpret_cast<int32_t*>(sf + 0x3C);
  spdlog::info("[LC] {}: Streaming: predAdded={} predDone={} reqAdded={} reqDone={}",
               context, predAdded, predCompleted, reqAdded, reqCompleted);
}

// ---------------------------------------------------------------------------
// Helper: log SectionStatus (current/previous/next section IDs)
// ---------------------------------------------------------------------------
static void LogSectionStatus(const char* context, void* status)
{
  if (!status) {
    spdlog::info("[LC] {}: SectionStatus is null", context);
    return;
  }
  int32_t prevSection = *reinterpret_cast<int32_t*>((char*)status + 0x10);
  int32_t currSection = *reinterpret_cast<int32_t*>((char*)status + 0x14);
  int32_t nextSection = *reinterpret_cast<int32_t*>((char*)status + 0x18);
  bool isGoBack       = *reinterpret_cast<bool*>((char*)status + 0x28);
  spdlog::info("[LC] {}: SectionStatus: prev={} curr={} next={} goBack={}",
               context, prevSection, currSection, nextSection, isGoBack);
}

// ---------------------------------------------------------------------------
// Helper: log PrimeApp.IsReloading flag
// ---------------------------------------------------------------------------
static void LogIsReloading(const char* context, void* app)
{
  if (!app) {
    return;
  }
  // IsReloading backing field is at offset 0x50 in PrimeApp
  bool isReloading = *reinterpret_cast<bool*>((char*)app + 0x50);
  int32_t sessionCount = *reinterpret_cast<int32_t*>((char*)app + 0x88);
  spdlog::info("[LC] {}: IsReloading={} SessionCount={}", context, isReloading, sessionCount);
}

// ===========================================================================
// LAUNCH LIFECYCLE HOOKS
// ===========================================================================

// PrimeApp — the main application controller
// Hook: PrimeApp.InitPlatformServer — connects to Scopely platform servers
static void PrimeApp_InitPlatformServer_Hook(auto original, void* _this)
{
  spdlog::info("[LC] === LAUNCH STEP: PrimeApp.InitPlatformServer ===");
  LogHubState("InitPlatformServer");
  LogIsReloading("InitPlatformServer pre", _this);
  original(_this);
  spdlog::info("[LC] === LAUNCH STEP: PrimeApp.InitPlatformServer DONE ===");
  LogIsReloading("InitPlatformServer post", _this);
}

// PrimeApp.ReloadWithReason — called before Reload, provides the reason
static void PrimeApp_ReloadWithReason_Hook(auto original, void* _this, void* reason)
{
  spdlog::warn("[LC] =============== RELOAD TRIGGER: PrimeApp.ReloadWithReason ===============");
  LogIsReloading("ReloadWithReason pre", _this);
  original(_this, reason);
  spdlog::warn("[LC] =============== RELOAD TRIGGER: PrimeApp.ReloadWithReason DONE ===============");
}

// PrimeApp.ReloadNetworkServices — re-initializes network connections during DoReload
static void PrimeApp_ReloadNetworkServices_Hook(auto original, void* _this, bool quickReloading)
{
  spdlog::warn("[LC] === RELOAD STEP: PrimeApp.ReloadNetworkServices (quick={}) ===", quickReloading);
  original(_this, quickReloading);
  spdlog::warn("[LC] === RELOAD STEP: PrimeApp.ReloadNetworkServices DONE ===");
}

// PrimeApp.OnEnterSectionReload — callback registered during GoToLoginSection
static int32_t PrimeApp_OnEnterSectionReload_Hook(auto original, void* _this,
                                                   void* status, int32_t phase, void* storage)
{
  spdlog::warn("[LC] === RELOAD STEP: PrimeApp.OnEnterSectionReload phase={} ({}) ===",
               phase, SectionEnterPhaseName(phase));
  LogSectionStatus("OnEnterSectionReload", status);
  int32_t result = original(_this, status, phase, storage);
  spdlog::warn("[LC] === RELOAD STEP: PrimeApp.OnEnterSectionReload DONE result={} ({}) ===",
               result, CheckFeedbackName(result));
  return result;
}

// PrimeApp.FullSync — full sync after login
static void PrimeApp_FullSync_Hook(auto original, void* _this, void* callbacks)
{
  spdlog::info("[LC] === LAUNCH STEP: PrimeApp.FullSync ===");
  LogIsReloading("FullSync pre", _this);
  original(_this, callbacks);
  spdlog::info("[LC] === LAUNCH STEP: PrimeApp.FullSync DONE ===");
  LogIsReloading("FullSync post", _this);
}

// Hub.Init — initializes all static Hub properties
// This is a static method, no this pointer
static void Hub_Init_Hook(auto original)
{
  spdlog::info("[LC] === LAUNCH STEP: Hub.Init ===");
  original();
  spdlog::info("[LC] === LAUNCH STEP: Hub.Init DONE ===");
  LogHubState("Hub.Init post");
}

// TransitionManager.Awake — creates BlurController, registers OnLeaveSection
static void TransitionManager_Awake_Hook(auto original, void* _this)
{
  spdlog::info("[LC] === LAUNCH STEP: TransitionManager.Awake (this={:x}) ===", (uintptr_t)_this);
  original(_this);
  spdlog::info("[LC] === LAUNCH STEP: TransitionManager.Awake DONE ===");
  LogTMState("TM.Awake post", _this);
}

// TransitionManager.OnEnable — registers OnEnterSection + OnLeaveSection
static void TransitionManager_OnEnable_Hook(auto original, void* _this)
{
  spdlog::info("[LC] === LAUNCH STEP: TransitionManager.OnEnable (this={:x}) ===", (uintptr_t)_this);
  original(_this);
  spdlog::info("[LC] === LAUNCH STEP: TransitionManager.OnEnable DONE ===");
}

// TransitionManager.OnDisable — unregisters OnEnterSection + OnLeaveSection
static void TransitionManager_OnDisable_Hook(auto original, void* _this)
{
  spdlog::info("[LC] === LAUNCH STEP: TransitionManager.OnDisable (this={:x}) ===", (uintptr_t)_this);
  original(_this);
  spdlog::info("[LC] === LAUNCH STEP: TransitionManager.OnDisable DONE ===");
}

// TransitionManager.OnDestroy
static void TransitionManager_OnDestroy_Hook(auto original, void* _this)
{
  spdlog::info("[LC] === LAUNCH STEP: TransitionManager.OnDestroy (this={:x}) ===", (uintptr_t)_this);
  original(_this);
  spdlog::info("[LC] === LAUNCH STEP: TransitionManager.OnDestroy DONE ===");
}

// LoginSequence.Awake — sets _instance, sets CurrentLoginStage, starts behavior tree
static void LoginSequence_Awake_Hook(auto original, void* _this)
{
  spdlog::info("[LC] === LAUNCH STEP: LoginSequence.Awake (this={:x}) ===", (uintptr_t)_this);
  original(_this);
  spdlog::info("[LC] === LAUNCH STEP: LoginSequence.Awake DONE ===");
  // Read CurrentLoginStage from offset 0xB0
  if (_this) {
    int32_t stage = *reinterpret_cast<int32_t*>((char*)_this + 0xB0);
    spdlog::info("[LC] LoginSequence.Awake: CurrentLoginStage={} ({})", stage, LoginStageName(stage));
  }
}

// LoginSequence.UpdateLoginStage — processes current stage
static void LoginSequence_UpdateLoginStage_Hook(auto original, void* _this)
{
  if (_this) {
    int32_t stage = *reinterpret_cast<int32_t*>((char*)_this + 0xB0);
    spdlog::info("[LC] LoginSequence.UpdateLoginStage: stage={} ({})", stage, LoginStageName(stage));
  }
  original(_this);
  if (_this) {
    int32_t stage = *reinterpret_cast<int32_t*>((char*)_this + 0xB0);
    spdlog::info("[LC] LoginSequence.UpdateLoginStage: post stage={} ({})", stage, LoginStageName(stage));
  }
}

// LoginSequence.set_CurrentLoginStage — fires OnLoginStateChanged event
static void LoginSequence_SetCurrentLoginStage_Hook(auto original, void* _this, int32_t value)
{
  spdlog::info("[LC] LoginSequence.set_CurrentLoginStage: {} -> {} ({})",
               _this ? LoginStageName(*reinterpret_cast<int32_t*>((char*)_this + 0xB0)) : "?",
               value, LoginStageName(value));
  original(_this, value);
}

// SectionManager.TriggerSectionChange — starts a section change
static void SectionManager_TriggerSectionChange_Hook(auto original, void* _this,
                                                     int32_t nextSectionID, void* args,
                                                     bool forcedSectionChange, bool isGoBackStep,
                                                     bool allowSameSection)
{
  spdlog::info("[LC] === SECTION CHANGE: TriggerSectionChange sectionID={} forced={} goBack={} sameAllowed={} ===",
               nextSectionID, forcedSectionChange, isGoBackStep, allowSameSection);
  original(_this, nextSectionID, args, forcedSectionChange, isGoBackStep, allowSameSection);
  spdlog::info("[LC] === SECTION CHANGE: TriggerSectionChange DONE ===");
}

// TransitionManager.SetLoadingScreen — starts a transition
static void TransitionManager_SetLoadingScreen_Hook(auto original, void* _this,
                                                    void* status, int32_t type, int32_t messagingType)
{
  spdlog::info("[LC] === TRANSITION: SetLoadingScreen (this={:x}, type={} ({}), messaging={}) ===",
               (uintptr_t)_this, type, TransitionTypeName(type), messagingType);
  LogTMState("SetLoadingScreen pre", _this);
  LogSectionStatus("SetLoadingScreen", status);
  original(_this, status, type, messagingType);
  spdlog::info("[LC] === TRANSITION: SetLoadingScreen DONE ===");
  LogTMState("SetLoadingScreen post", _this);
}

// TransitionManager.UpdateMessagingType — updates messaging type during transition
static void TransitionManager_UpdateMessagingType_Hook(auto original, void* _this, int32_t messagingType)
{
  spdlog::info("[LC] === TRANSITION: UpdateMessagingType (this={:x}, messaging={}) ===",
               (uintptr_t)_this, messagingType);
  original(_this, messagingType);
  spdlog::info("[LC] === TRANSITION: UpdateMessagingType DONE ===");
}

// TransitionManager.CanHide — check if TM can hide
static bool TransitionManager_CanHide_Hook(auto original, void* _this)
{
  bool result = original(_this);
  spdlog::info("[LC] CanHide: result={}", result);
  if (!result) {
    LogTMState("CanHide false", _this);
  }
  return result;
}

// TransitionManager.Hide — hides the transition
static void TransitionManager_Hide_Hook(auto original, void* _this, void* status)
{
  spdlog::info("[LC] === TRANSITION: Hide (this={:x}) ===", (uintptr_t)_this);
  LogTMState("Hide pre", _this);
  original(_this, status);
  spdlog::info("[LC] === TRANSITION: Hide DONE ===");
  LogTMState("Hide post", _this);
}

// TransitionManager.OnEnterSection — enter section callback
static int32_t TransitionManager_OnEnterSection_Hook(auto original, void* _this,
                                                     void* status, int32_t phase, void* storage)
{
  int32_t result = original(_this, status, phase, storage);
  spdlog::info("[LC] OnEnterSection: phase={} ({}) result={} ({})",
               phase, SectionEnterPhaseName(phase), result, CheckFeedbackName(result));
  if (result != 2) {
    LogTMState("OnEnterSection non-Ok", _this);
  }
  return result;
}

// TransitionManager.OnLeaveSection — leave section callback
static int32_t TransitionManager_OnLeaveSection_Hook(auto original, void* _this,
                                                     void* status, int32_t phase, void* storage)
{
  int32_t result = original(_this, status, phase, storage);
  spdlog::info("[LC] OnLeaveSection: phase={} ({}) result={} ({})",
               phase, SectionLeavePhaseName(phase), result, CheckFeedbackName(result));
  if (result != 2) {
    LogTMState("OnLeaveSection non-Ok", _this);
  }
  return result;
}

// TransitionManager.CheckResourcesOnLeave — phase 0 check
static int32_t TransitionManager_CheckResourcesOnLeave_Hook(auto original, void* _this,
                                                            void* status, int32_t phase, void* storage)
{
  int32_t result = original(_this, status, phase, storage);
  spdlog::info("[LC] CheckResourcesOnLeave: phase={} ({}) result={} ({})",
               phase, SectionLeavePhaseName(phase), result, CheckFeedbackName(result));
  if (result != 2) {
    LogStreamingState("CheckResourcesOnLeave non-Ok");
  }
  return result;
}

// TransitionManager.get_LoadingScreenActive — state check
static bool TransitionManager_GetLoadingScreenActive_Hook(auto original, void* _this)
{
  bool result = original(_this);
  int32_t state = _this ? *reinterpret_cast<int32_t*>((char*)_this + 0x58) : -1;
  spdlog::info("[LC] get_LoadingScreenActive: state={} ({}) -> {}", state, StateName(state), result);
  return result;
}

// TransitionViewController lifecycle
static void TransitionViewController_Awake_Hook(auto original, void* _this)
{
  spdlog::info("[LC] TransitionViewController.Awake (this={:x})", (uintptr_t)_this);
  original(_this);
}

static void TransitionViewController_AboutToShow_Hook(auto original, void* _this)
{
  spdlog::info("[LC] TransitionViewController.AboutToShow (this={:x})", (uintptr_t)_this);
  original(_this);
}

static void TransitionViewController_AboutToHide_Hook(auto original, void* _this)
{
  spdlog::info("[LC] TransitionViewController.AboutToHide (this={:x})", (uintptr_t)_this);
  original(_this);
}

// ===========================================================================
// RELOAD LIFECYCLE HOOKS
// ===========================================================================

// PrimeApp.Reload — entry point for reload
static void PrimeApp_Reload_Hook(auto original, void* _this,
                                 bool quickReloading, void* customReloadArgs, bool forceReload)
{
  spdlog::warn("[LC] =============== RELOAD START: PrimeApp.Reload ===============");
  spdlog::warn("[LC] Reload: quickReloading={} forceReload={}", quickReloading, forceReload);
  LogHubState("Reload pre");
  LogIsReloading("Reload pre", _this);
  original(_this, quickReloading, customReloadArgs, forceReload);
  spdlog::warn("[LC] =============== RELOAD: PrimeApp.Reload RETURNED ===============");
  LogHubState("Reload post");
  LogIsReloading("Reload post", _this);
}

// PrimeApp.StartReload — checks IsReloading, calls DoReload
static void PrimeApp_StartReload_Hook(auto original, void* _this, void* reloadArgs)
{
  spdlog::warn("[LC] === RELOAD STEP: PrimeApp.StartReload ===");
  original(_this, reloadArgs);
  spdlog::warn("[LC] === RELOAD STEP: PrimeApp.StartReload DONE ===");
}

// PrimeApp.DoReload — core reload orchestration
static void PrimeApp_DoReload_Hook(auto original, void* _this, void* reloadArgs)
{
  spdlog::warn("[LC] === RELOAD STEP: PrimeApp.DoReload (this={:x}) ===", (uintptr_t)_this);
  LogHubState("DoReload pre");
  LogIsReloading("DoReload pre", _this);
  original(_this, reloadArgs);
  spdlog::warn("[LC] === RELOAD STEP: PrimeApp.DoReload DONE ===");
  LogHubState("DoReload post");
  LogIsReloading("DoReload post", _this);
}

// PrimeApp.ReloadHub — re-initializes all Hub static properties
static void PrimeApp_ReloadHub_Hook(auto original, void* _this, bool quickReloading)
{
  spdlog::warn("[LC] === RELOAD STEP: PrimeApp.ReloadHub (quick={}) ===", quickReloading);
  LogHubState("ReloadHub pre");
  original(_this, quickReloading);
  spdlog::warn("[LC] === RELOAD STEP: PrimeApp.ReloadHub DONE ===");
  LogHubState("ReloadHub post");
}

// PrimeApp.GoToLoginSection — navigates to login section after reload
static void PrimeApp_GoToLoginSection_Hook(auto original, void* _this, bool quickReload)
{
  spdlog::warn("[LC] === RELOAD STEP: PrimeApp.GoToLoginSection (quick={}) ===", quickReload);
  LogHubState("GoToLoginSection pre");
  LogIsReloading("GoToLoginSection pre", _this);
  original(_this, quickReload);
  spdlog::warn("[LC] === RELOAD STEP: PrimeApp.GoToLoginSection DONE ===");
  LogHubState("GoToLoginSection post");
  LogIsReloading("GoToLoginSection post", _this);
}

// PrimeApp.ClearStaticEvents — removes all static event handlers
static void PrimeApp_ClearStaticEvents_Hook(auto original, void* _this)
{
  spdlog::warn("[LC] === RELOAD STEP: PrimeApp.ClearStaticEvents ===");
  original(_this);
  spdlog::warn("[LC] === RELOAD STEP: PrimeApp.ClearStaticEvents DONE ===");
}

// MonoSingleton.PrepareAllForReload — calls OnApplicationPrepareReload on all singletons
static void MonoSingleton_PrepareAllForReload_Hook(auto original)
{
  spdlog::warn("[LC] === RELOAD STEP: MonoSingleton.PrepareAllForReload ===");
  original();
  spdlog::warn("[LC] === RELOAD STEP: MonoSingleton.PrepareAllForReload DONE ===");
}

// MonoSingleton.ReloadAll — calls OnApplicationReload on all singletons
static void MonoSingleton_ReloadAll_Hook(auto original)
{
  spdlog::warn("[LC] === RELOAD STEP: MonoSingleton.ReloadAll ===");
  original();
  spdlog::warn("[LC] === RELOAD STEP: MonoSingleton.ReloadAll DONE ===");
}

// TransitionManager.OnApplicationReload — old TM cleanup during reload
static void TransitionManager_OnApplicationReload_Hook(auto original, void* _this)
{
  spdlog::warn("[LC] === RELOAD STEP: TransitionManager.OnApplicationReload (this={:x}) ===", (uintptr_t)_this);
  LogTMState("OnApplicationReload pre", _this);
  original(_this);
  spdlog::warn("[LC] === RELOAD STEP: TransitionManager.OnApplicationReload DONE ===");
  LogTMState("OnApplicationReload post", _this);
}

// ===========================================================================
// ADDITIONAL HOOKS — SectionManager, SectionDirectorMemoryManager, BlurController,
// CanvasController, VideoPlayerManager
// ===========================================================================

// SectionManager.CancelAllSectionChanges — called during DoReload
static void SectionManager_CancelAllSectionChanges_Hook(auto original, void* _this)
{
  spdlog::warn("[LC] === RELOAD STEP: SectionManager.CancelAllSectionChanges ===");
  original(_this);
  spdlog::warn("[LC] === RELOAD STEP: SectionManager.CancelAllSectionChanges DONE ===");
}

// SectionManager.ClearSectionHistory — may be called during reload
static void SectionManager_ClearSectionHistory_Hook(auto original, void* _this)
{
  spdlog::warn("[LC] === RELOAD STEP: SectionManager.ClearSectionHistory ===");
  original(_this);
  spdlog::warn("[LC] === RELOAD STEP: SectionManager.ClearSectionHistory DONE ===");
}

// SectionDirectorMemoryManager.ResetLoadState — clears section director memory during DoReload
static void SectionDirectorMemoryManager_ResetLoadState_Hook(auto original, void* _this)
{
  spdlog::warn("[LC] === RELOAD STEP: SectionDirectorMemoryManager.ResetLoadState ===");
  original(_this);
  spdlog::warn("[LC] === RELOAD STEP: SectionDirectorMemoryManager.ResetLoadState DONE ===");
}

// BlurController.SetBlur — starts blur transition
static void BlurController_SetBlur_Hook(auto original, void* _this)
{
  spdlog::info("[LC] BlurController.SetBlur (this={:x})", (uintptr_t)_this);
  original(_this);
}

// BlurController.SetClear — starts clear transition
static void BlurController_SetClear_Hook(auto original, void* _this)
{
  spdlog::info("[LC] BlurController.SetClear (this={:x})", (uintptr_t)_this);
  original(_this);
}

// BlurController.ForceCompletion — forces blur tween to complete
static void BlurController_ForceCompletion_Hook(auto original, void* _this)
{
  spdlog::info("[LC] BlurController.ForceCompletion (this={:x})", (uintptr_t)_this);
  if (_this) {
    float tweenVal = *reinterpret_cast<float*>((char*)_this + 0x20);
    float tweenTgt = *reinterpret_cast<float*>((char*)_this + 0x24);
    spdlog::info("[LC] ForceCompletion pre: tweenVal={:.4f} tweenTgt={:.4f}", tweenVal, tweenTgt);
  }
  original(_this);
  if (_this) {
    float tweenVal = *reinterpret_cast<float*>((char*)_this + 0x20);
    float tweenTgt = *reinterpret_cast<float*>((char*)_this + 0x24);
    spdlog::info("[LC] ForceCompletion post: tweenVal={:.4f} tweenTgt={:.4f}", tweenVal, tweenTgt);
  }
}

// VideoPlayerManager.SetupVideoPlayer — re-sets up video player during DoReload
static void VideoPlayerManager_SetupVideoPlayer_Hook(auto original, void* _this)
{
  spdlog::warn("[LC] === RELOAD STEP: VideoPlayerManager.SetupVideoPlayer ===");
  original(_this);
  spdlog::warn("[LC] === RELOAD STEP: VideoPlayerManager.SetupVideoPlayer DONE ===");
}

// ===========================================================================
// INSTALLATION
// ===========================================================================

#define LC_INSTALL_HOOK(HELPER, KLASS, METHOD, HOOK, LABEL)                                                             \
  do {                                                                                                                  \
    if (auto m = (HELPER).GetMethod(METHOD)) {                                                                          \
      SPUD_STATIC_DETOUR(m, HOOK);                                                                                      \
      spdlog::info("[LC] Hook installed: " LABEL);                                                                      \
    } else {                                                                                                            \
      ErrorMsg::MissingMethod(KLASS, METHOD);                                                                           \
    }                                                                                                                   \
  } while (0)

void InstallLifecycleLogger()
{
  spdlog::info("[LC] Installing lifecycle logger hooks...");

  // --- PrimeApp ---
  auto pa_h = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "PrimeApp");
  if (pa_h.isValidHelper()) {
    LC_INSTALL_HOOK(pa_h, "PrimeApp", "InitPlatformServer", PrimeApp_InitPlatformServer_Hook, "PrimeApp.InitPlatformServer");
    LC_INSTALL_HOOK(pa_h, "PrimeApp", "Reload", PrimeApp_Reload_Hook, "PrimeApp.Reload");
    LC_INSTALL_HOOK(pa_h, "PrimeApp", "StartReload", PrimeApp_StartReload_Hook, "PrimeApp.StartReload");
    LC_INSTALL_HOOK(pa_h, "PrimeApp", "DoReload", PrimeApp_DoReload_Hook, "PrimeApp.DoReload");
    LC_INSTALL_HOOK(pa_h, "PrimeApp", "ReloadHub", PrimeApp_ReloadHub_Hook, "PrimeApp.ReloadHub");
    LC_INSTALL_HOOK(pa_h, "PrimeApp", "GoToLoginSection", PrimeApp_GoToLoginSection_Hook, "PrimeApp.GoToLoginSection");
    LC_INSTALL_HOOK(pa_h, "PrimeApp", "ClearStaticEvents", PrimeApp_ClearStaticEvents_Hook, "PrimeApp.ClearStaticEvents");
  } else {
    ErrorMsg::MissingHelper("Core", "PrimeApp");
  }

  // --- Hub ---
  auto hub_h = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core", "Hub");
  if (hub_h.isValidHelper()) {
    // Hub.Init is a static method
    if (auto m = hub_h.GetMethod("Init", 0)) {
      SPUD_STATIC_DETOUR(m, Hub_Init_Hook);
      spdlog::info("[LC] Hook installed: Hub.Init");
    } else {
      ErrorMsg::MissingMethod("Hub", "Init");
    }
  } else {
    ErrorMsg::MissingHelper("Core", "Hub");
  }

  // --- TransitionManager ---
  auto tm_h = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.LoadingScreen", "TransitionManager");
  if (tm_h.isValidHelper()) {
    LC_INSTALL_HOOK(tm_h, "TransitionManager", "Awake", TransitionManager_Awake_Hook, "TM.Awake");
    LC_INSTALL_HOOK(tm_h, "TransitionManager", "OnEnable", TransitionManager_OnEnable_Hook, "TM.OnEnable");
    LC_INSTALL_HOOK(tm_h, "TransitionManager", "OnDisable", TransitionManager_OnDisable_Hook, "TM.OnDisable");
    LC_INSTALL_HOOK(tm_h, "TransitionManager", "OnDestroy", TransitionManager_OnDestroy_Hook, "TM.OnDestroy");
    LC_INSTALL_HOOK(tm_h, "TransitionManager", "SetLoadingScreen", TransitionManager_SetLoadingScreen_Hook, "TM.SetLoadingScreen");
    LC_INSTALL_HOOK(tm_h, "TransitionManager", "Hide", TransitionManager_Hide_Hook, "TM.Hide");
    LC_INSTALL_HOOK(tm_h, "TransitionManager", "OnEnterSection", TransitionManager_OnEnterSection_Hook, "TM.OnEnterSection");
    LC_INSTALL_HOOK(tm_h, "TransitionManager", "OnLeaveSection", TransitionManager_OnLeaveSection_Hook, "TM.OnLeaveSection");
    LC_INSTALL_HOOK(tm_h, "TransitionManager", "CheckResourcesOnLeave", TransitionManager_CheckResourcesOnLeave_Hook, "TM.CheckResourcesOnLeave");
    LC_INSTALL_HOOK(tm_h, "TransitionManager", "OnApplicationReload", TransitionManager_OnApplicationReload_Hook, "TM.OnApplicationReload");
  } else {
    ErrorMsg::MissingHelper("LoadingScreen", "TransitionManager");
  }

  // --- TransitionViewController ---
  auto tv_h = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.LoadingScreen", "TransitionViewController");
  if (tv_h.isValidHelper()) {
    LC_INSTALL_HOOK(tv_h, "TransitionViewController", "Awake", TransitionViewController_Awake_Hook, "TVC.Awake");
    LC_INSTALL_HOOK(tv_h, "TransitionViewController", "AboutToShow", TransitionViewController_AboutToShow_Hook, "TVC.AboutToShow");
    LC_INSTALL_HOOK(tv_h, "TransitionViewController", "AboutToHide", TransitionViewController_AboutToHide_Hook, "TVC.AboutToHide");
  } else {
    ErrorMsg::MissingHelper("LoadingScreen", "TransitionViewController");
  }

  // --- LoginSequence ---
  auto ls_h = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Login", "LoginSequence");
  if (ls_h.isValidHelper()) {
    LC_INSTALL_HOOK(ls_h, "LoginSequence", "Awake", LoginSequence_Awake_Hook, "LoginSequence.Awake");
    LC_INSTALL_HOOK(ls_h, "LoginSequence", "UpdateLoginStage", LoginSequence_UpdateLoginStage_Hook, "LoginSequence.UpdateLoginStage");
    // set_CurrentLoginStage has 1 arg
    if (auto m = ls_h.GetMethod("set_CurrentLoginStage", 1)) {
      SPUD_STATIC_DETOUR(m, LoginSequence_SetCurrentLoginStage_Hook);
      spdlog::info("[LC] Hook installed: LoginSequence.set_CurrentLoginStage");
    } else {
      ErrorMsg::MissingMethod("LoginSequence", "set_CurrentLoginStage");
    }
  } else {
    ErrorMsg::MissingHelper("Login", "LoginSequence");
  }

  // --- SectionManager ---
  auto sm_h = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Sections", "SectionManager");
  if (sm_h.isValidHelper()) {
    // TriggerSectionChange has 5 args
    if (auto m = sm_h.GetMethod("TriggerSectionChange", 5)) {
      SPUD_STATIC_DETOUR(m, SectionManager_TriggerSectionChange_Hook);
      spdlog::info("[LC] Hook installed: SectionManager.TriggerSectionChange");
    } else {
      ErrorMsg::MissingMethod("SectionManager", "TriggerSectionChange");
    }
  } else {
    ErrorMsg::MissingHelper("Sections", "SectionManager");
  }

  // --- MonoSingleton ---
  auto ms_h = il2cpp_get_class_helper("Assembly-CSharp", "", "MonoSingleton");
  if (ms_h.isValidHelper()) {
    LC_INSTALL_HOOK(ms_h, "MonoSingleton", "PrepareAllForReload", MonoSingleton_PrepareAllForReload_Hook, "MonoSingleton.PrepareAllForReload");
    LC_INSTALL_HOOK(ms_h, "MonoSingleton", "ReloadAll", MonoSingleton_ReloadAll_Hook, "MonoSingleton.ReloadAll");
  } else {
    ErrorMsg::MissingHelper("", "MonoSingleton");
  }

  spdlog::info("[LC] Lifecycle logger hooks installed.");
}

#undef LC_INSTALL_HOOK
