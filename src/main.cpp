#include "platform/web/web_application.hpp"
#include <emscripten.h>
/// Retains the player host across asynchronous browser callbacks.
static helengine::webgpu::WebApplication Application;
/// Starts GPU acquisition and returns to the browser event loop.
int main() { Application.Start(); return 0; }
/// Allows page teardown to retire the engine before the browser releases its GPU context.
extern "C" EMSCRIPTEN_KEEPALIVE void he_shutdown() { Application.Stop(); }
