#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "tests.hpp"

#include <charconv>
#include <format>
#include <optional>

// pull one window's block out of a /clients response, so its geometry can be
// read without focusing it. the workspace disambiguates a class that exists
// on more than one.
static std::string windowBlock(const std::string& clients, const std::string& cls, std::optional<int> ws = std::nullopt) {
    const auto CLASS = std::format("\n\tclass: {}\n", cls);
    const auto WS    = ws ? std::format("\n\tworkspace: {} (", *ws) : "";

    size_t     pos = 0;
    while ((pos = clients.find("Window ", pos)) != std::string::npos) {
        const auto END   = clients.find("Window ", pos + 1);
        const auto BLOCK = clients.substr(pos, END == std::string::npos ? std::string::npos : END - pos);

        if (BLOCK.contains(CLASS) && BLOCK.contains(WS))
            return BLOCK;

        pos++;
    }

    return "";
}

// "at" and "size" are printed as "x,y"
static std::pair<int, int> attributePair(const std::string& block, const std::string& attr) {
    const auto VALUE = Tests::getAttribute(block, attr);
    const auto COMMA = VALUE.find(',');

    if (COMMA == std::string::npos)
        return {0, 0};

    int x = 0, y = 0;
    std::from_chars(VALUE.data(), VALUE.data() + COMMA, x);
    std::from_chars(VALUE.data() + COMMA + 1, VALUE.data() + VALUE.size(), y);
    return {x, y};
}

TEST_CASE(dwindleFloatClamp) {
    for (auto const& win : {"a", "b", "c"}) {
        if (!Tests::spawnKitty(win)) {
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
        }
    }

    OK(getFromSocket("/eval hl.config({ dwindle = { force_split = 2 } })"));
    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', reserved = { top = 0, right = 20, bottom = 0, left = 20 } })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:c' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set', window = 'class:c' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.resize({ x = 1200, y = 900, window = 'class:c' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'unset', window = 'class:c' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set', window = 'class:c' })"));

    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at:");
        EXPECT_CONTAINS(str, "size: 1200,900");
    }

    OK(getFromSocket("/eval hl.config({ dwindle = { force_split = 0 } })"));
}

TEST_CASE(dwindleIssue13349) {

    // Test if dwindle properly uses a focal point to place a new window.
    // exposed by #13349 as a regression from #12890

    for (auto const& win : {"a", "b", "c"}) {
        if (!Tests::spawnKitty(win))
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:c' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 967,547");
        EXPECT_CONTAINS(str, "size: 931,511");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.move({ direction = 'left' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,547");
        EXPECT_CONTAINS(str, "size: 931,511");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.move({ direction = 'right' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 967,547");
        EXPECT_CONTAINS(str, "size: 931,511");
    }
}

TEST_CASE(dwindleSplit) {
    // Test various split methods

    Tests::spawnKitty("a");

    // these must not crash
    EXPECT_NOT(getFromSocket("/dispatch hl.dsp.layout('swapsplit')"), "ok");
    EXPECT_NOT(getFromSocket("/dispatch hl.dsp.layout('splitratio 1 exact')"), "ok");

    Tests::spawnKitty("b");

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('splitratio -0.2')"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 743,1036");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('splitratio 1.6 exact')"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1495,1036");
    }

    EXPECT_NOT(getFromSocket("/dispatch hl.dsp.layout('splitratio fhne exact')"), "ok");
    EXPECT_NOT(getFromSocket("/dispatch hl.dsp.layout('splitratio exact')"), "ok");
    EXPECT_NOT(getFromSocket("/dispatch hl.dsp.layout('splitratio -....9')"), "ok");
    EXPECT_NOT(getFromSocket("/dispatch hl.dsp.layout('splitratio ..9')"), "ok");
    EXPECT_NOT(getFromSocket("/dispatch hl.dsp.layout('splitratio')"), "ok");

    OK(getFromSocket("/dispatch hl.dsp.layout('togglesplit')"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876,823");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('swapsplit')"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,859");
        EXPECT_CONTAINS(str, "size: 1876,199");
    }
}

TEST_CASE(dwindleRotateSplit) {
    OK(getFromSocket("r/eval hl.config({ general = { gaps_in = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ general = { gaps_out = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ general = { border_size = 0 } })"));

    for (auto const& win : {"a", "b"}) {
        if (!Tests::spawnKitty(win))
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
    }

    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 0,0");
        EXPECT_CONTAINS(str, "size: 960,1080");
    }

    // test 4 repeated rotations by 90 degrees
    OK(getFromSocket("/dispatch hl.dsp.layout('rotatesplit')"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 0,0");
        EXPECT_CONTAINS(str, "size: 1920,540");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('rotatesplit')"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 960,0");
        EXPECT_CONTAINS(str, "size: 960,1080");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('rotatesplit')"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 0,540");
        EXPECT_CONTAINS(str, "size: 1920,540");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('rotatesplit')"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 0,0");
        EXPECT_CONTAINS(str, "size: 960,1080");
    }

    // test different angles
    OK(getFromSocket("/dispatch hl.dsp.layout('rotatesplit 180')"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 960,0");
        EXPECT_CONTAINS(str, "size: 960,1080");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('rotatesplit 270')"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 0,540");
        EXPECT_CONTAINS(str, "size: 1920,540");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('rotatesplit 360')"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 0,0");
        EXPECT_CONTAINS(str, "size: 1920,540");
    }

    // test negative angles
    OK(getFromSocket("/dispatch hl.dsp.layout('rotatesplit -90')"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 0,0");
        EXPECT_CONTAINS(str, "size: 960,1080");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('rotatesplit -180')"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 960,0");
        EXPECT_CONTAINS(str, "size: 960,1080");
    }
}

