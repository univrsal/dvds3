# dvds3
![nightly](https://github.com/univrsal/dvds3/workflows/nightly/badge.svg)
Will it hit the corner?

obs-studio plugin, which adds a dvd screen saver source type

Issues have moved to [git.vrsal.xyz/alex/dvds3/issues](https://git.vrsal.xyz/alex/dvds3/issues). You can sign up to submit new ones.

![logo](./data/dvd.png)

## Building

Requires OBS Studio 33.0.0 or newer. The plugin's only build dependency is libobs, which OBS exports as a CMake package.

```sh
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=<install-dir>
cmake --build build --config RelWithDebInfo
```

### Windows (x64)

libobs must be built from the OBS 33 source and installed (Development component) first. Then configure the plugin with:

| Option | Value |
| --- | --- |
| `libobs_DIR` | `<obs-sdk>/cmake` (contains `libobsConfig.cmake`) |
| `w32-pthreads_DIR` | `<obs-sdk>/cmake` (contains `w32-pthreadsConfig.cmake`) |
| `SIMDe_INCLUDE_DIR` | `<obs-deps>/include` (contains `simde/simde-common.h`) |

```powershell
cmake -S . -B build64 -G "Visual Studio 18 2026" -A x64 `
  -Dlibobs_DIR="<obs-sdk>\cmake" `
  -Dw32-pthreads_DIR="<obs-sdk>\cmake" `
  -DSIMDe_INCLUDE_DIR="<obs-deps>\include"
cmake --build build64 --config RelWithDebInfo
```

- `<obs-deps>` is the extracted `windows-deps-<date>-x64.zip` from [obs-deps releases](https://github.com/obsproject/obs-deps/releases).
- The `w32-pthreads_DIR` and `SIMDe_INCLUDE_DIR` overrides are needed because libobs' package config does not find its own dependencies when installed to a custom prefix.
- The `libobs_DIR` override is needed because the package is not on CMake's default search path.

The build output is `build64/RelWithDebInfo/dvd-screensaver.dll`.

### Linux

Install the `libobs-dev` and `libsimde-dev` packages from the OBS PPA (`ppa:obsproject/obs-studio`) for OBS 33 or newer. The package provides libobs' CMake config under the system library path, so no overrides are needed.

## Continuous integration

`.github/workflows/nightly.yml` builds the Linux `.deb` and the Windows x64 zip and installer. The Windows job builds libobs from the OBS 33.0.0 tag, using the same overrides as above.
