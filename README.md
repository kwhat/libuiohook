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
$ make && sudo make install
```
<<<<<<< HEAD
### Note for windows compilation
When using msys2/cygwin make sure to install dos2unix and convert the line endings in ```configure.ac``` and ```configure```
before running the above.
<<<<<<< HEAD
=======
After running ``bootsrap.sh`` and ``configure`` you can point [cmake](https://cmake.org) to the folder containting this repository and let it generate project files for Visual Studio.
>>>>>>> 1ad123d... Formed a proper sentence
=======
>>>>>>> 3a1ff5f... Update README.md

## Usage
* [Hook Demo](demos/demo_hook.c)
* [Async Hook Demo](demos/demo_hook_async.c)
* [Event Post Demo](demos/demo_post.c)
* [Properties Demo](demos/demo_properties.c)
* [Public Interface](include/uiohook.h)
* Please see the man pages for function documentation.