TEST_CASE(dwindleForceSplitOnMoveToWorkspace) {
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '2' })"));
    ASSERT(!!Tests::spawnKitty("kitty"), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    ASSERT(!!Tests::spawnKitty("kitty"), true);
    std::string posBefore = "at: " + Tests::getAttribute(getFromSocket("/activewindow"), "at");

    OK(getFromSocket("/eval hl.config({ dwindle = { force_split = 2 } })"));
    OK(getFromSocket("/dispatch hl.dsp.cursor.move_to_corner({ corner = 3 })")); // top left
    OK(getFromSocket("/dispatch hl.dsp.window.move({ workspace = '2' })"));

    // Should be moved to the right, so the position should change
    std::string activeWindow = getFromSocket("/activewindow");
    EXPECT(activeWindow.contains(posBefore), false);
}

TEST_CASE(dwindleMoveAcrossToggledSplit) {
    // If we have a split whose orientation has been manually toggled (e.g.
    // vertically stacked, when the split's aspect ratio is such that it would
    // prefer to be horizontally stacked by default), moving a window across
    // the split should NOT revert back to the preferred split orientation

    OK(getFromSocket("/eval hl.config({ dwindle = { force_split = 2 } })"));
    for (auto const& win : {"a", "b"}) {
        if (!Tests::spawnKitty(win))
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
    }
    OK(getFromSocket("/dispatch hl.dsp.layout('togglesplit')"));
    // Window A, now on top, is to be moved

    auto origWinB   = getFromSocket("/activewindow");
    auto expectPos  = "at: " + Tests::getAttribute(origWinB, "at");
    auto expectSize = "size: " + Tests::getAttribute(origWinB, "size");
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));

    OK(getFromSocket("/dispatch hl.dsp.window.move({ direction = 'down' })"));

    // Window A should be moved down, so position and size should swap with window B
    auto newWinA = getFromSocket("/activewindow");
    EXPECT_CONTAINS(newWinA, std::move(expectPos));
    EXPECT_CONTAINS(newWinA, std::move(expectSize));
}

TEST_CASE(dwindleMoveSmallWindowAcrossSplit) {
    // Small windows (<50% of their parent split's area) should be possible to
    // move across a split. Focal point weirdness has broken this in the past.

    OK(getFromSocket("/eval hl.config({ dwindle = { force_split = 1 } })"));
    OK(getFromSocket("/eval hl.config({ dwindle = { default_split_ratio = 1.2 } })"));
    for (auto const& win : {"a", "b"}) {
        if (!Tests::spawnKitty(win))
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
    }
    // Window B, on the left, is the smaller one

    auto posBefore = "at: " + Tests::getAttribute(getFromSocket("/activewindow"), "at");

    OK(getFromSocket("/dispatch hl.dsp.window.move({ direction = 'right' })"));

    // Window B should be moved right, so position should change
    EXPECT_NOT_CONTAINS(getFromSocket("/activewindow"), posBefore);
}

/*
    Fullscreen Tests

    Tests with `Shared test among all default handled FS` comment are duplicated among all layouts to test each layout individually

*/

TEST_CASE(dwindleFullscreenMaximiseDispatchers) {

    // Shared test among all default handled FS

    OK(getFromSocket("/eval hl.config({ general = { layout = 'dwindle' } })"));

    Tests::spawnKitty("kitty_A");
    Tests::spawnKitty("kitty_B");

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_A' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'unset' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', action = 'toggle' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 1");
        EXPECT_CONTAINS(str, "fullscreenClient: 1");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', action = 'toggle' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 2, client = 2, action = 'set' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 2, client = 2, action = 'set' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 2, client = 2, action = 'toggle' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 2, client = 2, action = 'toggle' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }
}

TEST_CASE(dwindleTestFsFocusUnderFSWindow) {

    // Shared test among all default handled FS

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'dwindle' } })"));

    for (auto const& win : {"one", "two", "three"}) {
        if (!Tests::spawnKitty(win))
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:one' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876,1036");
        EXPECT_CONTAINS(str, "class: one");
    }

    OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 1 } })"));

    Tests::spawnKitty("four");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876,1036");
        EXPECT_CONTAINS(str, "class: four");
        EXPECT_CONTAINS(str, "fullscreen: 1");
    }

    OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 0 } })"));

    Tests::spawnKitty("ignored");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876,1036");
        EXPECT_CONTAINS(str, "class: four");
        EXPECT_CONTAINS(str, "fullscreen: 1");
    }

    OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 2 } })"));

    Tests::spawnKitty("erstarrwashere");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: erstarrwashere");
        EXPECT_CONTAINS(str, "fullscreen: 0");
    }
}

TEST_CASE(dwindleNewWindowTakesOverFullscreen) {

    // Shared test among all default handled FS

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'dwindle' } })"));

    OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 0 } })"));

    Tests::spawnKitty("kitty_A");

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
        EXPECT_CONTAINS(str, "kitty_A");
    }

    Tests::spawnKitty("kitty_B");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
        EXPECT_CONTAINS(str, "kitty_A");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_B' })"));

    {
        // should be ignored as per focus_under_fullscreen 0
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
        EXPECT_CONTAINS(str, "kitty_A");
    }

    OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 1 } })"));

    Tests::spawnKitty("kitty_C");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
        EXPECT_CONTAINS(str, "kitty_C");
    }

    OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 2 } })"));

    Tests::spawnKitty("kitty_D");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
        EXPECT_CONTAINS(str, "kitty_D");
    }

    OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 0 } })"));

    Tests::killAllWindows();
}

TEST_CASE(dwindleExitWindowRetainsFullscreen) {

    // Shared test among all default handled FS

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'dwindle' } })"));

    OK(getFromSocket("/eval hl.config({ misc = { exit_window_retains_fullscreen = false } })"));

    Tests::spawnKitty("kitty_A");
    Tests::spawnKitty("kitty_B");

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.kill({ window = 'activewindow' })"));
    Tests::waitUntilWindowsN(1);

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
    }

    Tests::spawnKitty("kitty_B");
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen' })"));
    OK(getFromSocket("/eval hl.config({ misc = { exit_window_retains_fullscreen = true } })"));

    OK(getFromSocket("/dispatch hl.dsp.window.kill({ window = 'activewindow' })"));
    Tests::waitUntilWindowsN(1);

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    Tests::killAllWindows();
}

