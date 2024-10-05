// Copyright Valve Corporation, All rights reserved.

#ifndef SE_DEDICATED_VGUI_VGUI_HELPERS_H_
#define SE_DEDICATED_VGUI_VGUI_HELPERS_H_

#include "tier1/interface.h"

namespace se::dedicated {

int StartVGUI(CreateInterfaceFn dedicatedFactory);
void StopVGUI();
void RunVGUIFrame();
bool VGUIIsRunning();
bool VGUIIsStopping();
bool VGUIIsInConfig();
void VGUIFinishedConfig();
void VGUIPrintf(const char *msg);

}  // namespace se::dedicated

#endif  // !SE_DEDICATED_VGUI_VGUI_HELPERS_H_
