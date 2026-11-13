libUIOHook: Cross-platform keyboard and mouse hooking from userland. 
====================================================================

![build-binaires](https://github.com/kwhat/libuiohook/workflows/build-binaires/badge.svg)

## Compiling
Prerequisites: 
 * [cmake](https://cmake.org)
 * gcc, clang or msvc
 * Linux dependencies:
   * libinput-dev
   * libevdev-dev
   * libudev-dev
   * libwayland-dev
   * libwayland-bin _(provides `wayland-scanner`)_
   * wayland-protocols
   * libx11-dev
   * libx11-xcb-dev
   * libxinerama-dev
   * libxkbcommon-dev
   * libxkbcommon-x11-dev

   X11 and Wayland are used at runtime only if available, so the library still loads and
   runs on a pure X11, pure Wayland, or headless system.

```
$ git clone https://github.com/kwhat/libuiohook
$ cd libuiohook
$ mkdir build && cd build
$ cmake -S .. -D BUILD_SHARED_LIBS=ON -D BUILD_DEMO=ON -DCMAKE_INSTALL_PREFIX=../dist
$ cmake --build . --parallel 2 --target install  
```

### Configuration

|           | option                        | description            | default |
| --------- | ----------------------------- | ---------------------- | ------- | 
| __all__   | BUILD_DEMO:BOOL               | demo applications      | OFF     |
|           | BUILD_SHARED_LIBS:BOOL        | shared library         | ON      |
|           | ENABLE_TEST:BOOL              | testing                | OFF     |
|           | USE_EPOCH_TIME:BOOL           | unix epch event times  | OFF     |
| __OSX__   | USE_APPLICATION_SERVICES:BOOL | framework              | ON      |
|           | USE_IOKIT:BOOL                | framework              | ON      |
|           | USE_APPKIT:BOOL               | obj-c api              | ON      |
| __Win32__ |                               |                        |         |
| __Linux__ | USE_EVDEV:BOOL                | generic input driver   | ON      |
| __*nix__  | USE_XF86MISC:BOOL             | xfree86-misc extension | OFF     |
|           | USE_XINERAMA:BOOL             | xinerama library       | ON      |
|           | USE_XRANDR:BOOL               | xrandt extension       | OFF     |
|           | USE_XT:BOOL                   | x toolkit extension    | ON      |

## Usage
* [Hook Demo](demo/demo_hook.c)
* [Async Hook Demo](demo/demo_hook_async.c)
* [Event Post Demo](demo/demo_post.c)
* [Properties Demo](demo/demo_properties.c)
* [Public Interface](include/uiohook.h)
* Please see the man pages for function documentation.

### Linux touchpads

Touchpads are processed with libinput and forwarded through a virtual mouse.
This requires read access to the input device. Forwarding and event consumption
also require permission to grab it and write access to `/dev/uinput`. If the virtual
mouse cannot be created or the device cannot be grabbed, the hook observes events
without grabbing the touchpad; desktop input continues normally, but consumption
is unavailable. If initialization fails, the physical device is released.
Single-finger movement, physical clicks, tap-to-click, dragging, and two-finger
vertical/horizontal scrolling are translated into the existing mouse API.
Scrolling currently accumulates into wheel steps (15 libinput scroll units per step).

The hook uses its own libinput defaults, with tapping and two-finger scrolling
requested where supported. Desktop touchpad settings are not imported. Pinch and
swipe gestures are not forwarded to the desktop while the touchpad is grabbed.
Pointer coordinates use the existing relative-motion tracker and may differ from
the desktop cursor because the virtual mouse can receive further acceleration.

For a hardware smoke test, run `build/demo_hook` with the required device access.
Check movement, tap and physical clicks, dragging, both scroll directions, event
consumption in your callback, and normal touchpad operation after stopping the hook.

Hardware-independent unit tests (including touchpad translation) can be run with:

```sh
cmake -S . -B build -DENABLE_TEST=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

See [test/README.md](test/README.md) for mock boundaries and platform coverage.

### Enabling uinput on Linux

Run the setup commands below in a root shell. To enable virtual-device forwarding
and event consumption, load the `uinput` kernel module:

```sh
modprobe uinput
```

If the module is unavailable, enable `CONFIG_INPUT_UINPUT` in your kernel or
install your distribution's package containing the module. On systems using
`modules-load.d`, load it automatically at boot with:

```sh
echo uinput > /etc/modules-load.d/libuiohook.conf
```

Give the `input` group access to `/dev/uinput` by creating a udev rule:

```sh
echo 'KERNEL=="uinput", MODE="0660", GROUP="input"' > /etc/udev/rules.d/99-libuiohook.rules
udevadm control --reload-rules
udevadm trigger --action=change --subsystem-match=misc --sysname-match=uinput
udevadm settle
```

Reloading udev rules does not update existing devices, so the targeted trigger
applies the new permissions to an already loaded uinput device.
See the [udevadm documentation](https://www.freedesktop.org/software/systemd/man/latest/udevadm.html).

If the account running the application is not already in the `input` group, add
it below, replacing `your_username` with that account's name. Log out and back in
to that account before restarting the application:

```sh
usermod -aG input your_username
```

From the account running the application, check the device permissions and active
groups:

```sh
ls -l /dev/uinput
id -nG
```

The device should be owned by group `input` with group read/write permissions
(`crw-rw----`). The application also needs read access to the physical
`/dev/input/event*` devices, which commonly belong to the `input` group.

### macOS main-runloop requirements

Keyboard Unicode translation and system-key decoding run on the main thread.
Applications that run the hook on a worker thread must continue servicing the
main runloop. Both conversions use a shared request queue with a fixed Core
Foundation source registered in common modes. A request that has not started
within 250 ms is withdrawn and produces no translated characters/system-key
data; it cannot later access the caller's event or buffer. A conversion already
running finishes before its caller returns.

Call `hook_stop()` before joining the hook thread. Stopping cancels queued
translations before stopping the hook runloop, allowing shutdown even when the
main thread is no longer pumping events. Calls made on the main thread execute
directly. Restart creates a new queue generation so an old source cannot execute
or cancel new work.
