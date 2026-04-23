#include "imgui.h"
#include "soloud_demo_framework.h"

namespace
{
void draw_unavailable_demo(const char* title, const char* reason)
{
    DemoUpdateStart();
    ONCE(ImGui::SetNextWindowPos(ImVec2(20, 20)));
    ImGui::Begin(title);
    ImGui::Text("%s", reason);
    ImGui::Text("Press Esc to return to the launcher by restarting this demo.");
    ImGui::End();
    DemoUpdateEnd();
}
}

int DemoEntry_space(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    return 0;
}

void DemoMainloop_space()
{
    draw_unavailable_demo("space", "space demo needs OpenMPT and BRUCE.S3M, which are intentionally not linked in this project.");
}
