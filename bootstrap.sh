#!/bin/sh

if [ -d "/usr/local/share/aclocal" ]; then
    include="-I/usr/local/share/aclocal"
fi

if [ -d "/opt/local/share/aclocal" ]; then
    include="-I/opt/local/share/aclocal ${include}"
fi

autoreconf --install --verbose --force ${include}
