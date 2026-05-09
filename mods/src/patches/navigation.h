#pragma once

enum class SectionID : int;

void GotoSection(SectionID section_id, void* section_data = nullptr);
void ChangeNavigationSection(SectionID section_id);
bool MoveOfficerCanvas(bool go_left);