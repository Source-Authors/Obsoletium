// Copyright Valve Corporation, All rights reserved.

#include "ivoicecodec.h"

#include "VoiceEncoder_Speex.h"

#include <cstdio>
#include <system_error>

#include "tier0/memdbgon.h"

// For narrow band.
// Quality | Bit rate | Description
// 0       | 2,150    | Vocoder (mostly for comfort noise)
// 1       | 3,950    | Very noticeable artifacts/noise, good intelligibility
// 2       | 5,950    | Very noticeable artifacts/noise, good intelligibility
// 3-4     | 8,000    | Artifacts/noise sometimes noticeable
// 5-6     | 11,000   | Artifacts usually noticeable only with headphones
// 7-8     | 15,000   | Need good headphones to tell the difference
// 9       | 18,200   | Hard to tell the difference even with good headphones
// 10      | 24,600   | Completely transparent for voice, good quality music

namespace {

[[nodiscard]] int GetSpeexQualityLevel(VoiceCodecQuality quality) {
  switch (quality) {
    case VoiceCodecQuality::Noise:
      return 0;
    case VoiceCodecQuality::Lowest:
      return 2;
    case VoiceCodecQuality::Average:
      return 5;
    case VoiceCodecQuality::Good:
      return 8;
    case VoiceCodecQuality::Perfect:
      return 10;
    default:
      UNREACHABLE();
  }
}

}  // namespace

