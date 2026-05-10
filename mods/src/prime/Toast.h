#pragma once

#include <il2cpp/il2cpp_helper.h>

enum ToastState {
  Standard                  = 0,
  FactionWarning            = 1,
  FactionLevelUp            = 2,
  FactionLevelDown          = 3,
  FactionDiscovered         = 4,
  IncomingAttack            = 5,
  IncomingAttackFaction     = 6,
  FleetBattle               = 7,
  StationBattle             = 8,
  StationVictory            = 9,
  Victory                   = 10,
  Defeat                    = 11,
  StationDefeat             = 12,
  Tournament                = 14,
  ArmadaCreated             = 15,
  ArmadaCanceled            = 16,
  ArmadaIncomingAttack      = 17,
  ArmadaBattleWon           = 18,
  ArmadaBattleLost          = 19,
  DiplomacyUpdated          = 20,
  JoinedTakeover            = 21,
  CompetitorJoinedTakeover  = 22,
  AbandonedTerritory        = 23,
  TakeoverVictory           = 24,
  TakeoverDefeat            = 25,
  TreasuryProgress          = 26,
  TreasuryFull              = 27,
  Achievement               = 28,
  AssaultVictory            = 29,
  AssaultDefeat             = 30,
  ChallengeComplete         = 31,
  ChallengeFailed           = 32,
  StrikeHit                 = 33,
  StrikeDefeat              = 34,
  WarchestProgress          = 35,
  WarchestFull              = 36,
  PartialVictory            = 37,
  ArenaTimeLeft             = 38,
  ChainedEventScored        = 39,
  FleetPresetApplied        = 40,
  SurgeWarmUpEnded          = 41,
  SurgeHostileGroupDefeated = 42,
  SurgeTimeLeft             = 43,
};

struct Toast {
public:
  void* get_TextLocaleTextContext()
  {
    return *reinterpret_cast<void**>(reinterpret_cast<char*>(this) + 0x20);
  }

  Il2CppObject* get_Data()
  {
    return *reinterpret_cast<Il2CppObject**>(reinterpret_cast<char*>(this) + 0x38);
  }

  int get_State()
  {
    static auto prop = get_class_helper().GetProperty("State");
    return *prop.Get<ToastState>((void *)this);
  }

private:
  static IL2CppClassHelper &get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "Toast");
    return class_helper;
  }
};
