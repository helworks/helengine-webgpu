#pragma once
#include "IInputBackend.hpp"
#include "KeyboardState.hpp"
#include "MouseState.hpp"
#include <emscripten/html5.h>
namespace helengine::webgpu {
/// Captures browser input into the engine's portable frame state.
class WebInputBackend : public IInputBackend {
public:
    /// Subscribes to canvas mouse and window keyboard/focus events.
    WebInputBackend();
    /// Unsubscribes native event callbacks before destroying backend state.
    ~WebInputBackend() override;
    /// Reports the configured background input policy.
    bool get_ReceiveInputInBackground() override { return Background; }
    /// Changes whether focus loss releases browser input state.
    void set_ReceiveInputInBackground(bool value) override { Background = value; }
    /// Returns a value snapshot; key transitions remain owned by the shared engine InputSystem.
    InputFrameState CaptureFrame() override;
    /// Releases all pressed controls when the browser suspends capture.
    void ReleaseAll();
private:
    /// Holds currently pressed keys as the engine's own packed state.
    KeyboardState Keyboard;
    /// Holds mouse buttons, scaled position and cumulative wheel values.
    MouseState Mouse;
    /// Alternates borrowed snapshots so the engine retains previous-frame button edges.
    Array<InputGamepadState>* GamepadFrames[2] = {};
    unsigned GamepadFrame = 0;
    /// Stores the background capture policy.
    bool Background = false;
    /// Applies one key event without synthesizing repeat presses.
    static EM_BOOL Key(int type, const EmscriptenKeyboardEvent* event, void* userdata);
    /// Updates mouse state in canvas device pixels.
    static EM_BOOL Pointer(int type, const EmscriptenMouseEvent* event, void* userdata);
    /// Adds wheel motion to the portable mouse state.
    static EM_BOOL Wheel(int type, const EmscriptenWheelEvent* event, void* userdata);
    /// Releases pressed state when browser focus is lost.
    static EM_BOOL Blur(int type, const EmscriptenFocusEvent* event, void* userdata);
};
}
