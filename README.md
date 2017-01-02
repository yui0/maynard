# maynard

This is a desktop shell client for Weston based on GTK. It was based
on weston-gtk-shell, a project by Tiago Vignatti.

License information can be found in the LICENSE file in the same
directory as this README.

![Screenshot](maynard.jpg)

## Features

- Lightweight and minimalistic

## Installation

To make maynard

```Bash
./autogen.sh
./configure --prefix=/usr
make CC=clang
```

## FAQ

- GLib-GIO-ERROR **: Settings schema 'org.berry.maynard' is not installed

```
/usr/bin/glib-compile-schemas /usr/share/glib-2.0/schemas/
```