TEST_CASE(dwindleFullscreenPinnedWindows) {

    // Shared test among all default handled FS

    /*
    
    allow_pin_fullscreen -> Allow internal FSing a pinned window at all?

    if true: FSed pinned window doesn't behave as pinned while it is FS but continues to behave as pinned when it's unFS 
    if false: doesn't allow FSing it at all (client can be set if de-syncing internal and client)

    */

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'dwindle' } })"));

    Tests::spawnKitty("cake");

    OK(getFromSocket("/dispatch hl.dsp.window.float({action = 'enable', window = 'class:cake'})"));

    // resize to expected floating value: 200 x 200
    OK(getFromSocket("/dispatch hl.dsp.window.resize({x = 200, y = 200, relative = false, window = 'class:cake'})"));

    // Workspace we are testing on: 1
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));

    // Pin the window
    OK(getFromSocket("r/dispatch hl.dsp.window.pin({ window = 'class:cake' })"));

    // set to false, try to FS; expect the cake to be a lie
    OK(getFromSocket("r/eval hl.config({ binds = { allow_pin_fullscreen = false } })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:cake'})"));

    // Try with fullscreen
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: cake");
        EXPECT_CONTAINS(str, "pinned: 1");
        EXPECT_CONTAINS(str, "pinFullscreened: 0");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
        EXPECT_CONTAINS(str, "size: 200,200");
    }

    // Try with maximised
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: cake");
        EXPECT_CONTAINS(str, "pinned: 1");
        EXPECT_CONTAINS(str, "pinFullscreened: 0");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
        EXPECT_CONTAINS(str, "size: 200,200");
    }

    // Move to another workspace, expect it to follow
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '2' })"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "class: cake");
        EXPECT_CONTAINS(str, "pinned: 1");
        EXPECT_CONTAINS(str, "pinFullscreened: 0");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
        EXPECT_CONTAINS(str, "size: 200,200");
        EXPECT_CONTAINS(str, "workspace: 2");
    }

    // Move back to primary testing workspace, assumed it'll follow since the last test passed
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));

    // While syncing FS state, is not supposed to set either mode. If internal and client are decoupled, client is expected to go through
    // Try with fullscreen
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 2, client = 2, action = 'set', window = 'activewindow' })"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "class: cake");
        EXPECT_CONTAINS(str, "pinned: 1");
        EXPECT_CONTAINS(str, "pinFullscreened: 0");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
        EXPECT_CONTAINS(str, "size: 200,200");
        EXPECT_CONTAINS(str, "workspace: 1");
    }

    // Try with maximised
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 1, client = 1, action = 'set', window = 'activewindow' })"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "class: cake");
        EXPECT_CONTAINS(str, "pinned: 1");
        EXPECT_CONTAINS(str, "pinFullscreened: 0");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 1");
        EXPECT_CONTAINS(str, "size: 200,200");
        EXPECT_CONTAINS(str, "workspace: 1");
    }

    // re-set its FS values for the next test
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 0, client = 0, action = 'set', window = 'activewindow' })"));

    // set to true, try to FS; expect the cake to be real
    OK(getFromSocket("r/eval hl.config({ binds = { allow_pin_fullscreen = true } })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:cake'})"));

    // Try with fullscreen
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: cake");
        EXPECT_CONTAINS(str, "pinned: 0");
        EXPECT_CONTAINS(str, "pinFullscreened: 1");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
        EXPECT_CONTAINS(str, "at: 0,0");
        ASSERT_CONTAINS(str, "size: 1920,1080");
    }

    // Try with maximised
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: cake");
        EXPECT_CONTAINS(str, "pinned: 0");
        EXPECT_CONTAINS(str, "pinFullscreened: 1");
        EXPECT_CONTAINS(str, "fullscreen: 1");
        EXPECT_CONTAINS(str, "fullscreenClient: 1");
        EXPECT_CONTAINS(str, "at: 2,2");
        EXPECT_CONTAINS(str, "size: 1916,1076");
    }

    // unFs it, move to another workspace - expect it to follow
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 0, client = 0, action = 'set', window = 'activewindow' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '2' })"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "class: cake");
        // After the FSed pinned window is unFSed, expect its pinned value to come back
        EXPECT_CONTAINS(str, "pinned: 1");
        EXPECT_CONTAINS(str, "pinFullscreened: 0");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
        EXPECT_CONTAINS(str, "size: 200,200");
        EXPECT_CONTAINS(str, "workspace: 2");
    }

    // set the variable to false, unpin it and expect it to be FS-able
    OK(getFromSocket("r/eval hl.config({ binds = { allow_pin_fullscreen = false } })"));
    OK(getFromSocket("r/dispatch hl.dsp.window.pin({ window = 'class:cake' })"));

    // Try with fullscreen
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: cake");
        EXPECT_CONTAINS(str, "pinned: 0");
        EXPECT_CONTAINS(str, "pinFullscreened: 0");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
        EXPECT_CONTAINS(str, "at: 0,0");
        ASSERT_CONTAINS(str, "size: 1920,1080");
    }

    // Try with maximised
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: cake");
        EXPECT_CONTAINS(str, "pinned: 0");
        EXPECT_CONTAINS(str, "pinFullscreened: 0");
        EXPECT_CONTAINS(str, "fullscreen: 1");
        EXPECT_CONTAINS(str, "fullscreenClient: 1");
        EXPECT_CONTAINS(str, "at: 2,2");
        EXPECT_CONTAINS(str, "size: 1916,1076");
    }
}

