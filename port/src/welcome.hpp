// The first start's CHOOSE YOUR STYLE screen, before the start menu: CLASSIC or ENHANCED (settings.hpp's styles),
// ENHANCED first, shown until one is chosen; Escape does nothing there. GTA2_WELCOME=1 shows it again.
#pragma once

class Frontend;

namespace PortWelcome
{
// In the frontend's menu state, before its page: true while the screen takes the input.
bool Update(Frontend* pFrontend);
// In Frontend::Render: true when it drew the frame in place of the frontend's page.
bool Draw();
} // namespace PortWelcome
