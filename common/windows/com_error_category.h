// Copyright Valve Corporation, All rights reserved.
//
// COM error category.

#ifndef SE_COMMON_WINDOWS_COM_ERROR_CATEGORY_H_
#define SE_COMMON_WINDOWS_COM_ERROR_CATEGORY_H_

#include <comdef.h>

#include <string>
#include <system_error>

#ifdef SHADERAPIDX9
#include <d3d9.h>
#endif

using HRESULT = long;

namespace se::win::com {

#ifdef SHADERAPIDX9
/**
 * @brief Get DirectX 9 error description.
 * @param hr HRESULT.
 * @return const char* with error description or nullptr if no found.
 */
const char* GetD3DErrorDescription(HRESULT hr);
#endif

/**
 * @brief Component Object Model error category.
 */
class ComErrorCategory : public std::error_category {
 public:
  ComErrorCategory() noexcept = default;

  /**
   * @brief Error category name.
   * @return Category name.
   */
  [[nodiscard]] const char* name() const noexcept override { return "com"; }

  /**
   * @brief Gets error value HRESULT description.
   * @param error_value HRESULT.
   * @return Error description.
   */
  [[nodiscard]] std::string message(int error_value) const override {
    const _com_error com_error{static_cast<HRESULT>(error_value)};
    std::string message{com_error.ErrorMessage()};

    // Unknown error for FormatMessage, fallback to other methods.
    if (message.find("Unknown error 0x") == 0 ||
        message.find("IDispatch error #") == 0) {
#ifdef SHADERAPIDX9
      const char* d3dError = GetD3DErrorDescription(error_value);
      if (d3dError) message = d3dError;
#endif
    }

    return message;
  }
};

/**
 * @brief Gets COM error category singleton.
 * @return COM error category singleton.
 */
[[nodiscard]] inline ComErrorCategory& com_error_category() {
  static ComErrorCategory com_error_category;
  return com_error_category;
}

/**
 * @brief Get COM error code.
 * @param rc Native system COM error code.
 * @return System COM error code.
 */
[[nodiscard]] inline std::error_code GetComErrorCode(
    const HRESULT result) noexcept {
  return std::error_code{result, com_error_category()};
}

#ifdef SHADERAPIDX9
/**
 * @brief Get DirectX 9 error description.
 * @param hr HRESULT.
 * @return const char* with error description.
 */
inline const char* GetD3DErrorDescription(HRESULT hr) {
  switch (hr) {
    case D3DOK_NOAUTOGEN:
      return "D3DOK_NOAUTOGEN: This is a success code. However, the "
             "autogeneration of mipmaps is not supported for this format. This "
             "means that resource creation will succeed but the mipmap levels "
             "will not be automatically generated.";

    case D3DERR_CONFLICTINGRENDERSTATE:
      return "D3DERR_CONFLICTINGRENDERSTATE: The currently set render states "
             "cannot be used together.";

    case D3DERR_CONFLICTINGTEXTUREFILTER:
      return "D3DERR_CONFLICTINGTEXTUREFILTER: The current texture filters "
             "cannot be used together.";

    case D3DERR_CONFLICTINGTEXTUREPALETTE:
      return "D3DERR_CONFLICTINGTEXTUREPALETTE: The current textures cannot be "
             "used simultaneously.";

    case D3DERR_DEVICEHUNG:
      return "D3DERR_DEVICEHUNG: The device that returned this code caused the "
             "hardware adapter to be reset by the OS. Most applications should "
             "destroy the device and quit. Applications that must continue "
             "should destroy all video memory objects (surfaces, textures, "
             "state blocks etc) and call Reset() to put the device in a "
             "default state. If the application then continues rendering in "
             "the same way, the device will return to this state.";

    case D3DERR_DEVICELOST:
      return "D3DERR_DEVICELOST: The device has been lost but cannot be reset "
             "at this time. Therefore, rendering is not possible. A Direct3D "
             "device object other than the one that returned this code caused "
             "the hardware adapter to be reset by the OS. Delete all video "
             "memory objects (surfaces, textures, state blocks) and call "
             "Reset() to return the device to a default state. If the "
             "application continues rendering without a reset, the rendering "
             "calls will succeed.";

    case D3DERR_DEVICENOTRESET:
      return "D3DERR_DEVICENOTRESET: The device has been lost but can be reset "
             "at this time.";

    case D3DERR_DEVICEREMOVED:
      return "D3DERR_DEVICEREMOVED: The hardware adapter has been removed. "
             "Application must destroy the device, do enumeration of adapters "
             "and create another Direct3D device. If application continues "
             "rendering without calling Reset, the rendering calls will "
             "succeed.";

    case D3DERR_DRIVERINTERNALERROR:
      return "D3DERR_DRIVERINTERNALERROR: Internal driver error. Applications "
             "should destroy and recreate the device when receiving this "
             "error. For hints on debugging this error, see "
             "https://learn.microsoft.com/en-us/windows/win32/direct3d9/"
             "driver-internal-errors.";

    case D3DERR_DRIVERINVALIDCALL:
      return "D3DERR_DRIVERINVALIDCALL: Not used.";

    case D3DERR_INVALIDCALL:
      return "D3DERR_INVALIDCALL: The method call is invalid. For example, a "
             "method's parameter may not be a valid pointer.";

    case D3DERR_INVALIDDEVICE:
      return "D3DERR_INVALIDDEVICE: The requested device type is not valid.";

    case D3DERR_MOREDATA:
      return "D3DERR_MOREDATA: There is more data available than the specified "
             "buffer size can hold.";

    case D3DERR_NOTAVAILABLE:
      return "D3DERR_NOTAVAILABLE: This device does not support the queried "
             "technique.";

    case D3DERR_NOTFOUND:
      return "D3DERR_NOTFOUND: The requested item was not found.";

    case D3D_OK:
      return "D3D_OK: No error occurred.";

    case D3DERR_OUTOFVIDEOMEMORY:
      return "D3DERR_OUTOFVIDEOMEMORY: Direct3D does not have enough display "
             "memory to perform the operation. The device is using more "
             "resources in a single scene than can fit simultaneously into "
             "video memory. Present, PresentEx, or CheckDeviceState can return "
             "this error. Recovery is similar to D3DERR_DEVICEHUNG, though the "
             "application may want to reduce its per-frame memory usage as "
             "well to avoid having the error recur.";

    case D3DERR_TOOMANYOPERATIONS:
      return "D3DERR_TOOMANYOPERATIONS: The application is requesting more "
             "texture-filtering operations than the device supports.";

    case D3DERR_UNSUPPORTEDALPHAARG:
      return "D3DERR_UNSUPPORTEDALPHAARG: The device does not support a "
             "specified texture-blending argument for the alpha channel.";

    case D3DERR_UNSUPPORTEDALPHAOPERATION:
      return "D3DERR_UNSUPPORTEDALPHAOPERATION: The device does not support a "
             "specified texture-blending operation for the alpha channel.";

    case D3DERR_UNSUPPORTEDCOLORARG:
      return "D3DERR_UNSUPPORTEDCOLORARG: The device does not support a "
             "specified texture-blending argument for color values.";

    case D3DERR_UNSUPPORTEDCOLOROPERATION:
      return "D3DERR_UNSUPPORTEDCOLOROPERATION: The device does not support a "
             "specified texture-blending operation for color values.";

    case D3DERR_UNSUPPORTEDFACTORVALUE:
      return "D3DERR_UNSUPPORTEDFACTORVALUE: The device does not support the "
             "specified texture factor value. Not used; provided only to "
             "support older drivers.";

    case D3DERR_UNSUPPORTEDTEXTUREFILTER:
      return "D3DERR_UNSUPPORTEDTEXTUREFILTER: The device does not support the "
             "specified texture filter.";

    case D3DERR_WASSTILLDRAWING:
      return "D3DERR_WASSTILLDRAWING: The previous blit operation that is "
             "transferring information to or from this surface is incomplete.";

    case D3DERR_WRONGTEXTUREFORMAT:
      return "D3DERR_WRONGTEXTUREFORMAT: The pixel format of the texture "
             "surface is not valid.";

    case E_FAIL:
      return "E_FAIL: An undetermined error occurred inside the Direct3D "
             "subsystem.";

    case E_INVALIDARG:
      return "E_INVALIDARG: An invalid parameter was passed to the returning "
             "function.";

    case E_NOINTERFACE:
      return "E_NOINTERFACE: No object interface is available.";

    case E_NOTIMPL:
      return "E_NOTIMPL: Not implemented.";

    case E_OUTOFMEMORY:
      return "E_OUTOFMEMORY: Direct3D could not allocate sufficient memory to "
             "complete the call.";

    case S_NOT_RESIDENT:
      return "S_NOT_RESIDENT: At least one allocation that comprises the "
             "resources is on disk. Direct3D 9Ex only.";

    case S_RESIDENT_IN_SHARED_MEMORY:
      return "S_RESIDENT_IN_SHARED_MEMORY: No allocations that comprise the "
             "resources are on disk. However, at least one allocation is not "
             "in GPU-accessible memory. Direct3D 9Ex only.";

    case S_PRESENT_MODE_CHANGED:
      return "S_PRESENT_MODE_CHANGED: The desktop display mode has been "
             "changed. The application can continue rendering, but there might "
             "be color conversion/stretching. Pick a back buffer format "
             "similar to the current display mode, and call Reset to recreate "
             "the swap chains. The device will leave this state after a Reset "
             "is called. Direct3D 9Ex only.";

    case S_PRESENT_OCCLUDED:
      return "S_PRESENT_OCCLUDED: The presentation area is occluded. Occlusion "
             "means that the presentation window is minimized or another "
             "device entered the fullscreen mode on the same monitor as the "
             "presentation window and the presentation window is completely on "
             "that monitor. Occlusion will not occur if the client area is "
             "covered by another Window. Occluded applications can continue "
             "rendering and all calls will succeed, but the occluded "
             "presentation window will not be updated. Direct3D 9Ex only.";

    case D3DERR_UNSUPPORTEDOVERLAY:
      return "D3DERR_UNSUPPORTEDOVERLAY: The device does not support overlay "
             "for the specified size or display mode. Direct3D 9Ex under "
             "Windows 7 only.";

    case D3DERR_UNSUPPORTEDOVERLAYFORMAT:
      return "D3DERR_UNSUPPORTEDOVERLAYFORMAT: The device does not support "
             "overlay for the specified surface format. Direct3D 9Ex under "
             "Windows 7 only.";

    case D3DERR_CANNOTPROTECTCONTENT:
      return "D3DERR_CANNOTPROTECTCONTENT: The specified content cannot be "
             "protected. Direct3D 9Ex under Windows 7 only.";

    case D3DERR_UNSUPPORTEDCRYPTO:
      return "D3DERR_UNSUPPORTEDCRYPTO: The specified cryptographic algorithm "
             "is not supported. Direct3D 9Ex under Windows 7 only.";

    case D3DERR_PRESENT_STATISTICS_DISJOINT:
      return "D3DERR_PRESENT_STATISTICS_DISJOINT: The present statistics have "
             "no orderly sequence. Direct3D 9Ex under Windows 7 only.";

    default:
      return nullptr;
  }
}
#endif

}  // namespace se::win::com

#endif  // !SE_COMMON_WINDOWS_COM_ERROR_CATEGORY_H_
