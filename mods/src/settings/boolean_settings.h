#pragma once
#include "value_settings.h"

namespace mod_settings
{
using ReadResult  = ValueReadResult<bool>;
using Definition  = ValueDefinition<bool>;
using Snapshot    = ValueSnapshot<bool>;
using WriteResult = ValueWriteResult<bool>;
class BooleanSetting : public ValueSetting<bool>
{
public:
  using ValueSetting<bool>::ValueSetting;
};

enum class Registration { Added, Duplicate, Invalid, Frozen };
class BooleanRegistry
{
public:
  Registration Register(Definition definition)
  {
    CheckThread();
    if (frozen_)
      return Registration::Frozen;
    if (definition.id.empty() || definition.label.empty() || !definition.read || !definition.write)
      return Registration::Invalid;
    if (settings_.contains(definition.id))
      return Registration::Duplicate;
    auto id = definition.id;
    settings_.try_emplace(std::move(id), std::move(definition));
    return Registration::Added;
  }
  BooleanSetting* Find(const std::string& id)
  {
    CheckThread();
    frozen_ = true;
    auto it = settings_.find(id);
    return it == settings_.end() ? nullptr : &it->second;
  }
  void InvalidateSession()
  {
    CheckThread();
    for (auto& [id, setting] : settings_)
      setting.InvalidateSession();
  }

private:
  void CheckThread() const
  {
    if (std::this_thread::get_id() != thread_)
      throw std::logic_error("registry thread mismatch");
  }
  const std::thread::id                 thread_ = std::this_thread::get_id();
  bool                                  frozen_ = false;
  std::map<std::string, BooleanSetting> settings_;
};
} // namespace mod_settings
