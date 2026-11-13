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
| `common.wheel_helper` | All | Production scaling, fractional accumulation, range limits, and invalid wheel data |
| `common.logger` | All | Production logger with a mock callback |
| `evdev.input_helper` | Linux/evdev | Production key mappings, modifier masks, and pointer tracking with mock desktop providers |
| `windows.keycode_helper` | Windows | Production key mappings and modifier masks with fixed fixtures |
| `darwin.keycode_helper` | All | Production Darwin key mappings, modifier masks, and drag state with fixed fixtures |
| `evdev.wheel` | Linux/evdev | Real wheel capture/posting with mock desktop and uinput calls |
| `windows.wheel` | Windows | Real wheel capture/posting with mock settings queries and SendInput |
| `darwin.wheel` | macOS | Local Core Graphics events and real capture/posting with intercepted CGEventPost |
| `evdev.relative_mouse` | Linux/evdev | Real mouse dispatch and pointer tracking with mock display bounds |
| `evdev.properties.*` | Linux/evdev | Production property queries with mocked X11 and Wayland responses |
| `evdev.touchpad_translation` | Linux/evdev | Mock libinput events, mock uinput writes, real mouse dispatch and pointer/modifier state |
| `evdev.touchpad_initialization` | Linux/evdev | Mock uinput creation and libinput setup, with real helper cleanup |
| `package.*_consumer` | All | Independent CMake consumers of build-tree and installed packages |

CTest and the assertion helper are portable. Windows keycode tests require
Windows SDK headers; Darwin's pure helper needs no Apple SDK and runs on every
host. Native OS property providers and event dispatch still need their own
platform-specific mocked suites. Mapping tests do not cover those OS interactions.

Unit targets compile production sources directly and do not link the production
`uiohook` library. The evdev property object's constructor and destructor call mock
providers rather than opening a display or loading native helpers. Windows and
Darwin have dedicated `windows/keycode_helper_test.c` and
`darwin/keycode_helper_test.c` files and CTest targets. Neither calls OS helper
initialization.

## Darwin isolation

Darwin mapping, modifier, and drag-state tests compile
`src/darwin/keycode_helper.c` directly, without Apple frameworks. Fixtures cover
known keys, unknown keys, all mapped native keycodes, and independent state.
Native keycode zero is valid (A); unknown reverse mappings return `0xFF`.
Accessibility, keyboard layouts, Unicode translation, and run-loop integration
remain outside this pure helper suite and require macOS testing.

## Windows isolation

Windows mapping and modifier tests compile `src/windows/keycode_helper.c` and
its logger dependency directly. They do not compile the OS-dependent
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
independent modifier bits, keymap lookup, and pointer bounds. Relative-motion
cases cover signed deltas, dragging, absolute-coordinate clamping at screen
edges, delta saturation, consumption, and zero deltas on button/click events.
Touchpad cases also verify that fractional and observation-mode motion populate
`dx` and `dy`.

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

## Wheel tests

The common wheel helper runs on every host. It checks scale conversion without
applying `amount` a second time, zero amounts, invalid deltas/types/directions,
negative and fractional movement (including ten one-tenth steps), and capture
saturation. Evdev tests round-trip multiple detents on both axes, retain separate
fractions, and propagate mocked write failures. Touchpad tests also verify the
reported amount.

Windows tests intercept the SDK import pointers for `SendInput` and
`SystemParametersInfo`. They cover native wheel capture, horizontal posting,
line/page/disabled settings, large setting values, query failure fallback,
rescaling, and posting failures. macOS tests construct local Core Graphics
objects, cover diagonal and precise pixel capture, and intercept `CGEventPost`
so no input is injected. They verify line/pixel units, horizontal posting,
fractional accumulation, and invalid deltas. These native suites need their
platform SDKs; the Linux test run only executes the shared arithmetic and evdev
coverage.

## Package and CI checks

The package checks configure and link an independent consumer using
`find_package(uiohook)`. One uses the build-tree config; the other installs into a
private prefix inside the test build directory before configuring the consumer.
Neither check changes the system installation or runs the consumer executable.

`.github/workflows/tests.yml` builds and runs CTest on native Linux, Windows, and
macOS runners. Linux CI uses Ubuntu 26.04 LTS explicitly. ARM64 packaging uses a
native `ubuntu-26.04-arm` runner; ARM32 and x86 remain cross builds and do not try
to execute foreign binaries. ARM32 uses a separately resolved target sysroot so ports-mirror libraries do not
have to match packages installed on the x86 host. Its download-only apt state is
isolated from the runner, and target package scripts/tools are never executed.
The x86 cross build preserves the runner's native repositories, adds an i386-only
Ubuntu source, and selects host GLib generators explicitly. Both cross-build
setups retain the Ubuntu security repositories.

## Hardware checks

Run `demo_hook` separately to verify real device discovery, permissions, grabs,
virtual-device forwarding, libinput contact interpretation, and desktop behavior.
These checks are not part of the automated unit suite and must be performed on
each supported OS. Windows and macOS native builds have not been verified by the
Linux test run.
