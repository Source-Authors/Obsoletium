// Copyright Valve Corporation, All rights reserved.
//
// Purpose: PC XAudio2 Version.

#include "audio_pch.h"

#include "snd_dev_xaudio.h"
#include "tier1/utllinkedlist.h"
#include "tier3/tier3.h"
#include "video/ivideoservices.h"

#include "winlite.h"
#include "com_ptr.h"

#include <xaudio2.h>
#include <initguid.h>
#include <ks.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

/**
 * @brief Should spew XAudio2 packets delivery stats?
 */
ConVar snd_xaudio_spew_packets("snd_xaudio_spew_packets", "0", 0,
                               "Spew XAudio2 packet delivery", true, 0, true,
                               1);

extern ConVar snd_mute_losefocus;

namespace {

/**
 * @brief Add prefix for message group to message.
 * @tparam out_size
 * @param out Result message.
 * @param group Group name to prefix.
 * @param message Message.
 * @return Result message.
 */
template <size_t out_size>
const char *PrefixMessageGroup(char (&out)[out_size], const char *group,
                               const char *message) {
  const size_t length{strlen(message)};
  if (length > 1 && message[length - 1] == '\n') {
    V_sprintf_safe(out, "[%.3f][%s] %s", Plat_FloatTime(), group, message);
  } else {
    V_sprintf_safe(out, "%s", message);
  }

  return out;
}

/**
 * @brief Thread-safe debug warn. Can't use tier0/dbg as it writes to console
 * and not thread safe.
 * @param format Format.
 * @param Data.
 */
void DebugWarn(PRINTF_FORMAT_STRING const char *format, ...) {
  char tmp[512];
  tmp[0] = '\0';

  va_list argptr;
  va_start(argptr, format);  //-V2019 //-V2018
  V_vsprintf_safe(tmp, format, argptr);
  va_end(argptr);

  char buffer[512];
  Plat_DebugString(PrefixMessageGroup(buffer, "xaudio2", tmp));
}

/**
 * @brief The outer code mixes in PAINTBUFFER_SIZE (# of samples) chunks (see
 * MIX_PaintChannels), we will never need more than that many samples in a
 * buffer.  This ends up being about 20ms per buffer.
 */
constexpr inline unsigned kXAudio2BufferSamplesCount{8192u};

/**
 * @brief 7.1 means there are a max of 8 channels.
 */
constexpr inline unsigned kMaxXAudio2DeviceChannels{
    std::min(8, XAUDIO2_MAX_AUDIO_CHANNELS)};

/**
 * @brief Buffer return has a latency, so need a decent pool.
 */
constexpr inline unsigned kMaxXAudio2DeviceBuffersCount{
    std::min(kMaxXAudio2DeviceChannels * 6u,
             static_cast<unsigned>(XAUDIO2_MAX_QUEUED_BUFFERS))};

/**
 * @brief Audio device form factor.
 */
enum class AudioDeviceFormFactor {
  /**
   * @brief Headphones or headset.
   */
  HeadphonesOrHeadset = 0,
  /**
   * @brief Mono speaker.
   */
  MonoSpeaker = 1,
  /**
   * @brief Stereo speakers.
   */
  StereoSpeakers = 2,
  /**
   * @brief Quad speakers.
   */
  QuadSpeakers = 4,
  /**
   * @brief 5.1 surround speakers.
   */
  Digital5Dot1Surround = 5,
  /**
   * @brief 7.1 surround speakers.
   */
  Digital7Dot1Surround = 7
};

[[nodiscard]] constexpr short AdjustSampleVolume(int sample,
                                                 int volume_factor) noexcept {
  return CLIP((sample * volume_factor) >> 8);
}

}  // namespace

namespace se::engine::audio::xaudio2 {

/**
 * @brief Callback for XAudio2 device events.
 */
struct AudioXAudio2Callback {
  /**
   * @brief Called when this voice has just finished processing a buffer.  The
   * buffer can now be reused or destroyed.
   * @param buffer_idx Buffer index.
   */
  virtual void OnBufferEnd(size_t buffer_idx) = 0;
};

/**
 * @brief Client notification interface for XAudio2 voice events.
 */
class XAudio2VoiceCallback : public IXAudio2VoiceCallback {
 public:
  explicit XAudio2VoiceCallback(AudioXAudio2Callback *callback)
      : callback_{callback} {
    Assert(!!callback);
  }
  ~XAudio2VoiceCallback() = default;

  STDMETHOD_(void, OnStreamEnd)() override {}

  STDMETHOD_(void, OnVoiceProcessingPassEnd)() override {}

  STDMETHOD_(void, OnVoiceProcessingPassStart)
  (UINT32 samples_required) override {}

  /**
   * @brief Called when this voice has just finished processing a buffer.  The
   * buffer can now be reused or destroyed.
   * @param ctx Context. Buffer index in our case.
   */
  STDMETHOD_(void, OnBufferEnd)(void *ctx) override {
    callback_->OnBufferEnd(reinterpret_cast<size_t>(ctx));
  }

