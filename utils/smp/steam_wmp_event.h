// Copyright Valve Corporation, All rights reserved.

#ifndef SE_UTILS_SMP_STEAM_WMP_EVENT_H_
#define SE_UTILS_SMP_STEAM_WMP_EVENT_H_

using HRESULT = long;

namespace se::smp {

// event logging.
enum class SmpPlayerEvent {
  AppLaunch,
  AppExit,
  AppClose,
  FadeOut,

  BeginMedia,
  EndMedia,

  Jump2Home,
  Jump2End,

  Buffering,
  Waiting,
  Transitioning,
  Ready,
  Reconnecting,

  Play,
  Pause,
  Stop,
  ScrubFrom,
  ScrubTo,
  StepForward,
  StepBack,
  JumpFroward,
  JumpBack,
  Repeat,

  Maximize,
  Minimize,
  Restore,

  Error
};

bool FailedUI(HRESULT hr, const char* description);

}  // namespace se::smp

#endif  // !SE_UTILS_SMP_STEAM_WMP_EVENT_H_
