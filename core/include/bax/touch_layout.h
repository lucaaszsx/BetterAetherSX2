#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace bax::touch {

// The on-screen controls, in drawing order.
enum class Control : std::uint8_t {
    DPad,
    Stick,
    Cross,
    Circle,
    Square,
    Triangle,
    L1,
    L2,
    R1,
    R2,
    Select,
    Start,
};

inline constexpr std::size_t CONTROL_COUNT = static_cast<std::size_t>(Control::Start) + 1;

// A control's area in surface pixels: a circle when round, otherwise a box.
struct Shape {
    float center_x;
    float center_y;
    float half_width;
    float half_height;
    bool round;
};

// Where every control sits on a surface. Indexed by Control.
using Layout = std::array<Shape, CONTROL_COUNT>;

// Lays the controls out along the edges of a landscape surface, sized in density-independent units so they keep
// their physical size on every screen. density is pixels per dp.
Layout makeLayout(float width, float height, float density);

// The control under a point, if any. Later controls win where shapes overlap, as they are drawn on top.
std::optional<Control> controlAt(const Layout& layout, float x, float y);

// Directions pressed on the D-pad, as a bit set.
enum DPadDirection : std::uint8_t {
    DPAD_UP = 1 << 0,
    DPAD_RIGHT = 1 << 1,
    DPAD_DOWN = 1 << 2,
    DPAD_LEFT = 1 << 3,
};

// The D-pad directions for a finger at (x, y). The pad is split into eight sectors, so the diagonals press two
// directions. A finger close to the center presses nothing, which lets the player rest a thumb on it.
std::uint8_t dpadDirections(const Shape& dpad, float x, float y);

struct StickPosition {
    float x; // -1 is fully left, 1 is fully right.
    float y; // -1 is fully up, 1 is fully down.
};

// The stick tilt for a finger at (x, y): its offset from the center over the stick's radius, clamped to the unit
// circle so a finger past the edge holds the stick at full tilt.
StickPosition stickPosition(const Shape& stick, float x, float y);

} // namespace bax::touch
