### Status

Things may work for you, or may not.  Things may never work because of huge differences between Linux and Windows.  Or things may work in future, if you report the problem to https://github.com/rmyorston/busybox-w32.  If you don't have a GitHub account you can email me: rmy@pobox.com.

### Building

You need a MinGW compiler and a POSIX environment (so that `make menuconfig` works).  I cross-compile on Linux.  On Fedora or RHEL/CentOS+EPEL installing mingw32-gcc (32-bit build) or mingw64-gcc (64-bit build) will pull in everything needed.

To start, run `make mingw32_defconfig` or `make mingw64_defconfig`.  You can then customize your build with `make menuconfig`.

In particular you may need to adjust the compiler by going to Busybox Settings -> Build Options -> Cross Compiler Prefix

Then just `make`.

### Limitations

 - Use forward slashes in paths:  Windows doesn't mind and the shell will be happier.
 - Don't do wild things with Windows drive or UNC notation.
 - Wildcard expansion is disabled by default, though it can be turned on at compile time.  This only affects command line arguments to the binary:  the BusyBox shell has full support for wildcards.
 - Handling of users, groups and permissions is totally bogus.  The system only admits to knowing about the current user and always returns the same hardcoded uid, gid and permission values.
 - Some crufty old Windows code (Windows XP, cmd.exe) doesn't like forward slashes in environment variables.  The -X shell option (which must be the first argument) prevents busybox-w32 from changing backslashes to forward slashes.  If Windows programs don't run from the shell it's worth trying it.
 - If you want to install 32-bit BusyBox in a system directory on a 64-bit version of Windows you should put it in `C:\Windows\SysWOW64`, not `C:\Windows\System32` as you might expect.  On 64-bit systems the latter is for 64-bit binaries.
 - ANSI escape sequences are emulated by converting to the equivalent in the Windows console API.  Setting the environment variable `BB_SKIP_ANSI_EMULATION` will cause ANSI escapes to be passed to the console without emulation.  This may be useful for Windows consoles that support ANSI escapes (e.g. ConEmu).
 - It's possible to obtain pseudo-random numbers using `if=/dev/urandom` as the input file to `dd`.  The same emulation of `/dev/urandom` is used internally by the `shred` utility and to support https in `wget`.  Since the pseudo-random number generator isn't being seeded with sufficient entropy the randomness shouldn't be relied on for any serious use.
