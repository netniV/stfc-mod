#include "errormsg.h"

#include "patches/navigation.h"

#include "prime/Hub.h"
#include "prime/NavigationSectionManager.h"
#include "prime/ScreenManager.h"

void GotoSection(SectionID section_id, void* section_data)
{
  Hub::get_SectionManager()->TriggerSectionChange(section_id, section_data, false, false, true);
}

void ChangeNavigationSection(SectionID section_id)
{
  const auto section_data = Hub::get_SectionManager()->_sectionStorage->GetState(section_id);

  if (section_data) {
    GotoSection(section_id, section_data);
  } else {
    NavigationSectionManager::ChangeNavigationSection(section_id);
  }
}

bool MoveOfficerCanvas(bool go_left)
{
  go_left = go_left;

  auto const canvas = ScreenManager::GetTopCanvas(true);
  if (strcmp(((Il2CppObject*)(canvas))->klass->name, "OfficerShowcase_Canvas") == 0) {}

  return false;
}