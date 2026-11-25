#include "bax/touch_layout.h"

#include <cmath>
#include <gtest/gtest.h>

namespace bax::touch {
namespace {

constexpr float WIDTH = 2400.0f;
constexpr float HEIGHT = 1080.0f;
constexpr float DENSITY = 2.75f;

const Shape& shapeOf(const Layout& layout, Control control) {
    return layout[static_cast<std::size_t>(control)];
}

TEST(TouchLayout, KeepsEveryControlOnTheSurface) {
    for (const Shape& shape : makeLayout(WIDTH, HEIGHT, DENSITY)) {
        EXPECT_GE(shape.center_x - shape.half_width, 0.0f);
        EXPECT_LE(shape.center_x + shape.half_width, WIDTH);
        EXPECT_GE(shape.center_y - shape.half_height, 0.0f);
        EXPECT_LE(shape.center_y + shape.half_height, HEIGHT);
    }
}

TEST(TouchLayout, FindsTheControlUnderAFinger) {
    const Layout layout = makeLayout(WIDTH, HEIGHT, DENSITY);
    const Shape& cross = shapeOf(layout, Control::Cross);
    EXPECT_EQ(controlAt(layout, cross.center_x, cross.center_y), Control::Cross);
    const Shape& start = shapeOf(layout, Control::Start);
    EXPECT_EQ(controlAt(layout, start.center_x + start.half_width - 1, start.center_y), Control::Start);
}

TEST(TouchLayout, FindsNothingInTheMiddleOfTheScreen) {
    EXPECT_EQ(controlAt(makeLayout(WIDTH, HEIGHT, DENSITY), WIDTH / 2, HEIGHT / 2), std::nullopt);
}

// Two circles are compared exactly; any other pair by bounding boxes, which is stricter than the shapes themselves.
bool overlap(const Shape& a, const Shape& b) {
    if (a.round && b.round)
        return std::hypot(a.center_x - b.center_x, a.center_y - b.center_y) < a.half_width + b.half_width;
    return std::abs(a.center_x - b.center_x) < a.half_width + b.half_width &&
           std::abs(a.center_y - b.center_y) < a.half_height + b.half_height;
}

void expectNoOverlap(float width, float height, float density) {
    const Layout layout = makeLayout(width, height, density);
    for (std::size_t first = 0; first < layout.size(); ++first)
        for (std::size_t second = first + 1; second < layout.size(); ++second)
            EXPECT_FALSE(overlap(layout[first], layout[second])) << "controls " << first << " and " << second;
}

TEST(TouchLayout, KeepsControlsApartOnATallPhone) {
    expectNoOverlap(WIDTH, HEIGHT, DENSITY);
}

// 800 x 360 dp, about the smallest landscape phone screen in use.
TEST(TouchLayout, KeepsControlsApartOnASmallPhone) {
    expectNoOverlap(1600.0f, 720.0f, 2.0f);
}

TEST(TouchLayout, PressesOneDirectionOnTheEdgesOfTheDPad) {
    const Shape dpad{100.0f, 100.0f, 50.0f, 50.0f, true};
    EXPECT_EQ(dpadDirections(dpad, 140.0f, 100.0f), DPAD_RIGHT);
    EXPECT_EQ(dpadDirections(dpad, 100.0f, 60.0f), DPAD_UP);
    EXPECT_EQ(dpadDirections(dpad, 60.0f, 102.0f), DPAD_LEFT);
    EXPECT_EQ(dpadDirections(dpad, 100.0f, 140.0f), DPAD_DOWN);
}

TEST(TouchLayout, PressesTwoDirectionsOnADiagonal) {
    const Shape dpad{100.0f, 100.0f, 50.0f, 50.0f, true};
    EXPECT_EQ(dpadDirections(dpad, 130.0f, 70.0f), DPAD_UP | DPAD_RIGHT);
    EXPECT_EQ(dpadDirections(dpad, 70.0f, 130.0f), DPAD_DOWN | DPAD_LEFT);
}

TEST(TouchLayout, PressesNothingNearTheDPadCenter) {
    const Shape dpad{100.0f, 100.0f, 50.0f, 50.0f, true};
    EXPECT_EQ(dpadDirections(dpad, 105.0f, 103.0f), 0);
}

TEST(TouchLayout, TiltsTheStickByTheFingerOffset) {
    const Shape stick{100.0f, 100.0f, 50.0f, 50.0f, true};
    const StickPosition half_right = stickPosition(stick, 125.0f, 100.0f);
    EXPECT_FLOAT_EQ(half_right.x, 0.5f);
    EXPECT_FLOAT_EQ(half_right.y, 0.0f);
}

TEST(TouchLayout, HoldsTheStickAtFullTiltPastItsEdge) {
    const Shape stick{100.0f, 100.0f, 50.0f, 50.0f, true};
    const StickPosition far_up = stickPosition(stick, 100.0f, -100.0f);
    EXPECT_FLOAT_EQ(far_up.x, 0.0f);
    EXPECT_FLOAT_EQ(far_up.y, -1.0f);
}

} // namespace
} // namespace bax::touch
