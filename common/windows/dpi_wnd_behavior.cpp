// Copyright Valve Corporation, All rights reserved.
//
// DPI window behavior.

#include "stdafx.h"
#include "dpi_wnd_behavior.h"

#include "winlite.h"
#include <windowsx.h>  // GetWindowFont
#include <tuple>

#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace {

/**
 * @brief Update window child controls and DPI dependent things.
 * @param parentWnd Window.
 * @param parentOldDpiX Old X DPI value for window.
 * @param parentOldDpiY Old Y DPI value for window.
 */
void UpdateDpiDependentThings(HWND parent_window, unsigned parent_old_dpi_x,
                              unsigned parent_old_dpi_y) {
  const unsigned new_dpi_y{::GetDpiForWindow(parent_window)};

  // Send a new font to all child controls.
  const HFONT old_font{GetWindowFont(parent_window)};
  // "For uiAction values that contain strings within their associated
  // structures, only Unicode (LOGFONTW) strings are supported in this
  // function."
  //
  // See
  // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-systemparametersinfofordpi
  LOGFONTW font_settings = {};
  ::SystemParametersInfoForDpi(SPI_GETICONTITLELOGFONT, sizeof(font_settings),
                               &font_settings, FALSE, new_dpi_y);

  const HFONT new_font{::CreateFontIndirectW(&font_settings)};
  if (new_font) {
    ::DeleteObject(old_font);

    // Set a new font to our window.
    ::SendMessage(parent_window, WM_SETFONT, reinterpret_cast<WPARAM>(new_font),
                  MAKELPARAM(TRUE, 0));
  }

  const auto change_dpi_request =
      std::make_tuple(parent_old_dpi_x, parent_old_dpi_y, new_font);

  ::EnumChildWindows(
      parent_window,
      [](HWND child_window, LPARAM lParam) {
        auto* parent_old_dpis =
            reinterpret_cast<std::tuple<unsigned, unsigned, HFONT>*>(lParam);
        const auto& [old_dpi_x, old_dpi_y, scaled_font] = *parent_old_dpis;

        const unsigned new_x_dpi{::GetDpiForWindow(child_window)};
        // Microsoft docs claim they are same.
        const unsigned new_y_dpi{new_x_dpi};

        // Child window rect in screen coordinates.
        RECT rc_child;
        ::GetWindowRect(child_window, &rc_child);

        // Parent window rect in screen coordinates.
        const HWND parent_window{::GetParent(child_window)};
        RECT rc_parent;
        ::GetWindowRect(parent_window, &rc_parent);

        // Transforming the child window pos from screen space to parent window
        // space.
        POINT child_pos{rc_child.left, rc_child.top};
        ::ScreenToClient(parent_window, &child_pos);

        const int scaled_x{::MulDiv(child_pos.x, new_x_dpi, old_dpi_x)};
        const int scaled_y{::MulDiv(child_pos.y, new_y_dpi, old_dpi_y)};
        const int scaled_width{
            ::MulDiv(rc_child.right - rc_child.left, new_x_dpi, old_dpi_x)};
        const int scaled_height{
            ::MulDiv(rc_child.bottom - rc_child.top, new_y_dpi, old_dpi_y)};

        ::SetWindowPos(child_window, nullptr, scaled_x, scaled_y, scaled_width,
                       scaled_height, SWP_NOZORDER | SWP_NOACTIVATE);

        // Send a new font to all child controls.
        if (scaled_font) {
          ::SendMessage(child_window, WM_SETFONT,
                        reinterpret_cast<WPARAM>(scaled_font),
                        MAKELPARAM(TRUE, 0));
        }

        return 1;
      },
      reinterpret_cast<LPARAM>(&change_dpi_request));
}

// Get the font for the window, then delete it.
void DeleteWindowFont(HWND window) {
  // Get a handle to the font.
  HFONT font{GetWindowFont(window)};
  if (!font) return;

  SetWindowFont(window, nullptr, FALSE);
  ::DeleteObject(font);
}

}  // namespace

namespace se::windows::ui {

CDpiWindowBehavior::CDpiWindowBehavior()
    : m_window_handle{nullptr},
      m_current_dpi_x{USER_DEFAULT_SCREEN_DPI},
      m_current_dpi_y{USER_DEFAULT_SCREEN_DPI} {}

BOOL CDpiWindowBehavior::OnCreateWindow(HWND window) {
  Assert(window);

  m_window_handle = window;
  // Windows docs say DPI is same for X and Y.
  m_current_dpi_x = m_current_dpi_y = ::GetDpiForWindow(window);

  RECT rc_window;
  ::GetClientRect(window, &rc_window);

  // Compute and apply new window rectangle based on DPI.
  const BOOL window_has_menu{::GetMenu(window) ? TRUE : FALSE};
  ::AdjustWindowRectExForDpi(
      &rc_window, GetWindowLong(window, GWL_STYLE), window_has_menu,
      GetWindowLong(window, GWL_EXSTYLE), m_current_dpi_y);
  ::SetWindowPos(window, nullptr, 0, 0, rc_window.right - rc_window.left,
                 rc_window.bottom - rc_window.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

  // Need to notify children and dependent controls.
  UpdateDpiDependentThings(window, m_current_dpi_x, m_current_dpi_y);

  return TRUE;
}

void CDpiWindowBehavior::OnDestroyWindow() {
  Assert(m_window_handle);

  DeleteWindowFont(m_window_handle);
  m_window_handle = nullptr;
}

[[nodiscard]] int CDpiWindowBehavior::ScaleOnX(int value) const {
  return ::MulDiv(value, m_current_dpi_x, USER_DEFAULT_SCREEN_DPI);
}

[[nodiscard]] int CDpiWindowBehavior::ScaleOnY(int value) const {
  return ::MulDiv(value, m_current_dpi_y, USER_DEFAULT_SCREEN_DPI);
}

LRESULT CDpiWindowBehavior::OnWindowDpiChanged(WPARAM wParam, LPARAM lParam) {
  Assert(m_window_handle);

  // Get new window rectangle Windows suggests to us.
  const auto* new_rect = reinterpret_cast<RECT*>(lParam);
  Assert(new_rect);

  ::SetWindowPos(m_window_handle, nullptr, new_rect->left, new_rect->top,
                 new_rect->right - new_rect->left,
                 new_rect->bottom - new_rect->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);

  // Need to notify children and dependent controls.
  UpdateDpiDependentThings(m_window_handle, m_current_dpi_x, m_current_dpi_y);

  m_current_dpi_x = LOWORD(wParam);
  m_current_dpi_y = HIWORD(wParam);

  return 0;
}

}  // namespace se::windows::ui