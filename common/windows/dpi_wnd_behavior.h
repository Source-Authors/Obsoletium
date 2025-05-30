// Copyright Valve Corporation, All rights reserved.
//
// DPI window behavior.

#ifndef SE_COMMON_WINDOWS_DPI_WND_BEHAVIOR_H_
#define SE_COMMON_WINDOWS_DPI_WND_BEHAVIOR_H_

#include "tier0/basetypes.h"

FORWARD_DECLARE_HANDLE(HWND);

#if defined(_WIN64)
typedef unsigned __int64 UINT_PTR, *PUINT_PTR;
typedef __int64 LONG_PTR, *PLONG_PTR;
#else
typedef unsigned int UINT_PTR, *PUINT_PTR; //-V677
typedef long LONG_PTR, *PLONG_PTR; //-V677
#endif

/* Types use for passing & returning polymorphic values */
typedef UINT_PTR WPARAM; //-V677
typedef LONG_PTR LPARAM; //-V677
typedef LONG_PTR LRESULT; //-V677

namespace se::windows::ui {

/**
 * @brief Window behavior to handle DPI settings.
 */
class CDpiWindowBehavior {
 public:
  explicit CDpiWindowBehavior(bool applyDpiOnCreate = true);

  /**
   * @brief Callback on WM_CREATE.
   * @param window Window.
   * @return TRUE if success, FALSE otherwise.
   */
  BOOL OnCreateWindow(HWND window);

  /**
   * @brief Callback on WM_DESTROY.
   */
  void OnDestroyWindow();

  /**
   * @brief Callback on WM_DPICHANGED.
   * @param wParam WPARAM.
   * @param lParam LPARAM.
   * @return 0 on handled, non 0 otherwise.
   */
  LRESULT OnWindowDpiChanged(WPARAM wParam, LPARAM lParam);

  /**
   * @brief Apply current DPI to window and children.
   */
  BOOL ApplyDpiToWindow(bool recompute_window_size);

  /**
   * @brief Previous X DPI.
   * @return
   */
  [[nodiscard]] unsigned GetPreviousDpiX() const { return m_previous_dpi_x; }

  /**
   * @brief Previous Y DPI.
   * @return
   */
  [[nodiscard]] unsigned GetPreviousDpiY() const { return m_previous_dpi_y; }

  /**
   * @brief Current X DPI.
   * @return
   */
  [[nodiscard]] unsigned GetCurrentDpiX() const { return m_current_dpi_x; }

  /**
   * @brief Current Y DPI.
   * @return
   */
  [[nodiscard]] unsigned GetCurrentDpiY() const { return m_current_dpi_y; }

  /**
   * @brief Apply scaling on X axis.
   * @param value Value to scale.
   * @return Scaled value.
   */
  [[nodiscard]] int ScaleOnX(int value) const;

  /**
   * @brief Apply scaling on Y axis.
   * @param value Value to scale.
   * @return Scaled value.
   */
  [[nodiscard]] int ScaleOnY(int value) const;

 private:
  HWND m_window_handle;

  unsigned m_previous_dpi_x, m_previous_dpi_y;
  unsigned m_current_dpi_x, m_current_dpi_y;

  const bool m_apply_dpi_on_create;
};

}  // namespace se::windows::ui

#endif  // !SE_COMMON_WINDOWS_DPI_WND_BEHAVIOR_H_
