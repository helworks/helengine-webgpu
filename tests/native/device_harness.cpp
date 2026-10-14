#include "platform/webgpu/webgpu_device_context.hpp"
#include <emscripten.h>
using helengine::webgpu::WebGpuDeviceContext;
static WebGpuDeviceContext Context;
static int State = 0;
static std::string Error;
extern "C" {
EMSCRIPTEN_KEEPALIVE void he_test_start_gpu() { State = 1; Context.Initialize("#helengine-canvas", [] { State = 2; }, [](const std::string& message) { Error = message; State = -1; }); }
EMSCRIPTEN_KEEPALIVE int he_test_gpu_state() { return State; }
EMSCRIPTEN_KEEPALIVE const char* he_test_gpu_error() { return Error.c_str(); }
EMSCRIPTEN_KEEPALIVE int he_test_resize(int width, int height) { return Context.Resize(width, height) ? 1 : 0; }
EMSCRIPTEN_KEEPALIVE void he_test_stop_gpu() { Context.Shutdown(); State = 0; }
EMSCRIPTEN_KEEPALIVE void he_test_destroy_device() { wgpuDeviceDestroy(Context.GetDevice()); }
}