TEST_CASE(dwindleFullscreenNonInterference) {

    // Shared test among all default handled FS

    /*
    
    When a tiled/floating window is default handled FSed, it must not cause the windows under it to have moved/resized after it is unFSed

    also tests if floating pos/size is properly restored after fS-unfs

    */

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'dwindle' } })"));

    Tests::spawnKitty("red");
    Tests::spawnKitty("crimson");
    Tests::spawnKitty("blue");
    Tests::spawnKitty("cyan");
    Tests::spawnKitty("azure");
    Tests::spawnKitty("green");

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:red' })"));

    // Testing tiled first
    {

        // save all pos/size inc red

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:red' })"));
        auto redPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
        auto redSize = Tests::getAttribute(getFromSocket("/activewindow"), "size");

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:crimson' })"));
        auto crimsonPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
        auto crimsonSize = Tests::getAttribute(getFromSocket("/activewindow"), "size");

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:blue' })"));
        auto bluePos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
        auto blueSize = Tests::getAttribute(getFromSocket("/activewindow"), "size");

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:cyan' })"));
        auto cyanPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
        auto cyanSize = Tests::getAttribute(getFromSocket("/activewindow"), "size");

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:azure' })"));
        auto azurePos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
        auto azureSize = Tests::getAttribute(getFromSocket("/activewindow"), "size");

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:green' })"));
        auto greenPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
        auto greenSize = Tests::getAttribute(getFromSocket("/activewindow"), "size");

        // FS and unFS red, then check all positions are unchanged
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:red' })"));
        OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized' })"));
        OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized' })"));

        // red
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:red' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"), redPos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), redSize);

        // crimson
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:crimson' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"), crimsonPos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), crimsonSize);

        // blue
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:blue' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"), bluePos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), blueSize);

        // cyan
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:cyan' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"), cyanPos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), cyanSize);

        // azure
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:azure' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"), azurePos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), azureSize);

        // green
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:green' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"), greenPos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), greenSize);
    }

    // test floating

    {

        // float red
        OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set', window = 'class:red' })"));

        // save all pos/size (red's size will be its floating size)
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:red' })"));
        auto redPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
        auto redSize = Tests::getAttribute(getFromSocket("/activewindow"), "size");

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:crimson' })"));
        auto crimsonPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
        auto crimsonSize = Tests::getAttribute(getFromSocket("/activewindow"), "size");

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:blue' })"));
        auto bluePos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
        auto blueSize = Tests::getAttribute(getFromSocket("/activewindow"), "size");

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:cyan' })"));
        auto cyanPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
        auto cyanSize = Tests::getAttribute(getFromSocket("/activewindow"), "size");

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:azure' })"));
        auto azurePos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
        auto azureSize = Tests::getAttribute(getFromSocket("/activewindow"), "size");

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:green' })"));
        auto greenPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
        auto greenSize = Tests::getAttribute(getFromSocket("/activewindow"), "size");

        // FS and unFS red, then check all positions are unchanged
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:red' })"));
        OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized' })"));
        OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized' })"));

        // red
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:red' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"), redPos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), redSize);

        // crimson
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:crimson' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"), crimsonPos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), crimsonSize);

        // blue
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:blue' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"), bluePos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), blueSize);

        // cyan
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:cyan' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"), cyanPos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), cyanSize);

        // azure
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:azure' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"), azurePos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), azureSize);

        // green
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:green' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"), greenPos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), greenSize);
    }
}

