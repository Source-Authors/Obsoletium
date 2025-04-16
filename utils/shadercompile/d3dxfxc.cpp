// Copyright Valve Corporation, All rights reserved.
//
// Purpose: D3DX command implementation.

#include "d3dxfxc.h"

#include "shadercompile.h"
#include "cmdsink.h"

#include "dx_proxy/dx_proxy.h"

#include <d3dcompiler.h>
#include <comdef.h>
#include "com_ptr.h"

#include "tier0/icommandline.h"
#include "tier1/strtools.h"

namespace se::shader_compile::fxc_intercept {

// The command that is intercepted by this namespace routines
static constexpr inline const char kFxcCommand[]{"fxc.exe "};
static constexpr size_t kFxcCommandSize{std::size(kFxcCommand) - 1};

namespace {

// dimhotepus: Allow to report non DirectX compiler errors in blobs.
class ErrorD3DBlob final : public ID3DBlob {
 public:
  explicit ErrorD3DBlob(std::string &&error) noexcept
      : m_error{std::move(error)}, m_ref_counter{1} {}
  ~ErrorD3DBlob() noexcept = default;

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID iid, LPVOID *ppv) override {
    if (!ppv) return E_POINTER;

    if (IsEqualGUID(iid, __uuidof(ID3DBlob))) {
      auto *blob = static_cast<ID3DBlob *>(this);
      blob->AddRef();
      *ppv = blob;
      return S_OK;
    }

    return E_NOINTERFACE;
  }
  STDMETHODIMP_(ULONG) AddRef() override {
    return m_ref_counter.fetch_add(1, std::memory_order::memory_order_relaxed);
  }
  STDMETHODIMP_(ULONG) Release() override {
    const ULONG rc =
        m_ref_counter.fetch_sub(1, std::memory_order::memory_order_acq_rel);
    if (rc == 0) delete this;
    return rc;
  }

  // ID3DBlob
  STDMETHODIMP_(LPVOID) GetBufferPointer() override { return m_error.data(); }
  STDMETHODIMP_(SIZE_T) GetBufferSize() override { return m_error.length(); }

 private:
  std::string m_error;
  std::atomic_ulong m_ref_counter;
};

class HLSLCompilerResponse final
    : public se::shader_compile::command_sink::IResponse {
 public:
  HLSLCompilerResponse(se::win::com::com_ptr<ID3DBlob> shader,
                       se::win::com::com_ptr<ID3DBlob> errors, HRESULT hr);
  virtual ~HLSLCompilerResponse() = default;

 public:
  bool Succeeded() const override { return shader_ && hr_ == S_OK; }

  size_t GetResultBufferLen() override {
    return Succeeded() ? shader_->GetBufferSize() : 0;
  }

  const void *GetResultBuffer() override {
    return Succeeded() ? shader_->GetBufferPointer() : nullptr;
  }

  const char *GetListing() override {
    return listing_ ? static_cast<const char *>(listing_->GetBufferPointer())
                    : nullptr;
  }

 private:
  se::win::com::com_ptr<ID3DBlob> shader_;
  se::win::com::com_ptr<ID3DBlob> listing_;
  const HRESULT hr_;
};

HLSLCompilerResponse::HLSLCompilerResponse(
    se::win::com::com_ptr<ID3DBlob> shader,
    se::win::com::com_ptr<ID3DBlob> errors, HRESULT hr)
    : shader_(std::move(shader)), listing_(std::move(errors)), hr_(hr) {
  // dimhotepus: Report error if no errors blob.
  if (!listing_ && FAILED(hr)) {
    auto err = _com_error{hr};
    auto msg = std::string{err.ErrorMessage()};
    listing_.Attach(new ErrorD3DBlob{std::move(msg)});
  }
}

// Perform a fast shader file compilation.
//
// @param file_name	filename to compile (e.g.
// "debugdrawenvmapmask_vs20.fxc")
// @param macroses null-terminated array of macro-defines
// @param shader_model shader model for compilation
static std::unique_ptr<se::shader_compile::command_sink::IResponse>
FastShaderCompile(const char *file_name, const D3D_SHADER_MACRO *macroses,
                  const char *shader_model) {
  // DxProxyModule.
  static se::dx_proxy::DxProxyModule d3dProxyModule;

  constexpr unsigned kCommonFlags{
      /* D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_ENABLE_STRICTNESS  // |
                                                                     // Gfa
      /* D3DCOMPILE_AVOID_FLOW_CONTROL */};

  [[maybe_unused]] constexpr unsigned kDebugOnlyFlags{
      // Zi
      D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION |
      D3DCOMPILE_DEBUG_NAME_FOR_SOURCE};

  [[maybe_unused]] constexpr unsigned kReleaseOnlyFlags{
      D3DCOMPILE_OPTIMIZATION_LEVEL3};

  const unsigned flags1{kCommonFlags |
#ifdef _DEBUG
                        kDebugOnlyFlags
#else
                        kReleaseOnlyFlags
#endif
  };

  se::win::com::com_ptr<ID3DBlob> shader, errors;
  HRESULT hr{d3dProxyModule.D3DCompileFromFile(file_name, macroses, nullptr,
                                               "main", shader_model, flags1, 0,
                                               &shader, &errors)};

  return std::make_unique<HLSLCompilerResponse>(std::move(shader),
                                                std::move(errors), hr);
}

};  // namespace

