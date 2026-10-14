#include "platform/web/web_audio_backend.hpp"
#include "platform/web/web_input_backend.hpp"
#include "AudioAsset.hpp"
#include "AudioPlaybackRequest.hpp"
#include "Keys.hpp"
#include <emscripten.h>
#include <memory>
#include <stdexcept>

static helengine::webgpu::WebAudioBackend Audio;
static std::unique_ptr<helengine::webgpu::WebInputBackend> Input;
static std::string AudioError;
extern "C" {
/// Exercises the real native bridge with the PCM family emitted by the shared cooker.
EMSCRIPTEN_KEEPALIVE int he_test_prepared_pcm() {
    AudioAsset asset; AudioPlaybackRequest request; Array<uint8_t> pcm(1600);
    for (int i = 0; i < pcm.Length; ++i) pcm[i] = 0;
    pcm[0] = 0; pcm[1] = 32;
    asset.EncodingFamilyId = "pcm-buffered"; asset.Channels = 1; asset.SampleRate = 8000; asset.EncodedBytes = &pcm;
    request.BusId = "effects"; request.Gain = 1; request.Loop = false;
    try { AudioError.clear(); return Audio.Play(&asset, &request); }
    catch (const std::exception& error) { AudioError = error.what(); return -1; }
}
EMSCRIPTEN_KEEPALIVE const char* he_test_audio_error() { return AudioError.c_str(); }
EMSCRIPTEN_KEEPALIVE void he_test_start_input() { Input = std::make_unique<helengine::webgpu::WebInputBackend>(); }
EMSCRIPTEN_KEEPALIVE int he_test_mouse_x() { return Input->CaptureFrame().Mouse.get_X(); }
EMSCRIPTEN_KEEPALIVE int he_test_mouse_y() { return Input->CaptureFrame().Mouse.get_Y(); }
EMSCRIPTEN_KEEPALIVE int he_test_key_down(int key) { return Input->CaptureFrame().Keyboard.IsKeyDown(static_cast<Keys>(key)); }
}
