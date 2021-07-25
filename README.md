### Status

Things may work for you, or may not.  Things may never work because of huge differences between Linux and Windows.  Or things may work in future, if you report the problem on [GitHub](https://github.com/rmyorston/busybox-w32) or [GitLab](https://gitlab.com/rmyorston/busybox-w32).  If you don't have an account on one of those or you'd prefer to communicate privately you can email [rmy@pobox.com](mailto:rmy@pobox.com).

Additional information and downloads of precompiled binaries are available from [frippery.org](https://frippery.org/busybox).

### Building

You need a MinGW compiler and a POSIX environment.  I cross-compile on Linux.  On Fedora the following should pull in everything required:

`dnf install gcc make ncurses-devel perl-Pod-Html`

`dnf install mingw64-gcc mingw64-windows-default-manifest` (for a 64-bit build)

`dnf install mingw32-gcc mingw32-windows-default-manifest` (for a 32-bit build)

On Microsoft Windows you can install MSYS2 and a 64-bit toolchain by following [these instructions](https://www.msys2.org/#installation).  To obtain a 32-bit toolchain run:

`pacman -S --needed mingw-w64-i686-toolchain`

Run `mingw64.exe` or `mingw32.exe` from the installation directory.

On either Linux or Windows the commands `make mingw64_defconfig` or `make mingw32_defconfig` will pick up the default configuration.  You can then customize your build with `make menuconfig` (Linux only) or by editing `.config`, if you know what you're doing.

Then just `make` or `make CROSS_COMPILE=""` on Windows.

### Limitations

 - Use forward slashes in paths:  Windows doesn't mind and the shell will be happier.
 - Windows paths are different from Unix ([more detail](https://frippery.org/busybox/paths.html)):
   * Absolute paths: `c:/path` or `//host/share`
   * Relative to current directory of other drive: `c:path`
   * Relative to current root (drive or share): `/path`
   * Relative to current directory of current root (drive or share): `path`
 - Handling of users, groups and permissions is totally bogus.  The system only admits to knowing about the current user and always returns the same hardcoded uid, gid and permission values.
 - Some crufty old Windows code (Windows XP, cmd.exe) doesn't like forward slashes in environment variables.  The -X shell option (which must be the first argument) prevents busybox-w32 from changing backslashes to forward slashes.  If Windows programs don't run from the shell it's worth trying it.
 - If you want to install 32-bit BusyBox in a system directory on a 64-bit version of Windows you should put it in `C:\Windows\SysWOW64`, not `C:\Windows\System32` as you might expect.  On 64-bit systems the latter is for 64-bit binaries.
 - The system tries to detect the best way to handle ANSI escape sequences for the terminal being used.  If this doesn't work you can try setting the environment variable `BB_SKIP_ANSI_EMULATION=1` to force the use of literal ANSI escapes or `BB_SKIP_ANSI_EMULATION=0` to emulate them using the Windows console API.
 - It's possible to obtain pseudo-random numbers using `if=/dev/urandom` as the input file to `dd`.  The same emulation of `/dev/urandom` is used internally by the `shred` utility and to support https in `wget`.  Since the pseudo-random number generator isn't being seeded with sufficient entropy the randomness shouldn't be relied on for any serious use.
