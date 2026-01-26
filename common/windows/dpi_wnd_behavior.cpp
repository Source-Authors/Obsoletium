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

HFONT ScaleFontByDpi(HFONT old_font, unsigned previousDpiY, unsigned newDpiY) {
  using se::windows::ui::CDpiWindowBehavior;

  if (LOGFONT font_settings = {};
      ::GetObject(old_font, sizeof(font_settings), &font_settings)) {
    // Scale font.
    font_settings.lfHeight = CDpiWindowBehavior::ScaleByDpi(
        previousDpiY, font_settings.lfHeight, newDpiY);

    return ::CreateFontIndirect(&font_settings);
  }

  // Fallback to default system font when unable to get font.
  // "For uiAction values that contain strings within their associated
  // structures, only Unicode (LOGFONTW) strings are supported in this
  // function."
  //
  // See
  // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-systemparametersinfofordpi
  if (LOGFONTW font_settings = {}; ::SystemParametersInfoForDpi(
          SPI_GETICONTITLELOGFONT, sizeof(font_settings), &font_settings, FALSE,
          newDpiY)) {
    // Scale font.
    font_settings.lfHeight = CDpiWindowBehavior::ScaleByDpi(
        previousDpiY, font_settings.lfHeight, newDpiY);

    return ::CreateFontIndirectW(&font_settings);
  }

  return nullptr;
}

// Get the font for the window, then delete it.
BOOL DeleteWindowFont(HWND window) {
  // Get a handle to the font.
  HFONT font{GetWindowFont(window)};
  if (!font) return FALSE;

  SetWindowFont(window, nullptr, FALSE);
  return ::DeleteObject(font);
}

BOOL ApplyDpiToWindowFont(HWND window, unsigned previousDpiY,
                          unsigned newDpiY) {
  const HFONT old_font{GetWindowFont(window)};
  // Scale font by DPI.
  const HFONT new_font{ScaleFontByDpi(old_font, previousDpiY, newDpiY)};
  if (new_font) {
    // Set a new font to our window.
    ::SendMessage(window, WM_SETFONT, reinterpret_cast<WPARAM>(new_font),
                  MAKELPARAM(TRUE, 0));

    return old_font ? ::DeleteObject(old_font) : TRUE;
  }

  return FALSE;
}

/**
 * @brief Update window child controls and DPI dependent things.
 * @param parentWnd Window.
 * @param previousDpiX Old X DPI value for window.
 * @param previousDpiY Old Y DPI value for window.
 */
void ApplyDpiToWindow(HWND parentWnd, unsigned previousDpiX,
                      unsigned previousDpiY) {
  const unsigned new_dpi_y{::GetDpiForWindow(parentWnd)};

  ApplyDpiToWindowFont(parentWnd, previousDpiY, new_dpi_y);

  const auto change_dpi_request = std::make_tuple(previousDpiX, previousDpiY);

  // Return value is not used.
  // See
  // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumchildwindows
  (void)::EnumChildWindows(
      parentWnd,
      [](HWND child_window, LPARAM lParam) {
        auto* parent_old_dpis =
            reinterpret_cast<std::tuple<unsigned, unsigned>*>(lParam);
        const auto& [old_dpi_x, old_dpi_y] = *parent_old_dpis;

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

        // Transforming the child window pos from screen space to parent
        // window space.
        POINT child_pos{rc_child.left, rc_child.top};
        ::ScreenToClient(parent_window, &child_pos);

        using se::windows::ui::CDpiWindowBehavior;

        const int scaled_x{
            CDpiWindowBehavior::ScaleByDpi(old_dpi_x, child_pos.x, new_x_dpi)};
        const int scaled_y{
            CDpiWindowBehavior::ScaleByDpi(old_dpi_y, child_pos.y, new_y_dpi)};
        const int scaled_width{CDpiWindowBehavior::ScaleByDpi(
            old_dpi_x, rc_child.right - rc_child.left, new_x_dpi)};
        const int scaled_height{CDpiWindowBehavior::ScaleByDpi(
            old_dpi_y, rc_child.bottom - rc_child.top, new_y_dpi)};

        ::SetWindowPos(child_window, nullptr, scaled_x, scaled_y, scaled_width,
                       scaled_height, SWP_NOZORDER | SWP_NOACTIVATE);

        // Apply DPI scaling for font in all child controls.
        ApplyDpiToWindow(child_window, old_dpi_y, new_y_dpi);

        return 1;
      },
      reinterpret_cast<LPARAM>(&change_dpi_request));
}

}  // namespace