namespace se::audio::voice_codecs::speex {

constexpr inline int kSamplesPerSecond{8000};  // get 8000 samples/sec
constexpr inline int kSamplesPerFrame{160};    // in 160 samples per frame

// each quality has a different frame size
constexpr inline int
    kEncodedFrameSizes[to_underlying(VoiceCodecQuality::Perfect) + 1]{6, 15, 20,
                                                                      28, 38};

VoiceEncoder_Speex::VoiceEncoder_Speex() {
  encoder_state_ = nullptr;
  decoder_state_ = nullptr;
  quality_ = 0;
}

VoiceEncoder_Speex::~VoiceEncoder_Speex() {
  TermStates();

  Msg("Shut down up Speex codec %u.%u.%u.%u\n", SPEEX_LIB_GET_MAJOR_VERSION,
      SPEEX_LIB_GET_MINOR_VERSION, SPEEX_LIB_GET_MICRO_VERSION,
      SPEEX_LIB_GET_EXTRA_VERSION);
}

bool VoiceEncoder_Speex::Init(VoiceCodecQuality quality, int &rawFrameSize,
                              int &encodedFrameSize) {
  Msg("Start up Speex codec %u.%u.%u.%u\n", SPEEX_LIB_GET_MAJOR_VERSION,
      SPEEX_LIB_GET_MINOR_VERSION, SPEEX_LIB_GET_MICRO_VERSION,
      SPEEX_LIB_GET_EXTRA_VERSION);

  if (!InitStates()) return false;

  quality_ = GetSpeexQualityLevel(quality);

  rawFrameSize = kSamplesPerFrame * BYTES_PER_SAMPLE;
  encodedFrameSize = kEncodedFrameSizes[quality_];

  if (!EncoderCtl(SPEEX_SET_QUALITY, &quality_, "set quality")) {
    return false;
  }

  int postfilter = 1;  // Set the perceptual enhancement on
  if (!DecoderCtl(SPEEX_SET_ENH, &postfilter, "set enchancement on")) {
    return false;
  }

  int sample_rate = kSamplesPerSecond;
  if (!EncoderCtl(SPEEX_SET_SAMPLING_RATE, &sample_rate, "set sample rate") &&
      DecoderCtl(SPEEX_SET_SAMPLING_RATE, &sample_rate, "set sample rate")) {
    return false;
  }

  int frame_size_in_samples;
  if (!EncoderCtl(SPEEX_GET_FRAME_SIZE, &frame_size_in_samples,
                  "get encoder frame size")) {
    return false;
  }

  DevMsg("Speex codec started up (%d Hz sample rate, %d samples / frame)\n",
         sample_rate, frame_size_in_samples);

  return true;
}

void VoiceEncoder_Speex::Release() { delete this; }

void VoiceEncoder_Speex::EncodeFrame(const char *in, char *out) {
  short input[kSamplesPerFrame];
  const short *it = reinterpret_cast<const short *>(in);

  /*Copy the 16 bits values to float so Speex can work on them*/
  for (int i = 0; i < kSamplesPerFrame; i++) {
    input[i] = *it;

    ++it;
  }

  /*Flush all the bits in the struct so we can encode a new frame*/
  speex_bits_reset(&bits_);

  /*Encode the frame*/
  speex_encode_int(encoder_state_, input, &bits_);

#ifdef _DEBUG
  // Ensure our frame size is enough to write all.
  const int out_expected_bytes{speex_bits_nbytes(&bits_)};
  Assert(out_expected_bytes <= kEncodedFrameSizes[quality_]);
#endif

  /*Copy the bits to an array of char that can be written*/
  [[maybe_unused]] int out_written_bytes{
      speex_bits_write(&bits_, out, kEncodedFrameSizes[quality_])};
#ifdef _DEBUG
  if (out_written_bytes != out_expected_bytes) {
    Warning(
        "Speex encoder write failure: real written bytes (%d) != expected ones "
        "(%d)\n",
        out_written_bytes, out_expected_bytes);
  }
#endif
}

void VoiceEncoder_Speex::DecodeFrame(const char *in, char *out) {
  short output[kSamplesPerFrame] = {0};
  short *it = reinterpret_cast<short *>(out);

  /*Copy the data into the bit-stream struct*/
  speex_bits_read_from(&bits_, in, kEncodedFrameSizes[quality_]);

  /*Decode the data*/
  int rc{speex_decode_int(decoder_state_, &bits_, output)};
  if (rc == -2) {
    Warning("Speex decoder detected corrupted stream\n");
  } else if (rc == -1) {
    DevMsg("Speex decoder detected end of stream\n");
  } else {
    Assert(rc == 0);
  }

  /*Copy from float to short (16 bits) for output*/
  for (int i = 0; i < kSamplesPerFrame; i++) {
    *it = output[i];

    ++it;
  }
}

bool VoiceEncoder_Speex::ResetState() {
  return EncoderCtl(SPEEX_RESET_STATE, nullptr, "reset state") &&
         DecoderCtl(SPEEX_RESET_STATE, nullptr, "reset state");
}

bool VoiceEncoder_Speex::InitStates() {
  speex_bits_init(&bits_);
  if (!bits_.chars) {
    // Memory allocation failure.
    Warning("Speex codec startup failure: %s\n",
            std::generic_category().message(ENOMEM).c_str());
    return false;
  }

  encoder_state_ = speex_encoder_init(&speex_nb_mode);
  if (!encoder_state_) {
    // Memory allocation failure.
    Warning("Speex encoder startup failure: %s\n",
            std::generic_category().message(ENOMEM).c_str());
    return false;
  }

  decoder_state_ = speex_decoder_init(&speex_nb_mode);
  if (!decoder_state_) {
    // Memory allocation failure.
    Warning("Speex decoder startup failure: %s\n",
            std::generic_category().message(ENOMEM).c_str());
    return false;
  }

  return true;
}

void VoiceEncoder_Speex::TermStates() {
  if (encoder_state_) {
    speex_encoder_destroy(encoder_state_);
    encoder_state_ = nullptr;
  }

  if (decoder_state_) {
    speex_decoder_destroy(decoder_state_);
    decoder_state_ = nullptr;
  }

  speex_bits_destroy(&bits_);
}

[[nodiscard]] bool VoiceEncoder_Speex::EncoderCtl(int request, void *arg,
                                                  const char *arg_description) {
  int rc{speex_encoder_ctl(encoder_state_, request, arg)};
  if (!rc) return true;

  Warning("Speex encoder %s failure: %d\n", arg_description, rc);
  return false;
}

[[nodiscard]] bool VoiceEncoder_Speex::DecoderCtl(int request, void *arg,
                                                  const char *arg_description) {
  int rc{speex_decoder_ctl(encoder_state_, request, arg)};
  if (!rc) return true;

  Warning("Speex decoder %s failure: %d\n", arg_description, rc);
  return false;
}

}  // namespace se::audio::voice_codecs::speex

extern IVoiceCodec *CreateVoiceCodec_Frame(IFrameEncoder *pEncoder);

namespace {

void *CreateSpeexVoiceCodec() {
  return CreateVoiceCodec_Frame(
      new se::audio::voice_codecs::speex::VoiceEncoder_Speex);
}

}  // namespace

EXPOSE_INTERFACE_FN(CreateSpeexVoiceCodec, IVoiceCodec, "vaudio_speex")