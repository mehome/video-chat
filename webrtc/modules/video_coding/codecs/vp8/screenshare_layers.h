/* Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_VIDEO_CODING_CODECS_VP8_SCREENSHARE_LAYERS_H_
#define MODULES_VIDEO_CODING_CODECS_VP8_SCREENSHARE_LAYERS_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "api/video_codecs/vp8_frame_config.h"
#include "api/video_codecs/vp8_temporal_layers.h"
#include "modules/video_coding/codecs/vp8/include/temporal_layers_checker.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/utility/frame_dropper.h"
#include "rtc_base/rate_statistics.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

struct CodecSpecificInfoVP8;
class Clock;

class ScreenshareLayers final : public Vp8FrameBufferController {
 public:
  static const double kMaxTL0FpsReduction;
  static const double kAcceptableTargetOvershoot;
  static const int kMaxFrameIntervalMs;

  explicit ScreenshareLayers(int num_temporal_layers);
  ~ScreenshareLayers() override;

  size_t StreamCount() const override;

  bool SupportsEncoderFrameDropping(size_t stream_index) const override;

  // Returns the recommended VP8 encode flags needed. May refresh the decoder
  // and/or update the reference buffers.
  Vp8FrameConfig UpdateLayerConfig(size_t stream_index,
                                   uint32_t rtp_timestamp) override;

  // New target bitrate, per temporal layer.
  void OnRatesUpdated(size_t stream_index,
                      const std::vector<uint32_t>& bitrates_bps,
                      int framerate_fps) override;

  // Update the encoder configuration with target bitrates or other parameters.
  // Returns true iff the configuration was actually modified.
  bool UpdateConfiguration(size_t stream_index, Vp8EncoderConfig* cfg) override;

  void OnEncodeDone(size_t stream_index,
                    uint32_t rtp_timestamp,
                    size_t size_bytes,
                    bool is_keyframe,
                    int qp,
                    CodecSpecificInfo* info) override;

  void OnPacketLossRateUpdate(float packet_loss_rate) override;

  void OnRttUpdate(int64_t rtt_ms) override;

 private:
  enum class TemporalLayerState : int { kDrop, kTl0, kTl1, kTl1Sync };

  struct DependencyInfo {
    DependencyInfo() = default;
    DependencyInfo(absl::string_view indication_symbols,
                   Vp8FrameConfig frame_config)
        : decode_target_indications(
              GenericFrameInfo::DecodeTargetInfo(indication_symbols)),
          frame_config(frame_config) {}

    absl::InlinedVector<GenericFrameInfo::DecodeTargetIndication, 10>
        decode_target_indications;
    Vp8FrameConfig frame_config;
  };

  bool TimeToSync(int64_t timestamp) const;
  uint32_t GetCodecTargetBitrateKbps() const;

  int number_of_temporal_layers_;
  int active_layer_;
  int64_t last_timestamp_;
  int64_t last_sync_timestamp_;
  int64_t last_emitted_tl0_timestamp_;
  int64_t last_frame_time_ms_;
  rtc::TimestampWrapAroundHandler time_wrap_handler_;
  int min_qp_;
  int max_qp_;
  uint32_t max_debt_bytes_;

  std::map<uint32_t, DependencyInfo> pending_frame_configs_;

  // Configured max framerate.
  absl::optional<uint32_t> target_framerate_;
  // Incoming framerate from capturer.
  absl::optional<uint32_t> capture_framerate_;

  // Tracks what framerate we actually encode, and drops frames on overshoot.
  RateStatistics encode_framerate_;
  bool bitrate_updated_;

  static constexpr int kMaxNumTemporalLayers = 2;
  struct TemporalLayer {
    TemporalLayer()
        : state(State::kNormal),
          enhanced_max_qp(-1),
          last_qp(-1),
          debt_bytes_(0),
          target_rate_kbps_(0) {}

    enum class State {
      kNormal,
      kDropped,
      kReencoded,
      kQualityBoost,
      kKeyFrame
    } state;

    int enhanced_max_qp;
    int last_qp;
    uint32_t debt_bytes_;
    uint32_t target_rate_kbps_;

    void UpdateDebt(int64_t delta_ms);
  } layers_[kMaxNumTemporalLayers];

  void UpdateHistograms();
  TemplateStructure GetTemplateStructure(int num_layers) const;

  // Data for histogram statistics.
  struct Stats {
    int64_t first_frame_time_ms_ = -1;
    int64_t num_tl0_frames_ = 0;
    int64_t num_tl1_frames_ = 0;
    int64_t num_dropped_frames_ = 0;
    int64_t num_overshoots_ = 0;
    int64_t tl0_qp_sum_ = 0;
    int64_t tl1_qp_sum_ = 0;
    int64_t tl0_target_bitrate_sum_ = 0;
    int64_t tl1_target_bitrate_sum_ = 0;
  } stats_;

  // Optional utility used to verify reference validity.
  std::unique_ptr<TemporalLayersChecker> checker_;
};
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_VP8_SCREENSHARE_LAYERS_H_
