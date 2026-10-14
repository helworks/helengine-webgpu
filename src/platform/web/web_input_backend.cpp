#include "web_input_backend.hpp"
#include "Keys.hpp"
#include <emscripten.h>
namespace helengine::webgpu {
WebInputBackend::WebInputBackend() {
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, true, Key);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, true, Key);
    emscripten_set_blur_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, true, Blur);
    emscripten_set_mousedown_callback("#helengine-canvas", this, true, Pointer);
    emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, true, Pointer);
    emscripten_set_mousemove_callback("#helengine-canvas", this, true, Pointer);
    emscripten_set_wheel_callback("#helengine-canvas", this, true, Wheel);
}
WebInputBackend::~WebInputBackend() {
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, nullptr);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, nullptr);
    emscripten_set_blur_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, nullptr);
    emscripten_set_mousedown_callback("#helengine-canvas", nullptr, true, nullptr);
    emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, nullptr);
    emscripten_set_mousemove_callback("#helengine-canvas", nullptr, true, nullptr);
    emscripten_set_wheel_callback("#helengine-canvas", nullptr, true, nullptr);
}
InputFrameState WebInputBackend::CaptureFrame() {
    InputFrameState frame; frame.Keyboard = Keyboard; frame.Mouse = Mouse; return frame;
}
void WebInputBackend::ReleaseAll() {
    Keyboard.InternalClearAllKeys(); Mouse.set_LeftButton(ButtonState::Released); Mouse.set_RightButton(ButtonState::Released); Mouse.set_MiddleButton(ButtonState::Released);
}
EM_BOOL WebInputBackend::Key(int type, const EmscriptenKeyboardEvent* event, void* userdata) {
    auto* self = static_cast<WebInputBackend*>(userdata);
    if (!self->Background && !EM_ASM_INT({ return document.activeElement === document.getElementById('helengine-canvas'); })) return false;
    if (event->keyCode >= 256) return false;
    unsigned code = event->keyCode;
    bool right = event->location == 2;
    if (code == 16) code = right ? 161 : 160;
    else if (code == 17) code = right ? 163 : 162;
    else if (code == 18) code = right ? 165 : 164;
    auto key = static_cast<Keys>(code);
    if (type == EMSCRIPTEN_EVENT_KEYDOWN) self->Keyboard.InternalSetKey(key); else self->Keyboard.InternalClearKey(key);
    return true;
}
EM_BOOL WebInputBackend::Pointer(int, const EmscriptenMouseEvent* event, void* userdata) {
    auto* self = static_cast<WebInputBackend*>(userdata);
    int width, height; double cssWidth, cssHeight;
    emscripten_get_canvas_element_size("#helengine-canvas", &width, &height);
    emscripten_get_element_css_size("#helengine-canvas", &cssWidth, &cssHeight);
    if (cssWidth > 0 && cssHeight > 0) {
        double left = EM_ASM_DOUBLE({ return document.getElementById('helengine-canvas').getBoundingClientRect().left; });
        double top = EM_ASM_DOUBLE({ return document.getElementById('helengine-canvas').getBoundingClientRect().top; });
        self->Mouse.set_X(static_cast<int>((event->clientX - left) * width / cssWidth));
        self->Mouse.set_Y(static_cast<int>((event->clientY - top) * height / cssHeight));
    }
    self->Mouse.set_LeftButton(event->buttons & 1 ? ButtonState::Pressed : ButtonState::Released);
    self->Mouse.set_RightButton(event->buttons & 2 ? ButtonState::Pressed : ButtonState::Released);
    self->Mouse.set_MiddleButton(event->buttons & 4 ? ButtonState::Pressed : ButtonState::Released);
    return false;
}
EM_BOOL WebInputBackend::Wheel(int, const EmscriptenWheelEvent* event, void* userdata) {
    auto* self = static_cast<WebInputBackend*>(userdata);
    self->Mouse.set_ScrollWheelValue(self->Mouse.get_ScrollWheelValue() - static_cast<int>(event->deltaY));
    return true;
}
EM_BOOL WebInputBackend::Blur(int, const EmscriptenFocusEvent*, void* userdata) {
    auto* self = static_cast<WebInputBackend*>(userdata);
    if (!self->Background) self->ReleaseAll();
    return false;
}
}
