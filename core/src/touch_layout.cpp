#include "bax/touch_layout.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace bax::touch {

namespace {

// Sizes in dp, chosen for thumbs: buttons are at least 48 dp across, as the Android guidelines ask for touch targets.
constexpr float EDGE_MARGIN = 24.0f;
constexpr float DPAD_RADIUS = 64.0f;
constexpr float STICK_RADIUS = 60.0f;
constexpr float FACE_BUTTON_RADIUS = 28.0f;
constexpr float FACE_BUTTON_SPREAD = 52.0f;
constexpr float SHOULDER_HALF_WIDTH = 44.0f;
constexpr float SHOULDER_HALF_HEIGHT = 20.0f;
constexpr float SHOULDER_SPACING = 8.0f;
constexpr float SMALL_BUTTON_HALF_WIDTH = 36.0f;
constexpr float SMALL_BUTTON_HALF_HEIGHT = 16.0f;
constexpr float SMALL_BUTTON_GAP = 24.0f;
// The D-pad and face buttons sit at mid-height, where the thumbs rest when holding a phone sideways, and move down on
// short screens to stay clear of the shoulder buttons.
constexpr float CLUSTER_HEIGHT = 0.5f;
// The share of the D-pad's radius around its center that presses nothing.
constexpr float DPAD_DEAD_ZONE = 0.25f;

Shape circle(float x, float y, float radius) {
    return {x, y, radius, radius, true};
}

Shape box(float x, float y, float half_width, float half_height) {
    return {x, y, half_width, half_height, false};
}

bool contains(const Shape& shape, float x, float y) {
    const float dx = x - shape.center_x;
    const float dy = y - shape.center_y;
    if (shape.round) return dx * dx + dy * dy <= shape.half_width * shape.half_width;
    return std::abs(dx) <= shape.half_width && std::abs(dy) <= shape.half_height;
}

} // namespace

Layout makeLayout(float width, float height, float density) {
    const auto dp = [density](float value) { return value * density; };
    const float margin = dp(EDGE_MARGIN);
    const float upper_shoulder_y = margin + dp(SHOULDER_HALF_HEIGHT);
    const float lower_shoulder_y = upper_shoulder_y + dp(2 * SHOULDER_HALF_HEIGHT + SHOULDER_SPACING);
    const float below_shoulders = lower_shoulder_y + dp(SHOULDER_HALF_HEIGHT + SHOULDER_SPACING);
    const float cluster_y = std::max(
        height * CLUSTER_HEIGHT, below_shoulders + dp(std::max(DPAD_RADIUS, FACE_BUTTON_SPREAD + FACE_BUTTON_RADIUS)));
    const float dpad_x = margin + dp(DPAD_RADIUS);
    const float face_x = width - margin - dp(FACE_BUTTON_SPREAD + FACE_BUTTON_RADIUS);
    const float spread = dp(FACE_BUTTON_SPREAD);
    const float shoulder_left_x = margin + dp(SHOULDER_HALF_WIDTH);
    const float shoulder_right_x = width - shoulder_left_x;
    const float small_y = height - margin - dp(SMALL_BUTTON_HALF_HEIGHT);
    const float small_offset = dp(SMALL_BUTTON_HALF_WIDTH + SMALL_BUTTON_GAP / 2);

    Layout layout{};
    const auto place = [&layout](Control control, Shape shape) { layout[static_cast<std::size_t>(control)] = shape; };
    place(Control::DPad, circle(dpad_x, cluster_y, dp(DPAD_RADIUS)));
    place(Control::Stick,
          circle(dpad_x + dp(DPAD_RADIUS + STICK_RADIUS), height - margin - dp(STICK_RADIUS), dp(STICK_RADIUS)));
    place(Control::Cross, circle(face_x, cluster_y + spread, dp(FACE_BUTTON_RADIUS)));
    place(Control::Circle, circle(face_x + spread, cluster_y, dp(FACE_BUTTON_RADIUS)));
    place(Control::Square, circle(face_x - spread, cluster_y, dp(FACE_BUTTON_RADIUS)));
    place(Control::Triangle, circle(face_x, cluster_y - spread, dp(FACE_BUTTON_RADIUS)));
    place(Control::L2, box(shoulder_left_x, upper_shoulder_y, dp(SHOULDER_HALF_WIDTH), dp(SHOULDER_HALF_HEIGHT)));
    place(Control::L1, box(shoulder_left_x, lower_shoulder_y, dp(SHOULDER_HALF_WIDTH), dp(SHOULDER_HALF_HEIGHT)));
    place(Control::R2, box(shoulder_right_x, upper_shoulder_y, dp(SHOULDER_HALF_WIDTH), dp(SHOULDER_HALF_HEIGHT)));
    place(Control::R1, box(shoulder_right_x, lower_shoulder_y, dp(SHOULDER_HALF_WIDTH), dp(SHOULDER_HALF_HEIGHT)));
    place(Control::Select,
          box(width / 2 - small_offset, small_y, dp(SMALL_BUTTON_HALF_WIDTH), dp(SMALL_BUTTON_HALF_HEIGHT)));
    place(Control::Start,
          box(width / 2 + small_offset, small_y, dp(SMALL_BUTTON_HALF_WIDTH), dp(SMALL_BUTTON_HALF_HEIGHT)));
    return layout;
}

std::optional<Control> controlAt(const Layout& layout, float x, float y) {
    for (std::size_t index = layout.size(); index-- > 0;)
        if (contains(layout[index], x, y)) return static_cast<Control>(index);
    return std::nullopt;
}

std::uint8_t dpadDirections(const Shape& dpad, float x, float y) {
    const float dx = x - dpad.center_x;
    const float dy = y - dpad.center_y;
    const float dead_zone = dpad.half_width * DPAD_DEAD_ZONE;
    if (dx * dx + dy * dy < dead_zone * dead_zone) return 0;

    // Eight sectors of 45 degrees, the first centered on right, counting clockwise because y grows downwards.
    constexpr float SECTOR = std::numbers::pi_v<float> / 4;
    constexpr int SECTOR_COUNT = 8;
    const int sector = static_cast<int>(std::lround(std::atan2(dy, dx) / SECTOR) + SECTOR_COUNT) % SECTOR_COUNT;
    constexpr std::array<std::uint8_t, SECTOR_COUNT> DIRECTIONS = {
        DPAD_RIGHT, DPAD_RIGHT | DPAD_DOWN, DPAD_DOWN, DPAD_DOWN | DPAD_LEFT,
        DPAD_LEFT,  DPAD_LEFT | DPAD_UP,    DPAD_UP,   DPAD_UP | DPAD_RIGHT,
    };
    return DIRECTIONS[static_cast<std::size_t>(sector)];
}

StickPosition stickPosition(const Shape& stick, float x, float y) {
    float tilt_x = (x - stick.center_x) / stick.half_width;
    float tilt_y = (y - stick.center_y) / stick.half_width;
    const float length = std::hypot(tilt_x, tilt_y);
    if (length > 1.0f) {
        tilt_x /= length;
        tilt_y /= length;
    }
    return {tilt_x, tilt_y};
}

} // namespace bax::touch
