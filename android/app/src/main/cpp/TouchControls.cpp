// SPDX-License-Identifier: GPL-3.0-or-later
// The on-screen controls: which finger holds which control, the pad input that follows, and the drawing over the game
// image. Touches arrive on the UI thread; the drawing runs on the GS thread. The look is a placeholder until the app's
// visual direction is chosen.

#include "TouchControls.h"

#include "Session.h"
#include "bax/touch_layout.h"
#include "imgui.h"
#include "pcsx2/ImGui/ImGuiManager.h"
#include "pcsx2/SIO/Pad/PadDualshock2.h"

#include <array>
#include <atomic>
#include <cfloat>
#include <cstdint>
#include <jni.h>
#include <mutex>
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

std::mutex s_layout_mutex;
bax::touch::Layout s_layout{};
float s_density = 0.0f;

// UI thread only.
std::unordered_map<int, Finger> s_fingers;
std::array<float, PadDualshock2::Inputs::LENGTH> s_sent_values{};

// What the drawing shows, published by the UI thread.
std::atomic<bool> s_visible = true;
std::atomic<unsigned> s_held_controls = 0;
std::atomic<std::uint8_t> s_dpad_directions = 0;
std::atomic<float> s_stick_x = 0.0f;
std::atomic<float> s_stick_y = 0.0f;

unsigned bitOf(Control control) {
    return 1u << static_cast<unsigned>(control);
}

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
    unsigned held = 0;
    std::uint8_t dpad = 0;
    bax::touch::StickPosition stick{0.0f, 0.0f};
    {
        std::lock_guard lock(s_layout_mutex);
        for (const auto& [id, finger] : s_fingers) {
            held |= bitOf(finger.control);
            if (finger.control == Control::DPad)
                dpad |= bax::touch::dpadDirections(shapeOf(s_layout, Control::DPad), finger.x, finger.y);
            else if (finger.control == Control::Stick)
                stick = bax::touch::stickPosition(shapeOf(s_layout, Control::Stick), finger.x, finger.y);
            else if (const std::optional<u32> bind = buttonBind(finger.control))
                values[*bind] = 1.0f;
        }
    }
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
    s_held_controls = held;
    s_dpad_directions = dpad;
    s_stick_x = stick.x;
    s_stick_y = stick.y;
}

