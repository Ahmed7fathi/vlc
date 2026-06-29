# VLC media player

**VLC** is a libre and open source **media player** and **multimedia engine**,
focused on **playing everything**, and **running everywhere**.

**VLC** can play most multimedia files, discs, streams, devices and is also able to
convert, encode, **stream** and manipulate streams into numerous formats.

VLC is used by many over the world, on numerous platforms, for very different use cases.

The **engine of VLC** can be embedded into 3rd party applications, and is called *libVLC*.

**VLC** is part of the [VideoLAN project](https://videolan.org) and
is developed and supported by a community of volunteers.

The VideoLAN project was started at the university [École Centrale Paris](https://www.centralesupelec.fr/) who
relicensed VLC under the GPLv2 license in February 2001. Since then, VLC has
been downloaded **billions** of times.

## License

**VLC** is released under the GPLv2 *(or later)* license.
*On some platforms, it is de facto GPLv3, because of the licenses of dependencies*.

**libVLC**, the engine is released under the LGPLv2 *(or later)* license. \
This allows embedding the engine in 3rd party applications, while letting them to be licensed under other licenses.

# What's New

See [CHANGELOG.md](CHANGELOG.md) for a detailed history of changes.

Key highlights:

- **Timeline Thumbnail Preview** — YouTube-style video thumbnail previews on the seek bar hover (VLC 4.0)

# Platforms

VLC is available for the following platforms:
- [Windows] *(from 7 and later, including UWP platforms and all versions of Windows 10)*
- [macOS] *(10.10 and later)*
- [GNU/Linux] and affiliated
- [BSD] and affiliated
- [Android] *(4.2 and later)*, including Android TV and Android Auto
- [iOS] *(9 and later)*, including AppleTV and iPadOS
- Haiku, OS/2 and a few others.

[Windows]: https://www.videolan.org/vlc/download-windows.html
[macOS]: https://www.videolan.org/vlc/download-macosx.html
[GNU/Linux]: https://www.videolan.org/vlc/#download
[BSD]: https://www.videolan.org/vlc/download-freebsd.html
[Android]: https://www.videolan.org/vlc/download-android.html
[iOS]: https://www.videolan.org/vlc/download-ios.html

Not all platforms receive the same amount of care, due to our limited resources.

**Nota Bene**: The [Android app](https://code.videolan.org/videolan/vlc-android/) and
the [iOS app](https://code.videolan.org/videolan/vlc-ios/) are located in different repositories
than the main one.

# Building from Source

## Prerequisites

### Required Build Tools

- **Compiler**: C11/C17 compiler (GCC >= 5 or Clang >= 3.4) with C++17 support (G++ >= 7 or Clang++ >= 5)
- **Build System**: Meson >= 1.1.0 and Ninja (preferred), or GNU Autotools (autoconf, automake, libtool, make)
- **pkg-config**
- **GNU gettext** (for translations)
- **flex** and **bison** (for CSS/WebVTT parsing)
- **Rust nightly toolchain** (optional, for Rust modules)

### Required Development Libraries

The following libraries are required for a typical build:

- zlib
- libiconv
- pthreads
- Lua (>= 5.1, used for interfaces and scripting)
- libxml2 (XML parsing)
- gettext (internationalization)

### Optional but Common Dependencies

| Library | Purpose |
|---------|---------|
| FFmpeg (libavcodec, libavformat, libavutil, libswscale) | Video/audio codecs and container support |
| Qt 6 (qt6-base, qt6-declarative, qt6-shadertools) | Modern GUI (recommended) |
| GnuTLS | HTTPS/TLS support |
| libaom, libdav1d | AV1 codec support |
| libvpx | VP8/VP9 codec support |
| libopus, libvorbis, libogg | Audio codec support |
| libass | ASS/SSA subtitle rendering |
| freetype2, fontconfig, harfbuzz, fribidi | Text rendering |
| libbluray | Blu-ray playback |
| libdvdnav, libdvdread | DVD playback |
| libplacebo | Advanced video filtering |
| libgcrypt | Cryptography |
| ALSA, PulseAudio, PipeWire | Linux audio output |
| X11, XCB, Wayland | Display output on Linux |
| dbus | D-Bus integration |
| udev | Device discovery |
| libupnp | UPnP service discovery |

### Installing Prerequisites

**Debian / Ubuntu**:
```sh
sudo apt install build-essential meson ninja-build pkg-config gettext flex bison \
  liblua5.2-dev libxml2-dev libfreetype-dev libfontconfig-dev libharfbuzz-dev \
  libass-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev \
  libpostproc-dev libplacebo-dev libgcrypt-dev libssl-dev libdvdnav-dev \
  libdvdread-dev libbluray-dev libvpx-dev libopus-dev libvorbis-dev \
  libogg-dev libtheora-dev libaom-dev libdav1d-dev libxcb-shm0-dev \
  libxcb-xv0-dev libx11-dev libxcb-composite0-dev libxcb-randr0-dev \
  libxcb-xfixes0-dev libwayland-dev libpulse-dev libpipewire-0.3-dev \
  libalsa2-dev libdbus-1-dev libsystemd-dev libudev-dev libupnp-dev \
  qt6-base-dev qt6-declarative-dev qt6-shadertools-dev \
  libgl1-mesa-dev libgles2-mesa-dev libegl1-mesa-dev || echo "Check individual package names for your distribution"
```

**Fedora / RHEL**:
```sh
sudo dnf install meson ninja-build gcc gcc-c++ make pkgconfig gettext-devel flex bison \
  lua-devel libxml2-devel freetype-devel fontconfig-devel harfbuzz-devel \
  libass-devel ffmpeg-devel libplacebo-devel libgcrypt-devel \
  libdvdnav-devel libdvdread-devel libbluray-devel libvpx-devel \
  opus-devel libvorbis-devel libogg-devel libtheora-devel aom-devel \
  dav1d-devel libxcb-devel libX11-devel wayland-devel \
  pulseaudio-libs-devel pipewire-devel alsa-lib-devel \
  dbus-devel systemd-devel libudev-devel \
  qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtshadertools-devel \
  mesa-libGL-devel mesa-libGLES-devel mesa-libEGL-devel
```

**macOS** (using Homebrew):
```sh
brew install meson ninja pkg-config gettext flex bison lua libxml2 freetype \
  fontconfig harfbuzz libass ffmpeg libplacebo gcrypt libdvdnav libdvdread \
  libbluray libvpx opus libvorbis libogg aom dav1d qt glew
```

**Windows** (using contribs from VLC):
```sh
# Pre-built contribs are available; see:
# https://wiki.videolan.org/Win32Compile
```

## Building with Meson (Recommended)

VLC 4.0 uses Meson as the primary build system. It is still experimental but recommended for new work.

### Quick Start

```sh
# First-time setup
meson setup builddir

# Build (incremental, fast for repeated builds)
meson compile -C builddir

# Install (optional)
meson install -C builddir
```

> **Tip:** After editing code (but not `meson.build`), you only need to run `meson compile -C builddir` — no need to re-run `meson setup`. If you edit `meson.build`, run `meson setup --reconfigure builddir` first.

### Common Meson Options

```sh
# Enable debug build
meson setup builddir -Ddebug=true

# Enable extra compiler checks
meson setup builddir -Dextra_checks=true

# Enable tests (needed to build test executables)
meson setup builddir -Dtests=enabled

# Enable Rust modules (experimental)
meson setup builddir -Drust=enabled

# Disable Qt GUI (e.g., for headless/server use)
meson setup builddir -Dqt=disabled

# See all options
meson configure builddir
```

### Running the Built Binary

After building, you can run VLC directly from the build directory:

```sh
./builddir/bin/vlc
# or
./builddir/src/vlc
```

### Quick Rebuild (Qt plugin only)

After making changes to Qt GUI code, you can rebuild just the Qt plugin for faster iteration:

```sh
ninja -C builddir modules/libqt_plugin.so
```

This is much faster than a full rebuild and is sufficient for testing UI changes.

## Building with Autotools (Legacy)

VLC also maintains the traditional autotools-based build system:

```sh
# If building from Git, first generate the configure script
./bootstrap

# Configure
./configure

# Build
make -j$(nproc)

# Install (optional)
sudo make install
```

### Common Autotools Options

```sh
# Debug build
./configure --enable-debug

# Enable all optimizations (for production)
./configure --enable-optimizations

# Disable Qt GUI
./configure --disable-qt

# Enable Rust modules
./configure --enable-rust

# See all options
./configure --help
```

## Running Tests

### With Meson

```sh
# Configure with tests enabled
meson setup builddir -Dtests=enabled
ninja -C builddir

# Run all tests
meson test -C builddir

# Run a specific test suite
meson test -C builddir --suite test

# Run a specific test
meson test -C builddir test_libvlc_core

# Run tests with verbose output
meson test -C builddir -v
```

### With Autotools

```sh
# Build and run all tests
make check

# Build test programs without running them
make check_programs

# Run only player-related tests
make checkplayer
```

Note: Some tests require media sample files and may need to be run from the build directory.

# Contributing & Community

**VLC** is maintained by a community of people, and VideoLAN is not paying any of them.\
The community is composed of developers, helpers, maintainers, designers and writers that want
this open source project to thrive.

The main development of VLC is done in the C language, but this repository also contains
plenty of C++, Obj-C, asm and Rust.

Other repositories linked to vlc are done in languages including Kotlin/Java [(Android)](https://code.videolan.org/videolan/vlc-android/),
Swift [(iOS)](https://code.videolan.org/videolan/vlc-ios/), and C# [(libVLCSharp)](https://code.videolan.org/videolan/libvlcsharp/).

We need help with the following tasks:
- Coding
- Packaging for Windows, macOS and Linux distributions
- Technical writing for the documentation
- Design
- Support
- Community management and communication.

Please contribute :)

We are on IRC. You can find us on the **#videolan** channel on *[Libera.chat]*.

[Libera.chat]: https://libera.chat

## Contributions

Contributions are now done through Merge Requests on our [GitLab repository](https://code.videolan.org/videolan/vlc/).

CI and discussions should be resolved before a Merge Request can be merged.

# libVLC

**libVLC** is an embeddable engine for 3rd party applications and frameworks.

It runs on the same platforms as VLC *(and sometimes on more)* and can provide playback,
streaming and conversion of multimedia files and streams.


**libVLC** has numerous bindings for other languages, such as C++, Python and C#.

# Support

## Links

Some useful links that might help you:

- [VLC web site](https://www.videolan.org/vlc/)
- [Support](https://www.videolan.org/support/)
- [Forums](https://forum.videolan.org/)
- [Wiki](https://wiki.videolan.org/)
- [Developer's Corner](https://wiki.videolan.org/Developers_Corner)
- [VLC hacking guide](https://wiki.videolan.org/Hacker_Guide)
- [Bugtracker](https://code.videolan.org/videolan/vlc/-/issues)
- [VideoLAN web site](https://www.videolan.org/)

## Source Code sitemap
```
ABOUT-NLS          - Notes on the Free Translation Project.
AUTHORS            - VLC authors.
COPYING            - The GPL license.
COPYING.LIB        - The LGPL license.
INSTALL            - Installation and building instructions.
NEWS               - Important modifications between the releases.
README             - Project summary.
THANKS             - VLC contributors.

bin/               - VLC binaries.
bindings/          - libVLC bindings to other languages.
compat/            - compatibility library for operating systems missing
                     essential functionalities.
contrib/           - Facilities for retrieving external libraries and building
                     them for systems that don't have the right versions.
doc/               - Miscellaneous documentation.
extras/analyser    - Code analyser and editor specific files.
extras/buildsystem - Different build system specific files.
extras/misc        - Files that don't fit in the other extras/ categories.
extras/package     - VLC packaging specific files such as spec files.
extras/tools/      - Facilities for retrieving external building tools needed
                     for systems that don't have the right versions.
include/           - Header files.
lib/               - libVLC source code.
modules/           - VLC plugins and modules. Most of the code is here.
po/                - VLC translations.
share/             - Common resource files.
src/               - libvlccore source code.
test/              - Testing system.
```
