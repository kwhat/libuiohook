# Tests

Enable the unit tests and run them with CTest:

```sh
cmake -S . -B build -DENABLE_TEST=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

For a multi-configuration generator, add `--config Release` to the build command
and `-C Release` to CTest. Assertions remain active in Release builds. Failed
assertions print the source location and expression and return a nonzero exit
status to CTest. The old MinUnit runner and live-display initialization are gone.

Tests are not built by default.

## Portability

| Tests | Platforms | Dependencies exercised |
| --- | --- | --- |
| `common.logger` | All | Production logger with a mock callback |
| `evdev.input_helper` | Linux/evdev | Production key mappings, modifier masks, and pointer tracking with mock desktop providers |
| `windows.keycode_helper` | Windows | Production key mappings and modifier masks with fixed fixtures |
| `darwin.keycode_helper` | macOS | Production Darwin key mappings, modifier masks, and drag state with fixed fixtures |
| `evdev.properties.*` | Linux/evdev | Production property queries with mocked X11 and Wayland responses |
| `evdev.touchpad_translation` | Linux/evdev | Mock libinput events, mock uinput writes, real mouse dispatch and pointer/modifier state |
| `evdev.touchpad_initialization` | Linux/evdev | Mock uinput creation and libinput setup, with real helper cleanup |
| `package.*_consumer` | All | Independent CMake consumers of build-tree and installed packages |

Common logger and package-consumer tests run on all platforms. Platform-specific
fixtures run only for the platform being built: Windows tests on Windows,
Darwin tests on macOS, and evdev tests on Linux. Each uses its platform SDK
and headers. Native OS property providers and event dispatch still need their
own platform-specific mocked suites. Mapping tests do not cover those OS
interactions.

Unit targets compile production sources directly and do not link the production
`uiohook` library. The evdev property object's constructor and destructor call mock
providers rather than opening a display or loading native helpers. Windows and
Darwin have dedicated `windows/keycode_helper_test.c` and
`darwin/keycode_helper_test.c` files and CTest targets. Neither calls OS helper
initialization.

## Darwin isolation

Darwin mapping, modifier, and drag-state tests compile
`src/darwin/keycode_helper.c`, `src/darwin/mouse_helper.c`, and
`src/input_state.c` directly on macOS, using Apple SDK headers. Fixtures cover
known keys, unknown keys, all mapped native keycodes, and independent state.
Native keycode zero is valid (A); unknown reverse mappings return `0xFF`.
Accessibility, keyboard layouts, Unicode translation, and run-loop integration
remain outside this pure helper suite and require macOS testing.

## Windows isolation

Windows mapping and modifier tests compile `src/windows/keycode_helper.c`,
`src/input_state.c`, and their logger dependency directly. They do not compile the OS-dependent
`input_helper.c`, query live keyboard layouts, access the registry, or load
keyboard-layout DLLs. Windows SDK headers supply virtual-key constants and types.

Fixtures check named keys, function keys, left/right modifiers, keypad Enter,
keypad navigation, and unknown keys. Forward/reverse consistency is checked for
all mapped Windows virtual keys with and without the extended-key flag. Explicit
fixtures also verify the expected native values so round trips cannot hide a
consistently incorrect mapping.

Layout discovery, Unicode/dead-key handling, and modifier initialization from
`GetKeyState` remain in `input_helper.c`. Testing those paths will require mock
Windows API providers; they are not covered by the pure helper suite.

## evdev isolation

`evdev/mock_desktop.c` implements the existing X11 and Wayland provider interfaces.
It supplies fake repeat settings, pointer controls, click-time resources, monitor
layouts, a pointer seed, and a self-contained XKB keymap string. No X server,
Wayland compositor, input device, or elevated permissions are required. The real
xkbcommon library parses the fixture; key conversion methods are not mocked.

Property cases cover successful queries, missing displays, invalid responses,
Wayland preference, X11 fallback, and monitor layouts including negative origins.
Input-helper cases cover known mappings, unmapped keys, evdev/XKB offsets,
independent modifier bits, keymap lookup, and pointer bounds.

`evdev/touchpad_helper_test.c` compiles `src/evdev/touchpad_helper.c` as a
separate source file and accesses it through `touchpad_helper.h`. Mock libinput
and uinput functions are supplied at link time; the test does not link libinput,
include implementation files, or rename functions with preprocessor macros.
The helper's state is opaque to the test.

The tests exercise fractional movement, independent state for multiple touchpads,
vertical/horizontal scrolling, consumption, observation mode, device removal,
dispatch errors, and press/drag/release dispatch. The real mouse dispatcher and
pointer/modifier tracker run against the mock desktop providers. The button
callback is a test implementation of the callback supplied by the hook manager.

Initialization cases cover successful creation, descriptor and virtual-device
queries, missing or inaccessible uinput, failed libinput context/device
initialization, and resource cleanup. The newer-scroll-event check is enabled
only with libinput 1.19 or later; older headers still build and run the rest of
the suite. The tests use `/dev/null` through libinput's captured open/close
callbacks to verify observation mode and failed-grab fallback without hardware.

## Package and CI checks

The package checks configure and link an independent consumer using
`find_package(uiohook)`. One uses the build-tree config; the other installs into a
private prefix inside the test build directory before configuring the consumer.
Neither check changes the system installation or runs the consumer executable.

`.github/workflows/tests.yml` builds and runs CTest on native Linux, Windows, and
macOS runners. Linux packaging uses `ubuntu-latest`; ARM32, ARM64, and x86
cross builds install target development packages and select native GLib build
tools explicitly. They retain gold linker selection and do not execute target
binaries. Unit tests run on pushes to every branch without a separate PR trigger.

## Hardware checks

Run `demo_hook` separately to verify real device discovery, permissions, grabs,
virtual-device forwarding, libinput contact interpretation, and desktop behavior.
These checks are not part of the automated unit suite and must be performed on
each supported OS. Windows and macOS native builds have not been verified by the
Linux test run.

## Helper ownership

All backends use `src/input_state.c` for modifier and mouse-button mask state.
Keycode modules no longer own those masks. Darwin drag state lives in
`darwin/mouse_helper.c`; evdev pointer queries and position tracking live in
`evdev/mouse_helper.c`, along with its drag state.

The evdev keycode module owns its mapping table, XKB context, keymap, compose
state, translation functions, and keyboard setup/cleanup. Its `input_helper.c`
coordinates keyboard and pointer initialization and supplies event timestamps.
Existing evdev fixtures exercise those modules together through the same mock
desktop providers. Windows and Darwin native Unicode/layout integration remains
in their input helpers.