namespace se::windows::ui {

CDpiWindowBehavior::CDpiWindowBehavior(bool applyDpiOnCreate)
    : m_window_handle{nullptr},
      m_previous_dpi_x{USER_DEFAULT_SCREEN_DPI},
      m_previous_dpi_y{USER_DEFAULT_SCREEN_DPI},
      m_current_dpi_x{USER_DEFAULT_SCREEN_DPI},
      m_current_dpi_y{USER_DEFAULT_SCREEN_DPI},
      m_apply_dpi_on_create{applyDpiOnCreate}
#ifdef _DEBUG
      ,
      m_is_destroyed{false}
#endif
{
}

BOOL CDpiWindowBehavior::OnCreateWindow(HWND window) {
  Assert(window);

  m_window_handle = window;
#ifdef _DEBUG
  m_is_destroyed = false;
#endif
  // Windows docs say DPI is same for X and Y.
  m_previous_dpi_x = m_previous_dpi_y =
      std::exchange(m_current_dpi_x, ::GetDpiForWindow(window));
  m_current_dpi_y = m_current_dpi_x;

  // dimhotepus: Apply Dark mode if any and Mica styles to window title bar.
  Plat_ApplySystemTitleBarTheme(window, SystemBackdropType::TransientWindow);

  return ApplyDpiToWindow(m_apply_dpi_on_create);
}

void CDpiWindowBehavior::OnDestroyWindow() {
  Assert(!m_is_destroyed);
  Assert(m_window_handle);

  DeleteWindowFont(m_window_handle);
  m_window_handle = nullptr;
#ifdef _DEBUG
  m_is_destroyed = true;
#endif
}

[[nodiscard]] int CDpiWindowBehavior::ScaleOnX(int value) const {
  return ScaleByDpi(m_previous_dpi_x, value, m_current_dpi_x);
}

[[nodiscard]] int CDpiWindowBehavior::ScaleOnY(int value) const {
  return ScaleByDpi(m_previous_dpi_x, value, m_current_dpi_x);
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

  // Apply font scaling and notify children.
  ::ApplyDpiToWindow(m_window_handle, m_previous_dpi_x, m_previous_dpi_y);

  m_previous_dpi_x = std::exchange(m_current_dpi_x, LOWORD(wParam));
  m_previous_dpi_y = std::exchange(m_current_dpi_y, HIWORD(wParam));

  return 0;
}

BOOL CDpiWindowBehavior::ApplyDpiToWindow(bool recompute_window_size) {
  Assert(m_window_handle);

  BOOL rc{TRUE};

  if (RECT rc_window = {}; ::GetClientRect(m_window_handle, &rc_window)) {
    // Compute and apply new window rectangle based on DPI.
    const BOOL window_has_menu{::GetMenu(m_window_handle) ? TRUE : FALSE};

    rc = ::AdjustWindowRectExForDpi(
        &rc_window, GetWindowLong(m_window_handle, GWL_STYLE),
        window_has_menu,                                                //-V303
        GetWindowLong(m_window_handle, GWL_EXSTYLE), m_current_dpi_y);  //-V303
    Assert(rc);

    if (rc) {
      rc = ::SetWindowPos(m_window_handle, nullptr, 0, 0,
                          rc_window.right - rc_window.left,
                          rc_window.bottom - rc_window.top,
                          SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
      Assert(rc);
    }
  }

  if (rc && recompute_window_size) {
    // Apply font scaling and notify children.
    ::ApplyDpiToWindow(m_window_handle, m_previous_dpi_x, m_previous_dpi_y);
  }

  return rc;
}

int CDpiWindowBehavior::ScaleByDpi(unsigned oldDpi, int value,
                                   unsigned newDpi) {
  return ::MulDiv(value, newDpi, oldDpi);
}

}  // namespace se::windows::ui
