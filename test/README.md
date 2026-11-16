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
| `common.header` | All | Public macro precedence in mask operations, arithmetic, and keycode comparisons |
| `windows.unicode_table` | Windows | Owned keyboard-table decoder with fixed x86, x64, and WOW64 byte fixtures |
| `windows.unicode_helper` | Windows x64 | Real cache/loader code with mocked registry, DLL, memory-read, layout, and key-state APIs |
| `darwin.runloop_queue` | macOS | Production request queue with controlled pthread callers and a mock scheduler |
| `darwin.runloop_helper` | macOS | Actual Core Foundation source, main-thread execution, timeout, stop, and restart without an input hook |
| `common.wheel_helper` | All | Production scaling, fractional accumulation, range limits, and invalid wheel data |
| `common.logger` | All | Production logger with a mock callback |
| `common.numeric_helper` | All | Signed/unsigned 16-bit conversion boundaries and saturation |
| `darwin.numeric_conversion` | macOS | Double-to-16-bit clamps, overflow, NaN/infinity, and fractional truncation |
| `evdev.post_event` | Linux/evdev | Key posting before hook startup, reused initialization, and posting after cleanup with mock uinput |
| `evdev.input_helper` | Linux/evdev | Production key mappings, modifier masks, and pointer tracking with mock desktop providers |
| `windows.keycode_helper` | Windows | Production key mappings and modifier masks with fixed fixtures |
| `darwin.keycode_helper` | macOS | Production Darwin key mappings, modifier masks, and drag state with fixed fixtures |
| `evdev.relative_mouse` | Linux/evdev | Real mouse dispatch and pointer tracking with mock display bounds |
| `evdev.wheel` | Linux/evdev | Real wheel capture/posting with mock desktop and uinput calls |
| `windows.wheel` | Windows | Real wheel capture/posting with mock settings queries and SendInput |
| `darwin.wheel` | macOS | Local Core Graphics events and real capture/posting with intercepted CGEventPost |
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

The common wheel helper runs on every host. It checks scale conversion, invalid deltas/types/directions,
negative and fractional movement (including ten one-tenth steps), and capture
saturation. Evdev tests round-trip multiple detents on both axes, retain separate
fractions, and propagate mocked write failures. Touchpad tests also verify wheel rotation and scale.

Windows tests intercept the SDK import pointers for `SendInput` and
`SystemParametersInfo`. They cover native wheel capture, horizontal posting,
line/page/disabled settings, large setting values, query failure fallback,
rescaling, and posting failures. macOS tests construct local Core Graphics
objects, cover diagonal and fractional line capture from continuous devices, and intercept `CGEventPost`
so no input is injected. They verify unit-scroll types, horizontal posting,
fractional accumulation, and invalid deltas. These native suites need their
platform SDKs; the Linux test run only executes the shared arithmetic and evdev
coverage.

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

## Numeric conversion checks

`numeric_helper_test.c` checks exact limits, values just outside those limits,
negative-to-unsigned conversion, and extreme signed 64-bit inputs. The Darwin
numeric conversion fixture runs only on Apple builds and checks finite-value
truncation, signed and unsigned 16-bit boundaries, extreme magnitudes, NaN,
and infinity. Floating-point declarations and implementations are guarded by
`__APPLE__` in the shared numeric helper. On Apple, `clamp_int16` and
`clamp_uint16` accept `double`; on other platforms they accept `int64_t`.

## Posting initialization checks

`evdev/post_event_test.c` posts arrow-key press/release events before a hook
initializes translation, verifies that later initialization preserves pointer
state, and posts again after cleanup. It uses the real keycode helper and
libevdev device template with mocked uinput creation, writes, and destruction.

## Windows Unicode translation

`unicode_helper.c` owns layout discovery and the cache; `unicode_table.c` imports
and translates keyboard DLL data without Windows headers. Fixtures use explicit
ABI offsets and table strides, including padded rows, instead of native C
pointer structs. Cases cover x86 pointers, x64 pointers above 4 GB, WOW64's
8-byte fields at low addresses, invalid ranges/strides/modifier maps, optional
version-0 fields, truncated/unterminated tables, and missing dead-key rows.

Translation cases cover layout-specific modifier columns, Caps Lock/Shift/AltGr,
attribute bits, Caps Lock bounds, dead-key composition and chaining, unmatched
accents, null dead-key tables, ligatures with surrogate pairs, and exact/short
UTF-16 capacities. Imports own their data: overwriting the fixture afterward
must not change translation. Short buffers leave output and dead-key state
unchanged.

The Windows x64 cache test supplies link-time SDK import-pointer mocks. It
checks layout-switch state reset, cache-head removal, reuse of retained entries,
new layout loading, registry failure cleanup, unreadable image failure cleanup,
and immediate DLL unloading after import. Actual WOW64/ARM64 emulation and
non-US layouts still need native Windows validation; byte fixtures do not
establish that OS integration works on those machines.

## macOS main-runloop synchronization

The queue tests compile the production pthread queue without Core Foundation.
The queue detects main-thread callers using Darwin's `pthread_main_np()`. They
exercise independent completion for concurrent callers, cancellation while the
scheduler is not running, preservation of a running request until its callback
finishes, stale-session rejection, direct main-thread calls, and timeout removal
of middle and final queue entries. Completion broadcasts wake other callers as
well; each checks its own completion predicate before returning.

The native helper test creates a real Core Foundation source and verifies that
worker work executes on the main thread. It also leaves that loop unserviced,
checks that the pending request times out without a late callback, and exercises
stop, idempotent cleanup, and restart. It needs macOS CI; neither test installs
an event tap or injects input.
