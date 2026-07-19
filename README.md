libUIOHook: Cross-platform keyboard and mouse hooking from userland. 
====================================================================

![build-binaires](https://github.com/kwhat/libuiohook/workflows/build-binaires/badge.svg)

## Compiling
Prerequisites: 
 * [cmake](https://cmake.org)
 * gcc, clang or msvc
 * Linux dependencies:
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
