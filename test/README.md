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
| `<backend>.input_helper` | evdev, Windows, macOS | Production key mappings and modifier masks with fixed native/virtual code fixtures |
| `evdev.properties.*` | Linux/evdev | Production property queries with mocked X11 and Wayland responses |
| `evdev.touchpad_translation` | Linux/evdev | Mock libinput events, mock uinput writes, real mouse dispatch and modifier state |
| `evdev.touchpad_initialization` | Linux/evdev | Mock uinput creation and libinput setup, with real hook cleanup |
| `package.*_consumer` | All | Independent CMake consumers of build-tree and installed packages |

CTest and the assertion helper are portable. Native keycodes, native event
structures, and OS property providers differ, so backend fixtures must be built
with the corresponding SDK on each platform. The Linux run does not establish
that Windows or macOS works. Windows and macOS still need their own mocked
system-property and event-dispatch suites; the shared mapping tests do not cover
those OS interactions.

Unit targets compile production sources directly and do not link the production
`uiohook` library. The evdev property object's constructor and destructor call mock
providers rather than opening a display or loading native helpers. Windows and macOS helper tests
only call mapping and modifier functions, never helper initialization. They still
link the platform SDK libraries needed to compile the helper object.

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

The touchpad test currently includes `input_hook.c` to reach its private functions
and substitutes libinput calls with test functions. It exercises movement,
vertical/horizontal scrolling, consumption, observation mode, and
press/drag/release dispatch. Initialization cases cover successful creation,
missing or inaccessible uinput, failed libinput context/device initialization,
and resource cleanup. The newer-scroll-event check is enabled only with libinput
1.19 or later; older headers still build and run the rest of the suite. It uses
`/dev/null` to verify that a failed grab falls back to observation without needing
an input device. This source-inclusion technique is confined to the evdev test;
extracting the translator into a separate internal module would remove it.

## Package and CI checks

The package checks configure and link an independent consumer using
`find_package(uiohook)`. One uses the build-tree config; the other installs into a
private prefix inside the test build directory before configuring the consumer.
Neither check changes the system installation or runs the consumer executable.

`.github/workflows/tests.yml` builds and runs CTest on native Linux, Windows, and
macOS runners. Cross-compiled packaging jobs install the target architecture's
libinput development package but do not try to execute foreign binaries.

## Hardware checks

Run `demo_hook` separately to verify real device discovery, permissions, grabs,
virtual-device forwarding, libinput contact interpretation, and desktop behavior.
These checks are not part of the automated unit suite and must be performed on
each supported OS. Windows and macOS native builds have not been verified by the
Linux test run.