void onTouch(TouchAction action, int pointer_id, float x, float y) {
    if (action == TouchAction::Down) {
        s_visible = true;
        std::optional<Control> control;
        {
            std::lock_guard lock(s_layout_mutex);
            control = bax::touch::controlAt(s_layout, x, y);
        }
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

// Placeholder colors from the app's tokens: paper/0 outlines, ink/0 fills, azure/deep for what is held.
constexpr ImU32 OUTLINE_COLOR = IM_COL32(0xED, 0xF1, 0xF5, 0x99);
constexpr ImU32 IDLE_FILL_COLOR = IM_COL32(0x0A, 0x0D, 0x12, 0x4D);
constexpr ImU32 HELD_FILL_COLOR = IM_COL32(0x1C, 0x3A, 0x5E, 0xB3);
constexpr float OUTLINE_WIDTH_DP = 2.0f;
// Sizes of the symbols drawn inside the controls, as shares of the control's size.
constexpr float SYMBOL_SIZE = 0.4f;
constexpr float DPAD_ARROW_REACH = 0.75f;
constexpr float DPAD_ARROW_SIZE = 0.18f;
constexpr float STICK_KNOB_SIZE = 0.45f;
// How far below the center the triangle symbol's base sits, so the symbol looks centered.
constexpr float TRIANGLE_BASE = 0.7f;
constexpr float LABEL_SIZE = 1.1f;

void drawSymbol(ImDrawList& draw, Control control, ImVec2 center, float radius, float width) {
    const float size = radius * SYMBOL_SIZE;
    switch (control) {
    case Control::Cross:
        draw.AddLine({center.x - size, center.y - size}, {center.x + size, center.y + size}, OUTLINE_COLOR, width);
        draw.AddLine({center.x - size, center.y + size}, {center.x + size, center.y - size}, OUTLINE_COLOR, width);
        break;
    case Control::Circle:
        draw.AddCircle(center, size, OUTLINE_COLOR, 0, width);
        break;
    case Control::Square:
        draw.AddRect({center.x - size, center.y - size}, {center.x + size, center.y + size}, OUTLINE_COLOR, 0.0f, 0,
                     width);
        break;
    case Control::Triangle:
        draw.AddTriangle({center.x, center.y - size}, {center.x + size, center.y + size * TRIANGLE_BASE},
                         {center.x - size, center.y + size * TRIANGLE_BASE}, OUTLINE_COLOR, width);
        break;
    default:
        break;
    }
}

void drawDPad(ImDrawList& draw, ImVec2 center, float radius, std::uint8_t directions) {
    struct Arrow {
        std::uint8_t direction;
        float x;
        float y;
    };
    constexpr std::array<Arrow, 4> ARROWS = {{
        {bax::touch::DPAD_UP, 0.0f, -1.0f},
        {bax::touch::DPAD_RIGHT, 1.0f, 0.0f},
        {bax::touch::DPAD_DOWN, 0.0f, 1.0f},
        {bax::touch::DPAD_LEFT, -1.0f, 0.0f},
    }};
    const float reach = radius * DPAD_ARROW_REACH;
    const float size = radius * DPAD_ARROW_SIZE;
    for (const Arrow& arrow : ARROWS) {
        const ImVec2 tip{center.x + arrow.x * reach, center.y + arrow.y * reach};
        const ImVec2 base{tip.x - arrow.x * size * 2, tip.y - arrow.y * size * 2};
        const ImVec2 side{arrow.y * size, -arrow.x * size};
        const ImU32 color = (directions & arrow.direction) ? HELD_FILL_COLOR : OUTLINE_COLOR;
        draw.AddTriangleFilled(tip, {base.x + side.x, base.y + side.y}, {base.x - side.x, base.y - side.y}, color);
    }
}

const char* labelOf(Control control) {
    switch (control) {
    case Control::L1:
        return "L1";
    case Control::L2:
        return "L2";
    case Control::R1:
        return "R1";
    case Control::R2:
        return "R2";
    case Control::Select:
        return "SELECT";
    case Control::Start:
        return "START";
    default:
        return nullptr;
    }
}

} // namespace

void hideTouchControls() {
    s_visible = false;
}

void Host::DrawTouchControls() {
    if (!s_visible) return;
    bax::touch::Layout layout;
    float density;
    {
        std::lock_guard lock(s_layout_mutex);
        if (s_density == 0.0f) return;
        layout = s_layout;
        density = s_density;
    }
    ImDrawList& draw = *ImGui::GetForegroundDrawList();
    const float width = OUTLINE_WIDTH_DP * density;
    const unsigned held = s_held_controls;
    for (std::size_t index = 0; index < layout.size(); ++index) {
        const Control control = static_cast<Control>(index);
        const bax::touch::Shape& shape = layout[index];
        const ImVec2 center{shape.center_x, shape.center_y};
        const ImU32 fill = (held & bitOf(control)) ? HELD_FILL_COLOR : IDLE_FILL_COLOR;
        if (shape.round) {
            draw.AddCircleFilled(center, shape.half_width, fill);
            draw.AddCircle(center, shape.half_width, OUTLINE_COLOR, 0, width);
        } else {
            const ImVec2 min{shape.center_x - shape.half_width, shape.center_y - shape.half_height};
            const ImVec2 max{shape.center_x + shape.half_width, shape.center_y + shape.half_height};
            draw.AddRectFilled(min, max, fill, shape.half_height);
            draw.AddRect(min, max, OUTLINE_COLOR, shape.half_height, 0, width);
        }
        drawSymbol(draw, control, center, shape.half_width, width);
        if (control == Control::DPad) drawDPad(draw, center, shape.half_width, s_dpad_directions);
        if (control == Control::Stick) {
            const float travel = shape.half_width * (1.0f - STICK_KNOB_SIZE);
            const ImVec2 knob{center.x + s_stick_x * travel, center.y + s_stick_y * travel};
            draw.AddCircleFilled(knob, shape.half_width * STICK_KNOB_SIZE, OUTLINE_COLOR);
        }
        if (const char* label = labelOf(control)) {
            const float font_size = shape.half_height * LABEL_SIZE;
            const ImVec2 text = ImGui::GetFont()->CalcTextSizeA(font_size, FLT_MAX, 0.0f, label);
            draw.AddText(ImGui::GetFont(), font_size, {center.x - text.x / 2, center.y - text.y / 2}, OUTLINE_COLOR,
                         label);
        }
    }
}

extern "C" JNIEXPORT void JNICALL Java_io_github_lucaaszsx_betteraethersx2_runtime_NativeRuntime_nativeSetTouchLayout(
    JNIEnv*, jclass, jint width, jint height, jfloat density) {
    {
        std::lock_guard lock(s_layout_mutex);
        s_layout = bax::touch::makeLayout(static_cast<float>(width), static_cast<float>(height), density);
        s_density = density;
    }
    // Fingers held across a layout change would point at controls that moved.
    s_fingers.clear();
    updatePad();
}

extern "C" JNIEXPORT void JNICALL Java_io_github_lucaaszsx_betteraethersx2_runtime_NativeRuntime_nativeTouch(
    JNIEnv*, jclass, jint action, jint pointer_id, jfloat x, jfloat y) {
    onTouch(static_cast<TouchAction>(action), pointer_id, x, y);
}