TEST_CASE(defaultHandledFsfocusInDirection) {

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'dwindle' } })"));

    /*
            This test serves as a test for all layouts that use deafult FS behaviour
    */

    Tests::spawnKitty("normal1");
    Tests::spawnKitty("fs");
    Tests::spawnKitty("normal2");

    // if movefocus_cycles_fullscreen = false, all focus({direction}) is disallowed from moving focus from FS window
    OK(getFromSocket("r/eval hl.config({ binds = { movefocus_cycles_fullscreen = false } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:fs' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set', window = 'class:fs' })"));

    // on_focus_under_fullscreen = 0
    OK(getFromSocket("r/eval hl.config({ misc = { on_focus_under_fullscreen = 0 } })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: fs");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: fs");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'left' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: fs");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'up' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: fs");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'down' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: fs");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    // on_focus_under_fullscreen = 1
    OK(getFromSocket("r/eval hl.config({ misc = { on_focus_under_fullscreen = 1 } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'left' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: fs");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: fs");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    // on_focus_under_fullscreen = 2
    OK(getFromSocket("r/eval hl.config({ misc = { on_focus_under_fullscreen = 2 } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'left' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: fs");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    // if movefocus_cycles_fullscreen = true

    OK(getFromSocket("r/eval hl.config({ binds = { movefocus_cycles_fullscreen = true } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:fs' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set', window = 'class:fs' })"));

    // on_focus_under_fullscreen = 0
    OK(getFromSocket("r/eval hl.config({ misc = { on_focus_under_fullscreen = 0 } })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: fs");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: fs");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'left' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: fs");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'up' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: fs");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'down' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: fs");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    // on_focus_under_fullscreen = 1
    OK(getFromSocket("r/eval hl.config({ misc = { on_focus_under_fullscreen = 1 } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'left' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: normal1");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: fs");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    // on_focus_under_fullscreen = 2
    OK(getFromSocket("r/eval hl.config({ misc = { on_focus_under_fullscreen = 2 } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'left' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: normal1");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
    }
}

TEST_CASE(dwindleLayoutMsgWorkspaceTarget) {

    // layoutmsg can address a workspace other than the active one, for the
    // messages that are safe to handle that way - which mostly means naming
    // their own target, though the preselect reset qualifies by touching
    // nothing but that workspace's own pending state.

    const auto targeted = [](const std::string& ws, const std::string& msg) {
        return getFromSocket(std::format("/dispatch hl.dsp.layout({{ message = '{}', workspace = '{}' }})", msg, ws));
    };

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '2' })"));
    for (auto const& win : {"a", "b", "c"}) {
        SPAWN_KITTY(win);
    }
    Tests::waitUntilWindowsN(3);

    // leave that workspace entirely
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    SPAWN_KITTY("d");
    Tests::waitUntilWindowsN(4);

    const auto BEFORE   = getFromSocket("/clients");
    const auto C_BEFORE = Tests::getAttribute(windowBlock(BEFORE, "c"), "at");
    const auto D_BEFORE = Tests::getAttribute(windowBlock(BEFORE, "d"), "at");
    EXPECT_NOT(C_BEFORE, std::string{});

    // restructure workspace 2 while sitting on workspace 1
    OK(targeted("2", "movetoroot class:c"));

    const auto AFTER = getFromSocket("/clients");
    EXPECT_NOT(Tests::getAttribute(windowBlock(AFTER, "c"), "at"), C_BEFORE); // the addressed workspace changed
    EXPECT(Tests::getAttribute(windowBlock(AFTER, "d"), "at"), D_BEFORE);     // the active one did not
    EXPECT_CONTAINS(getFromSocket("/activeworkspace"), "ID 1");

    // the workspace field takes the selector forms dispatchers accept, and has
    // to name one that exists - the algorithm addressed belongs to a live space
    EXPECT_NOT(targeted("999999", "movetoroot class:c"), "ok");
    EXPECT_NOT(targeted("name:nosuchworkspace", "movetoroot class:c"), "ok");

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:build' })"));
    for (auto const& win : {"e", "f", "g"}) {
        SPAWN_KITTY(win);
    }
    Tests::waitUntilWindowsN(7);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));

    const auto G_BEFORE = Tests::getAttribute(windowBlock(getFromSocket("/clients"), "g"), "at");
    OK(targeted("name:build", "movetoroot class:g"));
    EXPECT_NOT(Tests::getAttribute(windowBlock(getFromSocket("/clients"), "g"), "at"), G_BEFORE);
    EXPECT_CONTAINS(getFromSocket("/activeworkspace"), "ID 1");

    // a message may only be addressed to a workspace if handling it cannot
    // reach outside that workspace. these take their node from the focused
    // window, so they would act on whatever the user is looking at - or, for
    // the first three, quietly do nothing and report success.
    for (auto const& msg : {"togglesplit", "swapsplit", "rotatesplit", "splitratio 0.5"}) {
        EXPECT_NOT(targeted("1", msg), "ok");
        EXPECT_NOT(targeted("2", msg), "ok");
    }

    // the two that act on a node are refused when they do not name one
    EXPECT_NOT(targeted("2", "movetoroot"), "ok");
    EXPECT_NOT(targeted("2", "preselect r"), "ok");

    // but a preselect reset names nothing because it places nothing - it only
    // clears that workspace's pending state, which is safe to do from here
    OK(targeted("2", "preselect x"));

    // and the gate does not care whether the workspace addressed is the one in
    // focus. the same messages are allowed, and the same ones refused.
    OK(targeted("1", "preselect r class:d"));
    EXPECT_NOT(targeted("1", "togglesplit"), "ok");

    // "active" means the focused window, so it would answer differently
    // depending on where the user is looking. refused either side of focus,
    // for both messages that take a selector.
    EXPECT_NOT(targeted("2", "preselect r active"), "ok");
    EXPECT_NOT(targeted("1", "preselect r active"), "ok");
    EXPECT_NOT(targeted("2", "movetoroot active"), "ok");
    EXPECT_NOT(targeted("1", "movetoroot active"), "ok");

    // the table form is for addressing a workspace, so it has to name one. a
    // missing, empty or mistyped field must not fall through to the active
    // workspace, which is what the string form is for.
    //
    // the message below is one that *would* succeed unaddressed - d is tiled
    // on the active workspace - so these can only pass by being refused.
    EXPECT_NOT(getFromSocket("/dispatch hl.dsp.layout({ message = 'preselect r class:d' })"), "ok");
    EXPECT_NOT(getFromSocket("/dispatch hl.dsp.layout({ message = 'preselect r class:d', workspace = '' })"), "ok");
    EXPECT_NOT(getFromSocket("/dispatch hl.dsp.layout({ message = 'preselect r class:d', workpace = '1' })"), "ok");
    EXPECT_NOT(getFromSocket("/dispatch hl.dsp.layout({ message = '', workspace = '1' })"), "ok");

    // the string form still reaches the active workspace, unaddressed
    OK(getFromSocket("/dispatch hl.dsp.layout('preselect r class:d')"));

    // workspace 1 outlives the harness reset, and its algorithm with it, so
    // nothing may be left armed here for the next test to consume
    OK(getFromSocket("/dispatch hl.dsp.layout('preselect x')"));

    // special workspaces address like any other. this goes last because an
    // open special workspace is what an unaddressed message resolves to, so
    // leaving one open would change the meaning of every check above.
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'special:scratch' })"));
    for (auto const& win : {"h", "i", "j"}) {
        SPAWN_KITTY(win);
    }
    Tests::waitUntilWindowsN(10);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));

    const auto J_BEFORE = Tests::getAttribute(windowBlock(getFromSocket("/clients"), "j"), "at");
    OK(targeted("special:scratch", "movetoroot class:j"));
    EXPECT_NOT(Tests::getAttribute(windowBlock(getFromSocket("/clients"), "j"), "at"), J_BEFORE);
}

