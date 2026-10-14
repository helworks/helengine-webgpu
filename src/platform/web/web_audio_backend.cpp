#include "web_audio_backend.hpp"
#include "AudioAsset.hpp"
#include "AudioPlaybackRequest.hpp"
#include <emscripten.h>
#include <cmath>
#include <stdexcept>

EM_JS(int, he_audio_play, (const uint8_t* bytes, int length, int channels, int sampleRate, const char* bus, float gain, int loop, float offset), {
    return window['helengineAudio']['play'](new Uint8Array(HEAPU8.buffer, bytes, length), channels, sampleRate, UTF8ToString(bus), gain, loop, offset);
});
EM_JS(void, he_audio_stop, (int voice), { window['helengineAudio']['stop'](voice); });
EM_JS(int, he_audio_playing, (int voice), { return window['helengineAudio']['isPlaying'](voice); });
EM_JS(void, he_audio_gain, (const char* bus, float gain), { window['helengineAudio']['setBusGain'](UTF8ToString(bus), gain); });
EM_JS(void, he_audio_pause, (const char* bus, int paused), { window['helengineAudio']['setBusPaused'](UTF8ToString(bus), Boolean(paused)); });
namespace helengine::webgpu {
int32_t WebAudioBackend::Play(AudioAsset* asset, AudioPlaybackRequest* request) {
    if (!asset || !request || !asset->EncodedBytes || !asset->EncodedBytes->Length) throw std::invalid_argument("Audio requires an asset, request and PCM bytes");
    if (asset->EncodingFamilyId != "pcm-buffered" && asset->EncodingFamilyId != "pcm" && asset->EncodingFamilyId != "pcm16" && asset->EncodingFamilyId != "pcm-s16le") throw std::runtime_error("Audio asset '" + asset->get_Id() + "' has unsupported Web Audio encoding: " + asset->EncodingFamilyId);
    if (asset->Channels < 1 || asset->Channels > 2 || asset->SampleRate <= 0 || asset->EncodedBytes->Length % (asset->Channels * 2)) throw std::invalid_argument("Audio requires complete mono/stereo PCM16 frames");
    if (request->BusId.empty() || !std::isfinite(request->Gain) || request->Gain < 0 || !std::isfinite(request->StartOffsetSeconds) || request->StartOffsetSeconds < 0) throw std::invalid_argument("Invalid audio bus, gain or start offset");
    return he_audio_play(asset->EncodedBytes->Data, asset->EncodedBytes->Length, asset->Channels, asset->SampleRate, request->BusId.c_str(), request->Gain, request->Loop, request->StartOffsetSeconds);
}
void WebAudioBackend::Stop(int32_t voice) { he_audio_stop(voice); }
bool WebAudioBackend::IsPlaying(int32_t voice) { return he_audio_playing(voice); }
void WebAudioBackend::SetBusGain(std::string bus, float gain) {
    if (bus.empty() || !std::isfinite(gain) || gain < 0) throw std::invalid_argument("Invalid audio bus gain");
    he_audio_gain(bus.c_str(), gain);
}
void WebAudioBackend::SetBusPaused(std::string bus, bool paused) {
    if (bus.empty()) throw std::invalid_argument("Audio bus id is required");
    he_audio_pause(bus.c_str(), paused);
}
}