  STDMETHOD_(void, OnBufferStart)(void *) override {}

  STDMETHOD_(void, OnLoopEnd)(void *) override {}

  /**
   * @brief Called in the event of a critical error during voice processing,
   * such as a failing xAPO or an error from the hardware XMA decoder.  The
   * voice may have to be destroyed and re-created to recover from the error.
   * The callback arguments report which buffer was being processed when the
   * error occurred, and its HRESULT code.
   * @param ctx Context. Buffer index in our case.
   * @param hr. HRESULT with error code.
   */
  STDMETHOD_(void, OnVoiceError)(void *ctx, HRESULT hr) override {
    DebugWarn("Voice processing error for buffer %zu: 0x%8x",
              reinterpret_cast<size_t>(ctx), hr);
  }

 private:
  AudioXAudio2Callback *callback_;
};

// Implementation of XAudio2 device.
class CAudioXAudio2 : public CAudioDeviceBase, AudioXAudio2Callback {
 public:
  friend IAudioDevice * ::Audio_CreateXAudioDevice();

  CAudioXAudio2() : xaudio2_voice_callback_{this} {}
  ~CAudioXAudio2() = default;

  bool IsActive() override { return true; }
  bool Init() override;
  void Shutdown() override;

  void Pause() override;
  void UnPause() override;
  int PaintBegin(float mix_ahead_time, int sound_time,
                 int painted_time) override;
  int GetOutputPosition() override;
  void ClearBuffer() override;
  void TransferSamples(int end) override;

  const char *DeviceName() override;
  int DeviceChannels() override { return device_channels_count_; }
  int DeviceSampleBits() override { return device_bits_per_sample_; }
  int DeviceSampleBytes() override { return device_bits_per_sample_ / 8; }
  int DeviceDmaSpeed() override { return device_sample_rate_; }
  int DeviceSampleCount() override { return device_samples_count_; }

  void OnBufferEnd(size_t buffer_idx) override;

 private:
  unsigned TransferStereo(const portable_samplepair_t *front, int painted_time,
                          int end_time, unsigned char *out_buffer);

  unsigned TransferMultichannelSurroundInterleaved(
      const portable_samplepair_t *front, const portable_samplepair_t *rear,
      const portable_samplepair_t *center, int painted_time, int end_time,
      unsigned char *out_buffer);

  // Channels per hardware output buffer (6 for 5.1, 2 for stereo)
  unsigned short device_channels_count_;
  // Bits per sample (16).
  unsigned short device_bits_per_sample_;
  // Count of mono samples in output buffer.
  unsigned device_samples_count_;
  // Samples per second per output buffer.
  unsigned device_sample_rate_;

  unsigned device_clock_divider_;

  se::win::com::com_ptr<IMMDeviceEnumerator> mm_device_enumerator_;
  se::win::com::com_ptr<IMMNotificationClient> mm_notification_client_;

  se::win::com::com_ptr<IXAudio2> xaudio2_engine_;
  IXAudio2MasteringVoice *xaudio2_mastering_voice_;
  IXAudio2SourceVoice *xaudio2_source_voice_;

  XAUDIO2_BUFFER audio_buffers_[kMaxXAudio2DeviceBuffersCount];
  BYTE *all_audio_buffers_;
  // Size of a single hardware output buffer, in bytes.
  unsigned audio_buffer_size_;

  CInterlockedUInt audio_buffer_tail_pos_;
  CInterlockedUInt audio_buffer_head_pos_;

  XAudio2VoiceCallback xaudio2_voice_callback_;
};

Singleton<CAudioXAudio2> g_xaudio2_device;

}  // namespace se::engine::audio::xaudio2

// Create XAudio Device
IAudioDevice *Audio_CreateXAudioDevice() {
  MEM_ALLOC_CREDIT();

  using se::engine::audio::xaudio2::g_xaudio2_device;

  auto *device = g_xaudio2_device.GetInstance();

  if (!device->Init()) {
    g_xaudio2_device.Delete();

    return nullptr;
  }

  return device;
}

namespace se::engine::audio::xaudio2 {

class ScopedPropVariant {
 public:
  ScopedPropVariant() noexcept { PropVariantInit(&prop_); }
  ~ScopedPropVariant() noexcept {
    [[maybe_unused]] HRESULT hr{PropVariantClear(&prop_)};
    Assert(SUCCEEDED(hr));
  }

  [[nodiscard]] VARTYPE type() const noexcept { return prop_.vt; }

  [[nodiscard]] PROPVARIANT *operator&() noexcept { return &prop_; }

  [[nodiscard]] bool as_wide_string(wchar_t *&str) const noexcept {
    if (prop_.vt == VT_LPWSTR) {
      str = prop_.pwszVal;
      return true;
    }

    str = nullptr;
    return false;
  }

  [[nodiscard]] bool as_uint(unsigned &value) const noexcept {
    if (prop_.vt == VT_UI4) {
      value = prop_.uintVal;
      return true;
    }

    value = UINT_MAX;
    return false;
  }