TEST_CASE(dwindlePreselectNamedTarget) {

    // preselect can name the window the next one splits off, instead of it
    // being implied by the pointer or the focused window. asserted via
    // directional focus so it doesn't depend on the output size.

    for (auto const& win : {"a", "b"}) {
        SPAWN_KITTY(win);
    }
    Tests::waitUntilWindowsN(2);

    // b is focused; ask for the next window to split a anyway
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:b' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('preselect r class:a')"));
    SPAWN_KITTY("c");
    Tests::waitUntilWindowsN(3);

    // c split a, so it sits directly right of it
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:c' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'l' })"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: a");

    // and had it not been named, b (focused) would have been split instead
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:b' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('preselect r')"));
    SPAWN_KITTY("d");
    Tests::waitUntilWindowsN(4);
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:d' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'l' })"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: b");

    // an unnamed preselect replaces a named one rather than inheriting its
    // target, so e follows focus. nothing opens between the two messages here,
    // which is what makes the stale target reachable.
    OK(getFromSocket("/dispatch hl.dsp.layout('preselect r class:a')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:b' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('preselect r')"));

    const auto STALE   = getFromSocket("/clients");
    const auto STALE_A = Tests::getAttribute(windowBlock(STALE, "a"), "size");
    const auto STALE_B = Tests::getAttribute(windowBlock(STALE, "b"), "size");

    SPAWN_KITTY("e");
    Tests::waitUntilWindowsN(5);

    const auto UNSTALE = getFromSocket("/clients");
    EXPECT(Tests::getAttribute(windowBlock(UNSTALE, "a"), "size"), STALE_A);
    EXPECT_NOT(Tests::getAttribute(windowBlock(UNSTALE, "b"), "size"), STALE_B);

    // a selector matching nothing is an error, not a silent no-op
    EXPECT_NOT(getFromSocket("/dispatch hl.dsp.layout('preselect r class:nosuchwindow')"), "ok");

    // and a rejected message must leave nothing armed. the selector below is
    // good, but the direction between it and the message name is empty, so if
    // the target were taken before the message was validated, the next window
    // would split a instead of following focus.
    const auto BEFORE = getFromSocket("/clients");
    const auto A_AT   = Tests::getAttribute(windowBlock(BEFORE, "a"), "at");
    const auto A_SIZE = Tests::getAttribute(windowBlock(BEFORE, "a"), "size");
    const auto B_SIZE = Tests::getAttribute(windowBlock(BEFORE, "b"), "size");

    EXPECT_NOT(getFromSocket("/dispatch hl.dsp.layout('preselect  class:a')"), "ok");
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:b' })"));
    SPAWN_KITTY("f");
    Tests::waitUntilWindowsN(6);

    const auto AFTER = getFromSocket("/clients");
    EXPECT(Tests::getAttribute(windowBlock(AFTER, "a"), "at"), A_AT);
    EXPECT(Tests::getAttribute(windowBlock(AFTER, "a"), "size"), A_SIZE);
    EXPECT_NOT(Tests::getAttribute(windowBlock(AFTER, "b"), "size"), B_SIZE);

    // a non-direction is the legacy reset for permanent_direction_override. it
    // drops the pending target along with the direction, and has no window of
    // its own to resolve - so it cannot fail on a selector that matches
    // nothing, and g follows focus rather than the target armed just above.
    OK(getFromSocket("/dispatch hl.dsp.layout('preselect r class:a')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('preselect x class:nosuchwindow')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:b' })"));

    const auto RESET   = getFromSocket("/clients");
    const auto RESET_A = Tests::getAttribute(windowBlock(RESET, "a"), "size");
    const auto RESET_B = Tests::getAttribute(windowBlock(RESET, "b"), "size");

    SPAWN_KITTY("g");
    Tests::waitUntilWindowsN(7);

    const auto UNRESET = getFromSocket("/clients");
    EXPECT(Tests::getAttribute(windowBlock(UNRESET, "a"), "size"), RESET_A);
    EXPECT_NOT(Tests::getAttribute(windowBlock(UNRESET, "b"), "size"), RESET_B);
}

