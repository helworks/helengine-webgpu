#pragma once
#include "IAudioBackend.hpp"
namespace helengine::webgpu {
/// Bridges prepared PCM16 assets to browser-owned Web Audio voices and buses.
class WebAudioBackend : public IAudioBackend {
public:
    /// Starts a validated mono/stereo resident PCM16 asset.
    int32_t Play(AudioAsset* asset, AudioPlaybackRequest* request) override;
    /// Stops a voice idempotently.
    void Stop(int32_t voiceId) override;
    /// Sets one mix bus's linear gain.
    void SetBusGain(HeCppString busId, float gain) override;
    /// Pauses/resumes voices while preserving their playback positions.
    void SetBusPaused(HeCppString busId, bool paused) override;
    /// Reports active or paused voice residency.
    bool IsPlaying(int32_t voiceId) override;
    /// Keeps the callback-driven Web Audio backend compatible with the core frame contract.
    void Update() override {}
};
}
