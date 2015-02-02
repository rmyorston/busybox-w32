### Status

This port is used in production at Tigress.  Things may work for you, or may not.  Things may never work because of huge differences between Linux and Windows.  Or things may work in future, if you report the problem to https://github.com/rmyorston/busybox-w32.

### Building

You need a MinGW compiler and a POSIX environment (so that `make menuconfig` works).  I cross compile from Linux, but MSYS or Cygwin should be OK.

To start, run `make mingw32_defconfig`.  You can then customize your build with `make menuconfig`. Alternatively, to build using Open Watcom, run `make watcom386_win32_defconfig`. For more details about Open Watcom
, see README.watcom.

In particular you may need to adjust the compiler by going to Busybox Settings -> Build Options -> Cross Compiler Prefix

Then just `make`.

### Limitations

 - Use forward slashes in paths:  Windows doesn't mind and the shell will be happier.
 - Don't do wild things with Windows drive or UNC notation.
 - tar doesn't support seamless compression/decompression:  use a pipeline to a compressor/decompressor.
