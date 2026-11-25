// SPDX-License-Identifier: GPL-3.0-or-later
// The on-screen controls: which finger holds which control, and the pad input that follows. Touches arrive on the UI
// thread.

#include "Session.h"
#include "bax/touch_layout.h"
#include "pcsx2/SIO/Pad/PadDualshock2.h"

#include <array>
#include <cstdint>
#include <jni.h>
#include <optional>
#include <unordered_map>

namespace {

using bax::touch::Control;

// Mirrors TouchAction in the app.
enum class TouchAction : int { Down, Move, Up };

struct Finger {
    Control control;
    float x;
    float y;
};

// UI thread only.
bax::touch::Layout s_layout{};
std::unordered_map<int, Finger> s_fingers;
std::array<float, PadDualshock2::Inputs::LENGTH> s_sent_values{};

const bax::touch::Shape& shapeOf(const bax::touch::Layout& layout, Control control) {
    return layout[static_cast<std::size_t>(control)];
}

// The pad bind of a control that is a plain button; the D-pad and stick drive several binds.
std::optional<u32> buttonBind(Control control) {
    switch (control) {
    case Control::Cross:
        return PadDualshock2::Inputs::PAD_CROSS;
    case Control::Circle:
        return PadDualshock2::Inputs::PAD_CIRCLE;
    case Control::Square:
        return PadDualshock2::Inputs::PAD_SQUARE;
    case Control::Triangle:
        return PadDualshock2::Inputs::PAD_TRIANGLE;
    case Control::L1:
        return PadDualshock2::Inputs::PAD_L1;
    case Control::L2:
        return PadDualshock2::Inputs::PAD_L2;
    case Control::R1:
        return PadDualshock2::Inputs::PAD_R1;
    case Control::R2:
        return PadDualshock2::Inputs::PAD_R2;
    case Control::Select:
        return PadDualshock2::Inputs::PAD_SELECT;
    case Control::Start:
        return PadDualshock2::Inputs::PAD_START;
    default:
        return std::nullopt;
    }
}

// Works out the pad from every finger on the screen and sends what changed.
void updatePad() {
    std::array<float, PadDualshock2::Inputs::LENGTH> values{};
    std::uint8_t dpad = 0;
    bax::touch::StickPosition stick{0.0f, 0.0f};
    for (const auto& [id, finger] : s_fingers)
        if (finger.control == Control::DPad)
            dpad |= bax::touch::dpadDirections(shapeOf(s_layout, Control::DPad), finger.x, finger.y);
        else if (finger.control == Control::Stick)
            stick = bax::touch::stickPosition(shapeOf(s_layout, Control::Stick), finger.x, finger.y);
        else if (const std::optional<u32> bind = buttonBind(finger.control))
            values[*bind] = 1.0f;
    values[PadDualshock2::Inputs::PAD_UP] = (dpad & bax::touch::DPAD_UP) ? 1.0f : 0.0f;
    values[PadDualshock2::Inputs::PAD_RIGHT] = (dpad & bax::touch::DPAD_RIGHT) ? 1.0f : 0.0f;
    values[PadDualshock2::Inputs::PAD_DOWN] = (dpad & bax::touch::DPAD_DOWN) ? 1.0f : 0.0f;
    values[PadDualshock2::Inputs::PAD_LEFT] = (dpad & bax::touch::DPAD_LEFT) ? 1.0f : 0.0f;
    values[PadDualshock2::Inputs::PAD_L_LEFT] = stick.x < 0.0f ? -stick.x : 0.0f;
    values[PadDualshock2::Inputs::PAD_L_RIGHT] = stick.x > 0.0f ? stick.x : 0.0f;
    values[PadDualshock2::Inputs::PAD_L_UP] = stick.y < 0.0f ? -stick.y : 0.0f;
    values[PadDualshock2::Inputs::PAD_L_DOWN] = stick.y > 0.0f ? stick.y : 0.0f;

    for (u32 bind = 0; bind < values.size(); ++bind) {
        if (values[bind] == s_sent_values[bind]) continue;
        s_sent_values[bind] = values[bind];
        setPadInput(bind, values[bind]);
    }
}

void onTouch(TouchAction action, int pointer_id, float x, float y) {
    if (action == TouchAction::Down) {
        const std::optional<Control> control = bax::touch::controlAt(s_layout, x, y);
        if (!control) return;
        s_fingers[pointer_id] = {*control, x, y};
    } else if (action == TouchAction::Move) {
        const auto finger = s_fingers.find(pointer_id);
        if (finger == s_fingers.end()) return;
        finger->second.x = x;
        finger->second.y = y;
    } else {
        s_fingers.erase(pointer_id);
    }
    updatePad();
}

} // namespace

extern "C" JNIEXPORT void JNICALL Java_io_github_lucaaszsx_betteraethersx2_runtime_NativeRuntime_nativeSetTouchLayout(
    JNIEnv*, jclass, jint width, jint height, jfloat density) {
    s_layout = bax::touch::makeLayout(static_cast<float>(width), static_cast<float>(height), density);
    // Fingers held across a layout change would point at controls that moved.
    s_fingers.clear();
    updatePad();
}

extern "C" JNIEXPORT void JNICALL Java_io_github_lucaaszsx_betteraethersx2_runtime_NativeRuntime_nativeTouch(
    JNIEnv*, jclass, jint action, jint pointer_id, jfloat x, jfloat y) {
    onTouch(static_cast<TouchAction>(action), pointer_id, x, y);
}
