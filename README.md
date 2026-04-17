# JuceReverb

A headless stereo reverb VST3 plugin built with JUCE, cross-compiled from Linux to Windows with mingw-w64. Intended as an effect in Ableton Live on Windows.

## Parameters

| Name       | Range      | Default | Notes                                              |
| ---------- | ---------- | ------- | -------------------------------------------------- |
| Dry/Wet    | 0 – 1      | 0.30    | Post-reverb mix against an untouched dry copy      |
| Decay      | 0 – 1      | 0.50    | Maps to `1 - damping` of the underlying Freeverb   |
| Size       | 0 – 1      | 0.50    | Room size                                          |
| Width      | 0 – 1      | 1.00    | Stereo width                                       |
| Pre-Delay  | 0 – 200 ms | 20 ms   | Delay line in front of the reverb, linear interp   |

No GUI — Ableton displays its generic parameter panel.

## Signal chain

`input → pre-delay (juce::dsp::DelayLine) → reverb (juce::dsp::Reverb) → manual dry/wet mix`

The reverb runs 100 % wet internally; dry/wet is applied afterwards against a snapshot of the input buffer.

## Project layout

```
CMakeLists.txt                 top-level build
cmake/toolchain-mingw.cmake    mingw-w64 cross-compile toolchain
Source/PluginProcessor.h/.cpp  the processor (5 params, no editor)
Dockerfile                     Ubuntu 24.04 image with all host + cross deps
JUCE/                          cloned separately, gitignored
build-win/                     out-of-tree build dir, gitignored
```

## Build

### Prerequisites

The Dockerfile already installs everything needed. Outside Docker you need: `build-essential`, `cmake >= 3.22`, `mingw-w64` (with the `-posix` threading variant), `pkg-config`, and the Linux dev headers used by JUCE's host-side `juceaide` helper (`libfreetype-dev`, `libfontconfig1-dev`, `libasound2-dev`, `libjack-jackd2-dev`, `libcurl4-openssl-dev`, `libx11-dev`, `libxcomposite-dev`, `libxcursor-dev`, `libxext-dev`, `libxinerama-dev`, `libxrandr-dev`, `libxrender-dev`, `libglu1-mesa-dev`, `mesa-common-dev`).

### Get JUCE

JUCE 8 blocks MinGW (`#error "MinGW is not supported"` in `juce_TargetPlatform.h`). Use 7.0.12:

```bash
git clone --depth 1 --branch 7.0.12 https://github.com/juce-framework/JUCE.git
```

### Configure and build

```bash
cmake -S . -B build-win \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw.cmake \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build-win --target JuceReverb_VST3 -j"$(nproc)"
```

Output bundle:

```
build-win/JuceReverb_artefacts/Release/VST3/JuceReverb.vst3/
```

Inside, the actual DLL lives at `Contents/x86_64-win/JuceReverb.vst3`.

## Install on Windows

Copy the whole `JuceReverb.vst3/` directory (not just the inner DLL) to:

```
C:\Program Files\Common Files\VST3\
```

Rescan plugins in Ableton — **JuceReverb** will appear under the VST3 effects.

## Notes

- The DLL is statically linked against libgcc, libstdc++, and libwinpthread, so it runs on any Windows 10+ machine without extra runtime DLLs. Verify with:
  ```bash
  x86_64-w64-mingw32-objdump -p build-win/JuceReverb_artefacts/Release/VST3/JuceReverb.vst3/Contents/x86_64-win/JuceReverb.vst3 | grep 'DLL Name'
  ```
  Only Windows system DLLs should be listed (KERNEL32, msvcrt, USER32, …) — no `libgcc_s_*`, `libstdc++-*`, or `libwinpthread-*`.
- `VST3_AUTO_MANIFEST FALSE` is set in `juce_add_plugin` because the manifest-generation helper tool `juce_vst3_helper` needs `Windows.h` and cannot be built on a Linux host.
- The mingw-w64 toolchain uses the `-posix` gcc variants (`x86_64-w64-mingw32-g++-posix`). The default `-win32` threading model does not implement `std::thread`, which JUCE requires.
