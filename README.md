### Status

Things may work for you, or may not.  Things may never work because of huge differences between Linux and Windows.  Or things may work in future, if you report the problem on [GitHub](https://github.com/rmyorston/busybox-w32) or [GitLab](https://gitlab.com/rmyorston/busybox-w32).  If you don't have an account on one of those or you'd prefer to communicate privately you can email [rmy@pobox.com](mailto:rmy@pobox.com).

Additional information is available from the [BusyBox for Windows](https://frippery.org/busybox/index.html) web page.  In particular:

- There are [downloads](https://frippery.org/busybox/index.html#downloads) of precompiled binaries for i686, x86_64 and aarch64.
- Release notes for the [current](https://frippery.org/busybox/release-notes/current.html) and [previous](https://frippery.org/busybox/release-notes/index.html) releases are available.

### Building

You need a MinGW toolchain and a POSIX environment.  I cross-compile on Linux.  On Fedora the following should pull in everything required:

`dnf install gcc make ncurses-devel perl-Pod-Html`

`dnf install mingw64-gcc` (for a 64-bit build)

`dnf install mingw32-gcc` (for a 32-bit build)

On Microsoft Windows you can install [w64devkit](https://github.com/skeeto/w64devkit/releases).  Get the `-i686` variant for a 32-bit build.  Unzip the file and run `w64devkit/w64devkit.exe`.

On either Linux or Windows the commands `make mingw64_defconfig` or `make mingw32_defconfig` will pick up the default configuration.  You can then customize your build with `make menuconfig` or by editing `.config`, if you know what you're doing.

Then just `make`.

See the [Building busybox-w32](https://frippery.org/busybox/build.html) web page for additional information.

### Hints

 - Use forward slashes in paths:  Windows doesn't mind and the shell will be happier.
 - Windows paths are different from Unix ([more detail](https://frippery.org/busybox/paths.html)):
   * Absolute paths: `c:/path` or `//host/share/path`
   * Relative to current directory of other drive: `c:path`
   * Relative to current root (drive or share): `/path`
   * Relative to current directory of current root (drive or share): `path`
 - Handling of users, groups and permissions is totally bogus.  The system only admits to knowing about the current user and employs various heuristics to synthesise uid, gid and permission values.
 - Some crufty old Windows code (Windows XP, cmd.exe) doesn't like forward slashes in environment variables.  The -X shell option prevents busybox-w32 from changing backslashes to forward slashes.  If Windows programs don't run from the shell it's worth trying it.
 - If you want to install 32-bit BusyBox in a system directory on a 64-bit version of Windows you should put it in `C:\Windows\SysWOW64`, not `C:\Windows\System32` as you might expect.  On 64-bit systems the latter is for 64-bit binaries.
 - The system tries to detect the best way to handle the terminal being used.  If this doesn't work you can try setting the environment variable `BB_TERMINAL_MODE=1` to force the use of literal ANSI escapes or `BB_TERMINAL_MODE=0` to emulate them using the Windows console API.
 - busybox-w32 prefers built-in applets to external programs when running commands. This preference can be overridden by setting the environment variable `BB_OVERRIDE_APPLETS` to a space-separated list of applet names. Thus, to use an external `make` in preference to the built-in applet set `BB_OVERRIDE_APPLETS="make"`.
 - It's possible to obtain pseudo-random numbers using `if=/dev/urandom` as the input file to `dd`.  The same emulation of `/dev/urandom` is used internally by the `shred` utility and to support https in `wget`.  Serious users of random numbers may, of course, wish to make alternative arrangements.