// Completely mimic the behaviour of "fxc.exe" in the specific cases related
// to shader compilations.
//
// Command in form:
// "fxc.exe /DSHADERCOMBO=1 /DTOTALSHADERCOMBOS=4 /DCENTROIDMASK=0
// /DNUMDYNAMICCOMBOS=4 /DFLAGS=0x0 /DNUM_BONES=1 /Dmain=main /Emain /Tvs_2_0
// /DSHADER_MODEL_VS_2_0=1 /nologo /Foshader.o
// debugdrawenvmapmask_vs20.fxc>output.txt 2>&1"
static std::unique_ptr<se::shader_compile::command_sink::IResponse>
ExecuteCommand(const char *in_command) {
  // Expect that the command passed is exactly "fxc.exe"
  Assert(!strncmp(in_command, kFxcCommand, kFxcCommandSize));

  in_command += kFxcCommandSize;

  // A duplicate portion of memory for modifications
  V_strdup_stack(in_command, editable_command);

  // Macros to be defined
  CUtlVector<D3D_SHADER_MACRO> macros{(intp)0, 4};

  // Shader model (determined when parsing "/D" flags)
  const char *in_shader_model{nullptr};

  // Iterate over the command line and find all "/D...=..." settings
  for (char *flag{editable_command}; (flag = strstr(flag, "/D")) != nullptr;
       /* advance inside */) {
    // Make sure this is a command-line flag (basic check for preceding space)
    if (flag > editable_command && flag[-1] && ' ' != flag[-1]) {
      ++flag;
      continue;
    }

    // Name is immediately after "/D"
    char *flag_name{flag + 2};  // 2 = length of "/D"
    // Value will be determined later
    char *flag_value = "";

    if (char *eq{strchr(flag, '=')}) {
      // Value is after '=' sign
      *eq = '\0';

      flag_value = eq + 1;
      flag = flag_value;
    }

    if (char *space{strchr(flag, ' ')}) {
      // Space is designating the end of the flag
      *space = '\0';

      flag = space + 1;
    } else {
      // Reached end of command line.
      flag = "";
    }

    // Shader model extraction.
    if (!strncmp(flag_name, "SHADER_MODEL_", ssize("SHADER_MODEL_") - 1)) {
      in_shader_model = flag_name + ssize("SHADER_MODEL_") - 1;
    }

    // Add the macro definition to the macros array.
    D3D_SHADER_MACRO &m{macros[macros.AddToTail()]};

    // Fill the macro data.
    m.Name = flag_name;
    m.Definition = flag_value;
  }

  // Add a NULL-terminator.
  macros.AddToTail(D3D_SHADER_MACRO{nullptr, nullptr});

  // Convert shader model to lowercase.
  char out_shader_model[16] = {};
  if (in_shader_model) {
    V_strcpy_safe(out_shader_model, in_shader_model);
  }
  Q_strlower(out_shader_model);

  // Determine the file name (at the end of the command line before
  // redirection).
  char const *file_name = "";
  if (const char *redirect_command{
          strstr(in_command, ">" kShaderCompilerOutputName " ")}) {
    size_t offset = redirect_command - in_command;

    editable_command[offset] = '\0';
    file_name = &editable_command[offset];

    while (file_name > editable_command && file_name[-1] &&
           ' ' != file_name[-1]) {
      --file_name;
    }
  }

  // Compile the stuff.
  return FastShaderCompile(file_name, macros.Base(), out_shader_model);
}

std::unique_ptr<se::shader_compile::command_sink::IResponse> TryExecuteCommand(
    const char *command) {
  static int dummy{(Msg("[shadercompile] Using new faster Vitaliy's "
                        "implementation.\n"),
                    1)};

  const bool ok{!strncmp(command,
                         se::shader_compile::fxc_intercept::kFxcCommand,
                         se::shader_compile::fxc_intercept::kFxcCommandSize)};
  if (ok) {
    // Trap "fxc.exe" so that we did not spawn extra process every time
    return se::shader_compile::fxc_intercept::ExecuteCommand(command);
  }

  return nullptr;
}

};  // namespace se::shader_compile::fxc_intercept
