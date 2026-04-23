#pragma once

#include "../../deps/imgui/imgui.h"

namespace ImGui
{
inline bool CollapsingHeader(const char* label, const char*, bool, bool default_open)
{
    ImGuiTreeNodeFlags flags = default_open ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;
    return CollapsingHeader(label, flags);
}
}
