# Aegisub

Aegisub is a subtitle editor, primarily aimed at typesetting and timing of
Advanced SubStation Alpha (ASS) subtitles.

## About this repository

This tree is a modernisation of the upstream
[Aegisub/Aegisub](https://github.com/Aegisub/Aegisub) repository, which has
been dormant since October 2019. The build system has been ported to
[Meson](https://mesonbuild.com/), the codebase raised to C++17, and the
Windows build moved to Visual Studio 2022 and wxWidgets 3.2.

If you are looking for a maintained fork with substantial new *features*
(video panning, line folding, VapourSynth and BestSource providers), see
[arch1t3cht/Aegisub](https://github.com/arch1t3cht/Aegisub), whose Meson build
scaffolding this port builds on.

## Building Aegisub

### Windows

This is the supported and tested platform.

Prerequisites:

1. **Visual Studio 2022** with the "Desktop development with C++" workload, or
   the standalone [Build Tools for Visual Studio 2022](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022).
   The Windows 10/11 SDK is included with that workload.
2. **Python 3.8 or newer**.
3. **Meson and Ninja**: `pip install meson ninja`

The June 2010 DirectX SDK is *no longer required*. Audio output now uses
XAudio2, which ships with the Windows SDK; DirectSound remains available as a
fallback and also builds against the modern SDK.

Building, from a Visual Studio developer command prompt (or any shell where
`vcvars64.bat` has been run):

```
git clone https://github.com/Aegisub/Aegisub.git
cd Aegisub
meson setup build-win --default-library=static
meson compile -C build-win
```

`--default-library=static` is required: ICU does not support a shared build on
Windows, and Aegisub has always shipped as a self-contained executable.

The first configure downloads and builds every dependency (Boost, ICU,
wxWidgets, FFmpeg, libass, LuaJIT and others) as Meson subprojects, so expect
it to take a while and a few GB of disk. Subsequent builds are incremental.
There is no longer any need to clone submodules — `git clone --recursive` is
not used and `.gitmodules` has been retired.

The result is `build-win/aegisub.exe`. To run it against the automation
scripts in this tree, either copy the `automation` directory next to the
executable or add it to the automation search path in Preferences.

To run the test suite:

```
meson test -C build-win --print-errorlogs
```

Useful configure options (`meson configure build-win` lists all of them):

| Option | Default | Purpose |
| --- | --- | --- |
| `-Dxaudio2=` | `auto` | XAudio2 audio output |
| `-Ddirectsound=` | `auto` | DirectSound audio output |
| `-Dffms2=` | `auto` | FFMS2 audio/video source |
| `-Dhunspell=` | `auto` | Spell checking |
| `-Duchardet=` | `auto` | Character set detection |
| `-Dcsri=` | `auto` | CSRI subtitle provider (VSFilter) |
| `-Dtests=` | `auto` | Build the unit tests |
| `-Ddefault_audio_output=` | `auto` | Force a specific audio backend |

The Aegisub installer bundles some files that are not built here (VSFilter,
for instance), so for a fully featured copy you may still want to copy the
extra files from an installed release into the build output.

### Linux and macOS

The autotools build (`configure.ac` and the per-directory makefiles) is still
present but is **not currently verified** — this modernisation targeted
Windows. The Meson build contains the non-Windows code paths and should be a
better starting point than autotools:

```
meson setup build
meson compile -C build
```

Expect to need to fix up dependency detection.

## Updating Moonscript

From within the Moonscript repository, run
`bin/moon bin/splat.moon -l moonscript moonscript/ > bin/moonscript.lua`.
Open the newly created `bin/moonscript.lua`, and within it make the following
changes:

1. Prepend the final line of the file, `package.preload["moonscript"]()`, with
   a `return`, producing `return package.preload["moonscript"]()`.
2. Within the function at `package.preload['moonscript.base']`, remove
   references to `moon_loader`, `insert_loader`, and `remove_loader`. This
   means removing their declarations, definitions, and entries in the returned
   table.
3. Within the function at `package.preload['moonscript']`, remove the line
   `_with_0.insert_loader()`.

The file is now ready for use, to be placed in `automation/include` within the
Aegisub repo.

## License

All files in this repository are licensed under various GPL-compatible
BSD-style licenses; see LICENCE and the individual source files for more
information. The official Windows and OS X builds are GPLv2 due to including
fftw3.