TEST_CASE(dwindlePreselectAcrossWorkspaces) {

    // both halves at once: aim an off-screen workspace's next split from the
    // workspace you are sitting on, then open a window there. the pointer is
    // parked over the window the fallback would otherwise have picked, so this
    // fails if the selector is ignored.

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '5' })"));
    for (auto const& win : {"a", "b"}) {
        SPAWN_KITTY(win);
    }
    Tests::waitUntilWindowsN(2);

    // leave 5 behind, and put a window on 1 to keep an eye on
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    SPAWN_KITTY("c");
    Tests::waitUntilWindowsN(3);

    const auto BEFORE = getFromSocket("/clients");
    const auto A_AT   = Tests::getAttribute(windowBlock(BEFORE, "a"), "at");
    const auto A_SIZE = Tests::getAttribute(windowBlock(BEFORE, "a"), "size");
    const auto B_AT   = Tests::getAttribute(windowBlock(BEFORE, "b"), "at");
    const auto B_SIZE = Tests::getAttribute(windowBlock(BEFORE, "b"), "size");
    const auto C_AT   = Tests::getAttribute(windowBlock(BEFORE, "c"), "at");
    const auto C_SIZE = Tests::getAttribute(windowBlock(BEFORE, "c"), "size");
    EXPECT_NOT(A_AT, std::string{});

    // 5 is not on screen, but its nodes still cover the monitor. park the
    // pointer in the middle of a, so getClosestNode would answer a.
    const auto A_BOX   = attributePair(windowBlock(BEFORE, "a"), "at");
    const auto A_EXTEN = attributePair(windowBlock(BEFORE, "a"), "size");
    OK(getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", A_BOX.first + A_EXTEN.first / 2, A_BOX.second + A_EXTEN.second / 2)));
    const auto CURSOR = getFromSocket("/cursorpos");

    // c exists, but not on 5 - the selector has to answer for the workspace
    // the message was addressed to
    EXPECT_NOT(getFromSocket("/dispatch hl.dsp.layout({ message = 'preselect r class:c', workspace = '5' })"), "ok");

    // now split b - unfocused, off screen, and not the one under the pointer.
    // hyprtester spawns kitty itself rather than through a dispatcher, so d is
    // placed with a window rule where a user would write an exec rule.
    OK(getFromSocket("/dispatch hl.dsp.layout({ message = 'preselect r class:b', workspace = '5' })"));
    OK(getFromSocket("/eval hl.window_rule({ match = { class = 'd' }, workspace = '5 silent' })"));
    SPAWN_KITTY("d");
    Tests::waitUntilWindowsN(4);

    const auto AFTER   = getFromSocket("/clients");
    const auto B_BLOCK = windowBlock(AFTER, "b");
    const auto D_BLOCK = windowBlock(AFTER, "d");
    const auto D_BOX   = attributePair(D_BLOCK, "at");
    const auto D_EXTEN = attributePair(D_BLOCK, "size");

    // d landed on 5 as b's sibling: b kept its corner but not its width, and d
    // shares b's row, starting past b's left edge
    EXPECT_CONTAINS(D_BLOCK, "workspace: 5");
    EXPECT(Tests::getAttribute(B_BLOCK, "at"), B_AT);
    EXPECT_NOT(Tests::getAttribute(B_BLOCK, "size"), B_SIZE);
    EXPECT(D_BOX.second, attributePair(B_BLOCK, "at").second);
    EXPECT(D_EXTEN.second, attributePair(B_BLOCK, "size").second);
    EXPECT(D_BOX.first > attributePair(B_BLOCK, "at").first, true);

    // the pointer's pick was overridden, so a is untouched
    EXPECT(Tests::getAttribute(windowBlock(AFTER, "a"), "at"), A_AT);
    EXPECT(Tests::getAttribute(windowBlock(AFTER, "a"), "size"), A_SIZE);

    // and we never left workspace 1, nor did anything on it move. the pointer
    // is part of that contract: this must run without disturbing the session
    // it is running underneath.
    EXPECT_CONTAINS(getFromSocket("/activeworkspace"), "ID 1");
    EXPECT(getFromSocket("/cursorpos"), CURSOR);

    // the selector is scoped to workspace 5, so a window elsewhere is simply
    // not found. a window that is on 5 but not in its tree is a different
    // refusal, and floating d is the way to reach it.
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set', window = 'class:d' })"));
    EXPECT_NOT(getFromSocket("/dispatch hl.dsp.layout({ message = 'preselect r class:d', workspace = '5' })"), "ok");
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: c");
    EXPECT(Tests::getAttribute(windowBlock(AFTER, "c"), "at"), C_AT);
    EXPECT(Tests::getAttribute(windowBlock(AFTER, "c"), "size"), C_SIZE);
}

TEST_CASE(dwindlePreselectSelectorScopedToWorkspace) {

    // the selector answers for the workspace the message was addressed to. an
    // unscoped query returns whichever match it enumerates first, so with the
    // same class on two workspaces it finds one by enumeration order and
    // rejects it for not being in the addressed tree.
    //
    // the decoy is deliberately created first. spawn it second and an unscoped
    // query reaches the right window by luck, and this stops testing anything.

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    SPAWN_KITTY("dup");
    SPAWN_KITTY("one");
    Tests::waitUntilWindowsN(2);

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '2' })"));
    SPAWN_KITTY("dup");
    SPAWN_KITTY("two");
    Tests::waitUntilWindowsN(4);

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));

    const auto BEFORE    = getFromSocket("/clients");
    const auto DUP2_SIZE = Tests::getAttribute(windowBlock(BEFORE, "dup", 2), "size");
    const auto DUP1_AT   = Tests::getAttribute(windowBlock(BEFORE, "dup", 1), "at");
    const auto DUP1_SIZE = Tests::getAttribute(windowBlock(BEFORE, "dup", 1), "size");
    const auto TWO_SIZE  = Tests::getAttribute(windowBlock(BEFORE, "two", 2), "size");
    EXPECT_NOT(DUP2_SIZE, std::string{});
    EXPECT_NOT(DUP1_SIZE, std::string{});

    // both workspaces have a "dup". address 2's.
    OK(getFromSocket("/dispatch hl.dsp.layout({ message = 'preselect r class:dup', workspace = '2' })"));
    OK(getFromSocket("/eval hl.window_rule({ match = { class = 'new' }, workspace = '2 silent' })"));
    SPAWN_KITTY("new");
    Tests::waitUntilWindowsN(5);

    const auto AFTER = getFromSocket("/clients");

    // 2's dup was split
    EXPECT_CONTAINS(windowBlock(AFTER, "new", 2), "class: new");
    EXPECT_NOT(Tests::getAttribute(windowBlock(AFTER, "dup", 2), "size"), DUP2_SIZE);
    EXPECT(Tests::getAttribute(windowBlock(AFTER, "two", 2), "size"), TWO_SIZE);

    // 1's was not touched, and we never left it
    EXPECT(Tests::getAttribute(windowBlock(AFTER, "dup", 1), "at"), DUP1_AT);
    EXPECT(Tests::getAttribute(windowBlock(AFTER, "dup", 1), "size"), DUP1_SIZE);
    EXPECT_CONTAINS(getFromSocket("/activeworkspace"), "ID 1");
}

