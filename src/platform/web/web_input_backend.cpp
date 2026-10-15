#include "web_input_backend.hpp"
#include "Keys.hpp"
#include <emscripten.h>
#include <algorithm>
#include <cmath>
#include <cstring>
namespace helengine::webgpu {
WebInputBackend::WebInputBackend() {
    for(auto& frame:GamepadFrames) frame=new Array<InputGamepadState>(4);
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, true, Key);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, true, Key);
    emscripten_set_blur_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, true, Blur);
    emscripten_set_mousedown_callback("#helengine-canvas", this, true, Pointer);
    emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, true, Pointer);
    emscripten_set_mousemove_callback("#helengine-canvas", this, true, Pointer);
    emscripten_set_wheel_callback("#helengine-canvas", this, true, Wheel);
}
WebInputBackend::~WebInputBackend() {
    for(auto* frame:GamepadFrames) delete frame;
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, nullptr);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, nullptr);
    emscripten_set_blur_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, nullptr);
    emscripten_set_mousedown_callback("#helengine-canvas", nullptr, true, nullptr);
    emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, nullptr);
    emscripten_set_mousemove_callback("#helengine-canvas", nullptr, true, nullptr);
    emscripten_set_wheel_callback("#helengine-canvas", nullptr, true, nullptr);
}
InputFrameState WebInputBackend::CaptureFrame() {
    InputFrameState frame; frame.Keyboard = Keyboard; frame.Mouse = Mouse;
    auto* pads=GamepadFrames[GamepadFrame]; GamepadFrame^=1;
    frame.Gamepads=pads; frame.GamepadCount=0;
    for(int index=0;index<4;++index) (*pads)[index]=InputGamepadState();
    bool active=Background || EM_ASM_INT({return document.hasFocus() && document.activeElement===document.getElementById('helengine-canvas') && !document.hidden;});
    if(active && emscripten_sample_gamepad_data()==EMSCRIPTEN_RESULT_SUCCESS) {
        int count=std::min(emscripten_get_num_gamepads(),4);
        constexpr int mapping[17]={0,1,2,3,4,5,8,9,15,14,6,7,10,11,12,13,16};
        for(int index=0;index<count;++index) {
            EmscriptenGamepadEvent source{};
            if(emscripten_get_gamepad_status(index,&source)!=EMSCRIPTEN_RESULT_SUCCESS || !source.connected || std::strcmp(source.mapping,"standard")!=0) continue;
            auto& pad=(*pads)[index]; pad.Connected=true; frame.GamepadCount=index+1;
            for(int button=0;button<std::min(source.numButtons,17);++button) if(source.digitalButton[button]) pad.Buttons|=uint64_t(1)<<mapping[button];
            auto axis=[&](int offset) { double value=offset<source.numAxes?source.axis[offset]:0; return static_cast<int16_t>(std::isfinite(value)?std::clamp(value,-1.0,1.0)*32767:0); };
            auto trigger=[&](int offset) { double value=offset<source.numButtons?source.analogButton[offset]:0; return static_cast<int16_t>(std::isfinite(value)?std::clamp(value,0.0,1.0)*32767:0); };
            pad.LeftStickX=axis(0);pad.LeftStickY=axis(1);pad.RightStickX=axis(2);pad.RightStickY=axis(3);pad.LeftTrigger=trigger(6);pad.RightTrigger=trigger(7);
        }
    }
    return frame;
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