  ScopedPropVariant(ScopedPropVariant &) = delete;
  ScopedPropVariant(ScopedPropVariant &&v) noexcept
      : prop_{std::move(v.prop_)} {
    BitwiseClear(v.prop_);
  }
  ScopedPropVariant &operator=(ScopedPropVariant &) = delete;
  ScopedPropVariant &operator=(ScopedPropVariant &&v) noexcept {
    std::swap(v.prop_, prop_);
    return *this;
  }

 private:
  PROPVARIANT prop_;
};

static bool GetDefaultAudioDeviceFormFactor(
    se::win::com::com_ptr<IMMDeviceEnumerator> &mm_device_enumerator,
    EDataFlow device_data_flow, ERole device_role,
    AudioDeviceFormFactor &form_factor) {
  // Default value.
  form_factor = AudioDeviceFormFactor::StereoSpeakers;

  se::win::com::com_ptr<IMMDevice> default_render_device;
  HRESULT hr{mm_device_enumerator->GetDefaultAudioEndpoint(
      device_data_flow, device_role, &default_render_device)};
  if (FAILED(hr)) {
    DebugWarn("Get default audio render endpoint failed w/e 0x%8x.\n", hr);
    return false;
  }

  se::win::com::com_ptr<IPropertyStore> props;
  hr = default_render_device->OpenPropertyStore(STGM_READ, &props);
  if (FAILED(hr)) {
    DebugWarn("Get default audio render endpoint props failed w/e 0x%8x.\n",
              hr);
    return false;
  }

  ScopedPropVariant device_friendly_name;
  hr = props->GetValue(PKEY_Device_FriendlyName, &device_friendly_name);
  if (SUCCEEDED(hr)) {
    wchar_t *audio_device_name{nullptr};
    if (device_friendly_name.as_wide_string(audio_device_name)) {
      // Print endpoint friendly name and endpoint ID.
      DebugWarn("Using system audio device \"%S\".\n", audio_device_name);
    } else {
      DebugWarn("Using system audio device \"%S\".\n", L"N/A");
    }
  } else {
    DebugWarn(
        "Get default audio render endpoint friendly name failed w/e 0x%8x.\n",
        hr);
    // Continue.
  }

  ScopedPropVariant device_physical_speakers;
  hr = props->GetValue(PKEY_AudioEndpoint_PhysicalSpeakers,
                       &device_physical_speakers);
  if (SUCCEEDED(hr)) {
    unsigned physical_speakers_mask{UINT_MAX};
    // May fail.
    if (device_physical_speakers.as_uint(physical_speakers_mask)) {
      if ((physical_speakers_mask & KSAUDIO_SPEAKER_7POINT1_SURROUND) ==
              KSAUDIO_SPEAKER_7POINT1_SURROUND) {
        form_factor = AudioDeviceFormFactor::Digital7Dot1Surround;
        return true;
      }

      if ((physical_speakers_mask & KSAUDIO_SPEAKER_5POINT1_SURROUND) ==
              KSAUDIO_SPEAKER_5POINT1_SURROUND ||
          // Obsolete, but still.
          (physical_speakers_mask & KSAUDIO_SPEAKER_5POINT1) &
              KSAUDIO_SPEAKER_5POINT1) {
        form_factor = AudioDeviceFormFactor::Digital5Dot1Surround;
        return true;
      }

      if ((physical_speakers_mask & KSAUDIO_SPEAKER_QUAD) ==
          KSAUDIO_SPEAKER_QUAD) {
        form_factor = AudioDeviceFormFactor::QuadSpeakers;
        return true;
      }

      if ((physical_speakers_mask & KSAUDIO_SPEAKER_STEREO) ==
          KSAUDIO_SPEAKER_STEREO) {
        form_factor = AudioDeviceFormFactor::StereoSpeakers;
        return true;
      }

      if ((physical_speakers_mask & KSAUDIO_SPEAKER_MONO) ==
          KSAUDIO_SPEAKER_MONO) {
        form_factor = AudioDeviceFormFactor::MonoSpeaker;
        return true;
      }
    }

    // Fallback to PKEY_AudioEndpoint_FormFactor.
  } else {
    DebugWarn(
        "Get default audio render endpoint physical speakers mask failed w/e "
        "0x%8x.\n",
        hr);

    // Fallback to PKEY_AudioEndpoint_FormFactor.
  }

  ScopedPropVariant device_form_factor;
  hr = props->GetValue(PKEY_AudioEndpoint_FormFactor, &device_form_factor);
  if (SUCCEEDED(hr)) {
    unsigned untyped_factor{UINT_MAX};
    if (device_form_factor.as_uint(untyped_factor)) {
      EndpointFormFactor typed_factor =
          static_cast<EndpointFormFactor>(untyped_factor);

      if (typed_factor == EndpointFormFactor::Headphones ||
          typed_factor == EndpointFormFactor::Headset) {
        form_factor = AudioDeviceFormFactor::HeadphonesOrHeadset;
      }

      if (typed_factor == EndpointFormFactor::Handset ||
          typed_factor == EndpointFormFactor::Speakers) {
        form_factor = AudioDeviceFormFactor::StereoSpeakers;
      }
    }
  } else {
    DebugWarn(
        "Get default audio render endpoint form factor failed w/e 0x%8x.\n",
        hr);
    return false;
  }

  return true;
}

class DefaultAudioDeviceChangedNotificationClient
    : public IMMNotificationClient {
 public:
  // Copy enumerator to ensure it is alive till client destructed.
  static HRESULT Create(
      se::win::com::com_ptr<IMMDeviceEnumerator> mm_device_enumerator,
      EDataFlow device_data_flow, ERole device_role,
      IMMNotificationClient **client) noexcept {
    if (!mm_device_enumerator || !client) return E_POINTER;

    auto *default_client = new DefaultAudioDeviceChangedNotificationClient(
        std::move(mm_device_enumerator), device_data_flow, device_role);
    const HRESULT hr{default_client->registration_hr()};
    if (SUCCEEDED(hr)) {
      *client = default_client;
      return S_OK;
    }

    default_client->Release();
    return hr;
  }

  HRESULT STDMETHODCALLTYPE QueryInterface(
      REFIID riid, _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject) override {
    if (IID_IUnknown == riid) {
      AddRef();
      *ppvObject = static_cast<IUnknown *>(this);
    } else if (__uuidof(IMMNotificationClient) == riid) {
      AddRef();
      *ppvObject = static_cast<IMMNotificationClient *>(this);
    } else {
      *ppvObject = nullptr;
      return E_NOINTERFACE;
    }

    return S_OK;
  }

  ULONG STDMETHODCALLTYPE AddRef() override { return ++ref_counter_; }

  ULONG STDMETHODCALLTYPE Release() override {
    ULONG current_ref_counter = --ref_counter_;
    if (current_ref_counter == 0) {
      delete this;
    }
    return current_ref_counter;
  }

  HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(
      _In_ LPCWSTR pwstrDeviceId, _In_ DWORD dwNewState) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE OnDeviceAdded(_In_ LPCWSTR pwstrDeviceId) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  OnDeviceRemoved(_In_ LPCWSTR pwstrDeviceId) override {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  OnDefaultDeviceChanged(_In_ EDataFlow flow, _In_ ERole role,
                         _In_opt_ LPCWSTR pwstrDefaultDeviceId) override {
    // Note, this code executed from system thread.
    //
    // In implementing the IMMNotificationClient interface, the client should
    // observe these rules to avoid deadlocks and undefined behavior:
    //
    // 1. The methods of the interface must be nonblocking.  The client should
    // never wait on a synchronization object during an event callback.
    // 2. To avoid dead locks, the client should never call
    // IMMDeviceEnumerator::RegisterEndpointNotificationCallback or
    // IMMDeviceEnumerator::UnregisterEndpointNotificationCallback in its
    // implementation of IMMNotificationClient methods.
    // 3. The client should never release the final reference on an MMDevice API
    // object during an event callback.
    if (flow == device_data_flow_ && role == device_role_) {
      DebugWarn("Default system audio '%s' device for '%s' has been %s.\n",
                GetDeviceDataFlowDescription(device_data_flow_),
                GetDeviceRoleDescription(device_role_),
                pwstrDefaultDeviceId != nullptr ? "changed" : "removed");

      if (pwstrDefaultDeviceId) {
        AudioDeviceFormFactor form_factor{
            GetDefaultAudioDeviceFormFactor(mm_device_enumerator_, flow, role,
                                            form_factor)
                ? form_factor
                : AudioDeviceFormFactor::StereoSpeakers};
      } else {
        // TODO: reinit with null audio device as no audio in system.
      }
    }

    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(
      _In_ LPCWSTR pwstrDeviceId, _In_ const PROPERTYKEY key) override {
    return S_OK;
  }

 private:
  se::win::com::com_ptr<IMMDeviceEnumerator> mm_device_enumerator_;

  const EDataFlow device_data_flow_;
  const ERole device_role_;

  std::atomic_ulong ref_counter_;
  HRESULT registration_hr_;

  DefaultAudioDeviceChangedNotificationClient(
      se::win::com::com_ptr<IMMDeviceEnumerator> &&mm_device_enumerator,
      EDataFlow device_data_flow, ERole device_role) noexcept
      : mm_device_enumerator_{std::move(mm_device_enumerator)},
        device_data_flow_{device_data_flow},
        device_role_{device_role} {
    Assert(!!mm_device_enumerator_);

    ++ref_counter_;

    // May fail with out of memory.
    registration_hr_ =
        mm_device_enumerator_->RegisterEndpointNotificationCallback(this);
    Assert(SUCCEEDED(registration_hr_));
  }

  ~DefaultAudioDeviceChangedNotificationClient() noexcept {
    if (SUCCEEDED(registration_hr_)) {
      [[maybe_unused]] HRESULT hr{
          mm_device_enumerator_->UnregisterEndpointNotificationCallback(this)};
      Assert(SUCCEEDED(hr));
    }
  }

  HRESULT registration_hr() const noexcept { return registration_hr_; }

  [[nodiscard]] static const char *GetDeviceDataFlowDescription(
      EDataFlow device_data_flow) {
    switch (device_data_flow) {
      case EDataFlow::eRender:
        return "Render";
      case EDataFlow::eCapture:
        return "Capture";
      case EDataFlow::eAll:
        return "Render & Capture";
      default:
        return "N/A";
    }
  }

  [[nodiscard]] static const char *GetDeviceRoleDescription(ERole device_role) {
    switch (device_role) {
      case ERole::eConsole:
        return "Games, system notification sounds, and voice commands";
      case ERole::eMultimedia:
        return "Music, movies, narration, and live music recording.";
      case ERole::eCommunications:
        return "Voice communications (talking to another person).";
      default:
        return "N/A";
    }
  }
};

// Updates windows settings based on snd_surround cvar changing.  This should
// only happen if the user has changed it via the console or the UI.  Changes
// won't take effect until the engine has restarted.
void OnSndSurroundCvarChanged(IConVar *con_var, const char *old_string,
                              float old_value) {
  // if the old value is -1, we're setting this from the detect routine for the
  // first time no need to reset the device.
  if (old_value == -1) return;

  // restart sound system so it takes effect.
  g_pSoundServices->RestartSoundSystem();
}

void OnSndVarChanged(IConVar *con_var, const char *old_string,
                     float old_value) {
  ConVarRef var(con_var);

  // restart sound system so the change takes effect
  if (var.GetInt() != static_cast<int>(old_value)) {
    g_pSoundServices->RestartSoundSystem();
  }
}

// Initialize XAudio2
bool CAudioXAudio2::Init() {
  snd_surround.InstallChangeCallback(&OnSndSurroundCvarChanged);
  snd_mute_losefocus.InstallChangeCallback(&OnSndVarChanged);

  // Audio rendering stream.  Audio data flows from the application to the audio
  // endpoint device, which renders the stream.
  constexpr EDataFlow device_data_flow{EDataFlow::eRender};
  // Audio device for games, system notification sounds, and voice commands.
  constexpr ERole device_role{ERole::eConsole};

  HRESULT hr{mm_device_enumerator_.CreateInstance(__uuidof(MMDeviceEnumerator),
                                                  nullptr, CLSCTX_ALL)};
  if (FAILED(hr)) {
    Warning("XAudio2: Create media devices enumerator failed w/e 0x%8x.\n", hr);
    Warning(
        "XAudio2: Unable to get default system audio device, assume stereo "
        "speakers.\n");

    snd_surround.SetValue(to_underlying(AudioDeviceFormFactor::StereoSpeakers));
  } else {
    hr = DefaultAudioDeviceChangedNotificationClient::Create(
        mm_device_enumerator_, device_data_flow, device_role,
        &mm_notification_client_);
    if (FAILED(hr)) {
      Warning(
          "XAudio2: Create default system audio device change watcher failed "
          "w/e 0x%8x.\n",
          hr);
      Warning(
          "XAudio2: Changing default system audio device will not change game "
          "sound output.\n");
    }

    AudioDeviceFormFactor form_factor{
        GetDefaultAudioDeviceFormFactor(mm_device_enumerator_, device_data_flow,
                                        device_role, form_factor)
            ? form_factor
            : AudioDeviceFormFactor::StereoSpeakers};
    snd_surround.SetValue(to_underlying(form_factor));
  }

  m_bHeadphone = false;
  m_bSurround = false;
  m_bSurroundCenter = false;

  switch (snd_surround.GetInt()) {
    case to_underlying(AudioDeviceFormFactor::HeadphonesOrHeadset):
      m_bHeadphone = true;
      device_channels_count_ = 2;
      break;

    default:
    // TODO: Add mono speaker support.
    case to_underlying(AudioDeviceFormFactor::MonoSpeaker):
    case to_underlying(AudioDeviceFormFactor::StereoSpeakers):
      device_channels_count_ = 2;
      break;

    case to_underlying(AudioDeviceFormFactor::QuadSpeakers):
      m_bSurround = true;
      device_channels_count_ = 4;
      break;

    case to_underlying(AudioDeviceFormFactor::Digital5Dot1Surround):
      m_bSurround = true;
      m_bSurroundCenter = true;
      device_channels_count_ = 6;
      break;

    case to_underlying(AudioDeviceFormFactor::Digital7Dot1Surround):
      m_bSurround = true;
      m_bSurroundCenter = true;
      device_channels_count_ = 8;
      break;
  }

  // Initialize the XAudio2 Engine.
  //
  // If you specify XAUDIO2_ANY_PROCESSOR, the system will use all of the
  // device's processors and, as noted above, create a worker thread for
  // each processor.
  //
  // Implementations targeting Games and WIN10_19H1 and later, should use
  // XAUDIO2_USE_DEFAULT_PROCESSOR instead to let XAudio2 select the appropriate
  // default processor for the hardware platform.
  hr = XAudio2Create(&xaudio2_engine_, 0, XAUDIO2_USE_DEFAULT_PROCESSOR);
  if (FAILED(hr)) {
    Warning("XAudio2: Creating engine failed w/e 0x%8x.\n", hr);
    return false;
  }

  // Create the mastering voice, this will upsample to the devices target
  // hardware output rate.
  hr = xaudio2_engine_->CreateMasteringVoice(&xaudio2_mastering_voice_);
  if (FAILED(hr)) {
    Warning("XAudio2: Creating mastering voice failed w/e 0x%8x.\n", hr);
    return false;
  }

  device_bits_per_sample_ = 16;
  device_sample_rate_ = SOUND_DMA_SPEED;

  // 16 bit PCM
  WAVEFORMATEX waveFormatEx = {};
  waveFormatEx.wFormatTag = WAVE_FORMAT_PCM;
  waveFormatEx.nChannels = device_channels_count_;
  // Sample rate.
  waveFormatEx.nSamplesPerSec = device_sample_rate_;
  waveFormatEx.wBitsPerSample = device_bits_per_sample_;
  // Block size.
  waveFormatEx.nBlockAlign =
      (waveFormatEx.nChannels * waveFormatEx.wBitsPerSample) / 8;
  waveFormatEx.nAvgBytesPerSec =
      waveFormatEx.nSamplesPerSec * waveFormatEx.nBlockAlign;
  waveFormatEx.cbSize = 0;

  hr = xaudio2_engine_->CreateSourceVoice(
      &xaudio2_source_voice_, &waveFormatEx, 0, XAUDIO2_DEFAULT_FREQ_RATIO,
      &xaudio2_voice_callback_, nullptr, nullptr);
  if (FAILED(hr)) {
    Warning("XAudio2: Creating source voice failed w/e 0x%8x.\n", hr);
    return false;
  }

  float channel_volumes[kMaxXAudio2DeviceChannels];
  for (size_t i = 0; i < kMaxXAudio2DeviceChannels; i++) {
    channel_volumes[i] = !m_bSurround && i >= 2 ? 0 : 1.0f;
  }

  hr = xaudio2_source_voice_->SetChannelVolumes(device_channels_count_,
                                                channel_volumes);
  if (FAILED(hr)) {
    Warning("XAudio2: Setting %hu channel volumes failed w/e 0x%8x.\n",
            device_channels_count_, hr);
    return false;
  }

  audio_buffer_size_ = kXAudio2BufferSamplesCount *
                       (device_bits_per_sample_ / 8) * device_channels_count_;
  all_audio_buffers_ =
      new BYTE[kMaxXAudio2DeviceBuffersCount * audio_buffer_size_];

  ClearBuffer();

  V_memset(audio_buffers_, 0, sizeof(audio_buffers_));
  for (size_t i = 0; i < kMaxXAudio2DeviceBuffersCount; i++) {
    audio_buffers_[i].pAudioData = all_audio_buffers_ + i * audio_buffer_size_;
    audio_buffers_[i].pContext = reinterpret_cast<void *>(i);
  }

  audio_buffer_head_pos_ = 0;
  audio_buffer_tail_pos_ = 0;

  // Number of mono samples output buffer may hold
  device_samples_count_ = kMaxXAudio2DeviceBuffersCount *
                          (audio_buffer_size_ / DeviceSampleBytes());

  // NOTE: This really shouldn't be tied to the # of bufferable samples.  This
  // just needs to be large enough so that it doesn't fake out the sampling in
  // GetSoundTime().  Basically GetSoundTime() assumes a cyclical time stamp and
  // finds wraparound cases but that means it needs to get called much more
  // often than once per cycle.  So this number should be much larger than the
  // framerate in terms of output time.
  device_clock_divider_ = device_samples_count_ / DeviceChannels();

  hr = xaudio2_source_voice_->Start(0);
  if (FAILED(hr)) {
    Warning("XAudio2: Unable to start engine w/e 0x%8x.\n", hr);
    return false;
  }

  DevMsg("XAudio2 device initialized:\n");
  DevMsg("%s |  %d channel(s) |  %d bits/sample |  %d Hz\n", DeviceName(),
         DeviceChannels(), DeviceSampleBits(), DeviceDmaSpeed());

  // Tell video subsytem to use our device as out one.
  if (g_pVideo) {
    g_pVideo->SoundDeviceCommand(VideoSoundDeviceOperation::HOOK_X_AUDIO,
                                 xaudio2_engine_.GetInterfacePtr());
  }

  // success
  return true;
}

// Shutdown XAudio2
void CAudioXAudio2::Shutdown() {
  // Tell video subsytem to not use our device as out one.
  if (g_pVideo) {
    g_pVideo->SoundDeviceCommand(VideoSoundDeviceOperation::HOOK_X_AUDIO,
                                 nullptr);
  }

  if (xaudio2_source_voice_) {
    xaudio2_source_voice_->Stop(0);
    // Slow, blocking.
    xaudio2_source_voice_->DestroyVoice();
    xaudio2_source_voice_ = nullptr;

    delete[] all_audio_buffers_;
  }

  if (xaudio2_mastering_voice_) {
    // Slow, blocking.
    xaudio2_mastering_voice_->DestroyVoice();
    xaudio2_mastering_voice_ = nullptr;
  }

  if (xaudio2_engine_) xaudio2_engine_.Release();
}

// XAudio2 has completed a packet.  Assuming they are sequential.
void CAudioXAudio2::OnBufferEnd(size_t buffer_idx) {
  // packet completion expected to be sequential
  Assert(buffer_idx == static_cast<size_t>(audio_buffer_tail_pos_ %
                                           kMaxXAudio2DeviceBuffersCount));

  ++audio_buffer_tail_pos_;

  if (snd_xaudio_spew_packets.GetBool()) {
    if (audio_buffer_tail_pos_ == audio_buffer_head_pos_) {
      Warning("XAudio2: Starved.\n");
    } else {
      Msg("XAudio2: Packet Callback, Submit: %2u, Free: %2u.\n",
          audio_buffer_head_pos_ - audio_buffer_tail_pos_,
          kMaxXAudio2DeviceBuffersCount -
              (audio_buffer_head_pos_ - audio_buffer_tail_pos_));
    }
  }

  if (audio_buffer_tail_pos_ == audio_buffer_head_pos_) {
    // Very bad, out of packets, XAudio2 is starving.  Mix thread didn't keep up
    // with audio clock and submit packets submit a silent buffer to keep stream
    // playing and audio clock running.
    unsigned head{audio_buffer_head_pos_++};
    XAUDIO2_BUFFER &buffer{
        audio_buffers_[head % kMaxXAudio2DeviceBuffersCount]};

    V_memset(const_cast<byte *>(buffer.pAudioData), 0, audio_buffer_size_);

    buffer.AudioBytes = audio_buffer_size_;

    const HRESULT hr{xaudio2_source_voice_->SubmitSourceBuffer(&buffer)};
    if (FAILED(hr))
      Warning("XAudio2: Starved. Submitting silent buffer failed w/e 0x%8x.\n",
              hr);
  }
}

// Return the "write" cursor.  Used to clock the audio mixing.  The actual hw
// write cursor and the number of samples it fetches is unknown.
int CAudioXAudio2::GetOutputPosition() {
  XAUDIO2_VOICE_STATE state = {};
  state.SamplesPlayed = 0;

  xaudio2_source_voice_->GetState(&state);

  return state.SamplesPlayed % device_clock_divider_;
}

// Pause playback
void CAudioXAudio2::Pause() {
  if (xaudio2_source_voice_) {
    const HRESULT hr{xaudio2_source_voice_->Stop(0)};
    if (FAILED(hr)) {
      Warning("XAudio2: Stopping source voice failed w/e 0x%8x.\n", hr);
    }
  }
}

// Resume playback
void CAudioXAudio2::UnPause() {
  if (xaudio2_source_voice_) {
    const HRESULT hr{xaudio2_source_voice_->Start(0)};
    if (FAILED(hr)) {
      Warning("XAudio2: Starting source voice failed w/e 0x%8x.\n", hr);
    }
  }
}

// Calculate the paint position.
int CAudioXAudio2::PaintBegin(float mix_ahead_time, int sound_time,
                              int painted_time) {
  // soundtime = total full samples that have been played out to hardware at
  // dmaspeed paintedtime = total full samples that have been mixed at speed

  // endtime = target for full samples in mixahead buffer at speed
  const int mixaheadtime{static_cast<int>(mix_ahead_time * DeviceDmaSpeed())};

  int end_time{sound_time + mixaheadtime};
  if (end_time <= painted_time) return end_time;

  const int samples_per_channel{DeviceSampleCount() / DeviceChannels()};

  if (end_time - sound_time > samples_per_channel) {
    end_time = sound_time + samples_per_channel;
  }

  if ((end_time - painted_time) & 0x03) {
    // The difference between endtime and painted time should align on
    // boundaries of 4 samples.  This is important when upsampling from 11khz ->
    // 44khz.
    end_time -= (end_time - painted_time) & 0x03;
  }

  return end_time;
}

// Fill the output buffers with silence.
void CAudioXAudio2::ClearBuffer() {
  V_memset(all_audio_buffers_, 0,
           kMaxXAudio2DeviceBuffersCount * audio_buffer_size_);
}

// Fill the output buffer with L/R samples.
unsigned CAudioXAudio2::TransferStereo(
    const portable_samplepair_t *front_buffer, int painted_time, int end_time,
    unsigned char *out_buffer) {
  const int volume_factor{static_cast<int>(S_GetMasterVolume() * 256)};

  // get size of output buffer in full samples (LR pairs)
  // number of sequential sample pairs that can be written
  unsigned linear_count{static_cast<unsigned>(DeviceSampleCount()) >> 1};

  // clamp output count to requested number of samples
  if (linear_count > static_cast<unsigned>(end_time - painted_time)) {
    linear_count = end_time - painted_time;
  }

  // linear_count is now number of mono 16 bit samples (L and R) to xfer.
  linear_count <<= 1u;

  const portable_samplepair_t *front{front_buffer};
  short *out{reinterpret_cast<short *>(out_buffer)};

  // transfer mono 16bit samples multiplying each sample by volume.
  for (unsigned i = 0; i < linear_count; i += 2) {
    // L Channel
    *out++ = AdjustSampleVolume(front->left, volume_factor);

    // R Channel
    *out++ = AdjustSampleVolume(front->right, volume_factor);

    ++front;
  }

  return linear_count * DeviceSampleBytes();
}

// Fill the output buffer with interleaved surround samples.
// TODO: Add 7.1 support.
unsigned CAudioXAudio2::TransferMultichannelSurroundInterleaved(
    const portable_samplepair_t *front_buffer,
    const portable_samplepair_t *rear_buffer,
    const portable_samplepair_t *center_buffer, int painted_time, int end_time,
    unsigned char *out_buffer) {
  const int volume_factor{static_cast<int>(S_GetMasterVolume() * 256)};

  // number of mono samples per channel
  // number of sequential samples that can be wrriten
  unsigned linear_count{audio_buffer_size_ /
                        (DeviceSampleBytes() * DeviceChannels())};

  // clamp output count to requested number of samples
  if (linear_count > static_cast<unsigned>(end_time - painted_time)) {
    linear_count = end_time - painted_time;
  }

  const portable_samplepair_t *front{front_buffer};
  const portable_samplepair_t *rear{rear_buffer};
  const portable_samplepair_t *center{center_buffer};

  short *out{reinterpret_cast<short *>(out_buffer)};

  for (unsigned i = 0; i < linear_count; ++i) {
    // FL
    *out++ = AdjustSampleVolume(front->left, volume_factor);

    // FR
    *out++ = AdjustSampleVolume(front->right, volume_factor);

    ++front;

    // Quad speakers have no center.
    if (device_channels_count_ > 4) {
      // Center
      *out++ = AdjustSampleVolume(center->left, volume_factor);

      // Let the hardware mix the sub from the main channels since
      // we don't have any sub-specific sounds, or direct sub-addressing
      *out++ = 0;

      ++center;
    }

    // RL
    *out++ = AdjustSampleVolume(rear->left, volume_factor);

    // RR
    *out++ = AdjustSampleVolume(rear->right, volume_factor);

    ++rear;
  }

  return linear_count * DeviceSampleBytes() * DeviceChannels();
}

// Transfer up to a full paintbuffer (PAINTBUFFER_SIZE) of samples out to the
// XAudio2 buffer(s).
void CAudioXAudio2::TransferSamples(int end_time) {
  if (audio_buffer_head_pos_ - audio_buffer_tail_pos_ >=
      kMaxXAudio2DeviceBuffersCount) {
    DebugWarn("No free audio buffers! All %u buffers taken.\n",
              kMaxXAudio2DeviceBuffersCount);

    return;
  }

  const int samples_count{end_time - g_paintedtime};
  if (samples_count > kXAudio2BufferSamplesCount) {
    DebugWarn("Overflowed mix buffer! All %u samples taken.\n",
              kXAudio2BufferSamplesCount);

    end_time = g_paintedtime + kXAudio2BufferSamplesCount;
  }

  const unsigned buffer_idx{audio_buffer_head_pos_++};
  XAUDIO2_BUFFER &buffer{
      audio_buffers_[buffer_idx % kMaxXAudio2DeviceBuffersCount]};

  if (!m_bSurround) {
    buffer.AudioBytes =
        TransferStereo(PAINTBUFFER, g_paintedtime, end_time,
                       const_cast<unsigned char *>(buffer.pAudioData));
  } else {
    buffer.AudioBytes = TransferMultichannelSurroundInterleaved(
        PAINTBUFFER, REARPAINTBUFFER, CENTERPAINTBUFFER, g_paintedtime,
        end_time, const_cast<unsigned char *>(buffer.pAudioData));
  }

  const HRESULT hr{xaudio2_source_voice_->SubmitSourceBuffer(&buffer)};
  if (FAILED(hr)) {
    DebugWarn("Submitting samples buffer failed w/e 0x%8x.\n", hr);
  }
}

// Get our device name
const char *CAudioXAudio2::DeviceName() {
  if (m_bSurround) {
    if (device_channels_count_ == 4) {
      return "XAudio2 4 Channel Surround";
    }

    if (device_channels_count_ == 6) {
      return "XAudio2 5.1 Channel Surround";
    }

    if (device_channels_count_ == 8) {
      return "XAudio2 7.1 Channel Surround";
    }
  }

  if (m_bHeadphone) {
    return "XAudio2 Headphones";
  }

  return "XAudio2 Speakers";
}

}  // namespace se::engine::audio::xaudio2