TEST_CASE(dwindleMovetorootSelectorIsAuthoritative) {

    // movetoroot is allowed against an addressed workspace because it names
    // the node it moves. that premise only holds if the selector is both
    // scoped to the workspace addressed and authoritative - a selector that
    // matches nothing must fail, not quietly fall back to the focused node.

    const auto targeted = [](const std::string& ws, const std::string& msg) {
        return getFromSocket(std::format("/dispatch hl.dsp.layout({{ message = '{}', workspace = '{}' }})", msg, ws));
    };

    // the decoy on workspace 1 is created first, so an unscoped query finds it
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    SPAWN_KITTY("one");
    SPAWN_KITTY("dup");
    SPAWN_KITTY("uno");
    Tests::waitUntilWindowsN(3);

    // "dup" is not spawned first here: the first window on a workspace is a
    // direct child of root, and movetoroot rightly refuses to move one there
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '2' })"));
    SPAWN_KITTY("two");
    SPAWN_KITTY("dup");
    SPAWN_KITTY("dos");
    Tests::waitUntilWindowsN(6);

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));

    // movetoroot defaults to "stable", which keeps the moved window on the same
    // side of the screen, so its corner alone need not move - compare the box
    const auto boxOf = [](const std::string& clients, const std::string& cls, int ws) {
        const auto BLOCK = windowBlock(clients, cls, ws);
        return Tests::getAttribute(BLOCK, "at") + " " + Tests::getAttribute(BLOCK, "size");
    };

    const auto BEFORE   = getFromSocket("/clients");
    const auto DUP2_BOX = boxOf(BEFORE, "dup", 2);
    const auto DUP1_BOX = boxOf(BEFORE, "dup", 1);
    EXPECT_NOT(DUP2_BOX, std::string{" "});

    // both workspaces have a "dup"; the addressed one is what answers
    OK(targeted("2", "movetoroot class:dup"));

    const auto AFTER = getFromSocket("/clients");
    EXPECT_NOT(boxOf(AFTER, "dup", 2), DUP2_BOX);
    EXPECT(boxOf(AFTER, "dup", 1), DUP1_BOX);

    // a selector matching nothing errors rather than acting on the focused
    // node. workspace 1 is the active one here, so a fallback to focus would
    // succeed and silently restructure it.
    const auto SETTLED = getFromSocket("/clients");
    EXPECT_NOT(targeted("1", "movetoroot class:nosuchwindow"), "ok");
    EXPECT(getFromSocket("/clients"), SETTLED);

    // and the same unaddressed, which is the path every existing config takes
    EXPECT_NOT(getFromSocket("/dispatch hl.dsp.layout('movetoroot class:nosuchwindow')"), "ok");
    EXPECT(getFromSocket("/clients"), SETTLED);

    // the optional third argument still reaches the handler past the selector.
    // both workspaces were built the same way, so a stable move on one and an
    // unstable move on the other have to land differently.
    const auto DUP2_STABLE = boxOf(getFromSocket("/clients"), "dup", 2);
    OK(targeted("1", "movetoroot class:dup unstable"));

    const auto DUP1_UNSTABLE = boxOf(getFromSocket("/clients"), "dup", 1);
    EXPECT_NOT(DUP1_UNSTABLE, DUP1_BOX);    // it moved
    EXPECT_NOT(DUP1_UNSTABLE, DUP2_STABLE); // and not to where stable put it

    // an unnamed movetoroot reads "unstable" as its selector, so it is refused
    // for naming nothing that exists rather than acting on the focused node
    EXPECT_NOT(targeted("1", "movetoroot unstable"), "ok");
}

TEST_CASE(masterRefusesTargetedLayoutMsg) {

    // the opt-in is per receiving algorithm, and the base class refuses. an
    // algorithm that has not opted in gets nothing, whatever its messages are
    // named - which is what lets plugin layouts stay safe by default.

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'master' } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '2' })"));
    for (auto const& win : {"a", "b"}) {
        SPAWN_KITTY(win);
    }
    Tests::waitUntilWindowsN(2);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    SPAWN_KITTY("c");
    Tests::waitUntilWindowsN(3);

    const auto BEFORE = getFromSocket("/clients");

    // orientationleft is the sharp case: it clears fullscreen on the focused
    // window before touching the addressed workspace's orientation
    for (auto const& msg : {"orientationleft", "orientationnext", "swapwithmaster", "focusmaster", "mfact +0.1"}) {
        EXPECT_NOT(getFromSocket(std::format("/dispatch hl.dsp.layout({{ message = '{}', workspace = '2' }})", msg)), "ok");
        EXPECT_NOT(getFromSocket(std::format("/dispatch hl.dsp.layout({{ message = '{}', workspace = '1' }})", msg)), "ok");
    }

    // nothing moved on either workspace
    EXPECT(getFromSocket("/clients"), BEFORE);

    // and master still works normally when no workspace is addressed
    OK(getFromSocket("/dispatch hl.dsp.layout('orientationleft')"));
}

TEST_CASE(dwindleTargetedSelectorScopesFloatingAndTiled) {

    // of the three focus-relative selector forms, "active" is refused for
    // targeted messages, but "floating" and "tiled" are allowed because the
    // workspace constraint makes them answer for the workspace addressed
    // rather than the focused window's - including when no window is focused
    // there at all.

    const auto targeted = [](const std::string& ws, const std::string& msg) {
        return getFromSocket(std::format("/dispatch hl.dsp.layout({{ message = '{}', workspace = '{}' }})", msg, ws));
    };

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '2' })"));
    SPAWN_KITTY("t1");
    SPAWN_KITTY("t2");
    Tests::waitUntilWindowsN(2);

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    SPAWN_KITTY("here");
    Tests::waitUntilWindowsN(3);

    // "tiled" resolves on workspace 2 even though focus is on 1
    const auto BEFORE = getFromSocket("/clients");
    OK(targeted("2", "preselect r tiled"));
    OK(getFromSocket("/eval hl.window_rule({ match = { class = 'landed' }, workspace = '2 silent' })"));
    SPAWN_KITTY("landed");
    Tests::waitUntilWindowsN(4);

    EXPECT_CONTAINS(windowBlock(getFromSocket("/clients"), "landed", 2), "workspace: 2");
    EXPECT(Tests::getAttribute(windowBlock(getFromSocket("/clients"), "here"), "at"), Tests::getAttribute(windowBlock(BEFORE, "here"), "at"));
    EXPECT_CONTAINS(getFromSocket("/activeworkspace"), "ID 1");

    // and "floating" finds nothing on a workspace that has no floating window,
    // rather than answering from the focused window's workspace
    EXPECT_NOT(targeted("2", "preselect r floating"), "ok");
}
