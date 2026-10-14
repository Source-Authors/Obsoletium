// Copyright Valve Corporation, All rights reserved.

#ifndef SE_DEDICATED_VGUI_VGUI_HELPERS_H_
#define SE_DEDICATED_VGUI_VGUI_HELPERS_H_

#include "tier1/interface.h"

namespace s1::dedicated {

int StartVGUI(CreateInterfaceFn dedicatedFactory);
void StopVGUI();
void RunVGUIFrame();
bool VGUIIsRunning();
bool VGUIIsStopping();
bool VGUIIsInConfig();
void VGUIFinishedConfig();
void VGUIPrintf(const char *msg);

}  // namespace s1::dedicated

#endif  // !SE_DEDICATED_VGUI_VGUI_HELPERS_H_
