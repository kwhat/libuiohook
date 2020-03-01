libuiohook
==========

A multi-platform C library to provide global keyboard and mouse hooks from userland.

## Compiling
Prerequisites: 
 * [cmake](https://cmake.org)
 * pkg-config
 * gcc, clang or msvc
 * X11, XCB, XKB Common when building for X11
```
$ git clone https://github.com/kwhat/libuiohook
$ cd libuiohook
$ mkdir build && cd build
$ cmake ..
$ make
```

## Usage
* [Hook Demo](demo/demo_hook.c)
* [Async Hook Demo](demo/demo_hook_async.c)
* [Event Post Demo](demo/demo_post.c)
* [Properties Demo](demo/demo_properties.c)
* [Public Interface](include/uiohook.h)
* Please see the man pages for function documentation.
