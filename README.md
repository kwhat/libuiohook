libuiohook
==========

A multi-platform C library to provide global keyboard and mouse hooks from userland.

## Compiling
Prerequisites: 
 * autotools
 * pkg-config 
 * libtool 
 * gcc, clang or msys2/mingw32
```
./bootstrap.sh
./configure
make && make install
```
### Note for windows compilation
When using msys2/cygwin make sure to install dos2unix and convert the line endings in ```configure.ac``` and ```configure```
before running the above.

## Usage
* [Hook Demo](demo/demo_hook.c)
* [Async Hook Demo](demo/demo_hook_async.c)
* [Event Post Demo](demo/demo_post.c)
* [Properties Demo](demo/demo_properties.c)
* [Public Interface](include/uiohook.h)
* Please see the man pages for function documentation.
