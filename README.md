# boxing-trainer-cpp

Modern C++23 port of the Go/Fyne boxing trainer. The desktop UI uses FLTK, with the dependency declared in `vcpkg.json`.

## Build

The presets use Homebrew LLVM C++ (`/opt/homebrew/opt/llvm/bin/clang++`) and Ninja, not Apple `/usr/bin/c++`. They also pin Homebrew `clang-scan-deps` and libc++'s `libc++.modules.json`, which CMake needs for C++23 modules and `import std;`. The desktop preset uses a vcpkg overlay triplet, `arm64-osx-homebrew-llvm`, for both target and host triplets so vcpkg-built dependencies use the same Homebrew LLVM toolchain. The project is configured as C++23, uses `import std;` for the standard library, and uses named modules for its own code: `boxing_trainer.common`, `boxing_trainer.text`, `boxing_trainer.combo`, `boxing_trainer.model`, `boxing_trainer.video_url`, and `boxing_trainer.app`.

## Clean Build / Regenerate CMake

CMake build directories are machine-local. `CMakeCache.txt` stores absolute paths to the source tree, compilers, vcpkg checkout, generated module metadata, and dependency install directories. If this repository is moved, for example from `boxing-trainer-go/boxing-trainer-cpp` to `boxing-trainer-cpp`, any existing `build*` directory must be regenerated before building again.

For the fastest clean rebuild, refresh the existing CMake directory for the preset you want:

```sh
cmake --fresh --preset core-tests
cmake --build --preset core-tests
ctest --preset core-tests
```

For the desktop app:

```sh
export VCPKG_ROOT="$HOME/vcpkg"
cmake --fresh --preset desktop-vcpkg
cmake --build --preset desktop-vcpkg
ctest --preset desktop-vcpkg
```

For a full clean build from scratch, remove all generated build directories and configure again:

```sh
cmake -E rm -rf build build-core build-release build-gui-probe
cmake --preset core-tests
cmake --build --preset core-tests
ctest --preset core-tests
```

Use the desktop preset instead of `core-tests` in the last three commands when you need the FLTK application:

```sh
export VCPKG_ROOT="$HOME/vcpkg"
cmake --preset desktop-vcpkg
cmake --build --preset desktop-vcpkg
ctest --preset desktop-vcpkg
```

Do not commit `CMakeCache.txt`, `CMakeFiles/`, `build/`, or `build-*`; they are generated and can contain absolute paths from another checkout.

Required tools:

```sh
brew install llvm cmake ninja
```

Use the workflow presets when you want CMake to configure before it builds.

Core model/parser tests do not require GUI dependencies:

```sh
cmake --workflow --preset core-tests
```

That workflow is equivalent to:

```sh
cmake --preset core-tests
cmake --build --preset core-tests
ctest --preset core-tests
```

Desktop build with vcpkg-managed FLTK:

```sh
cmake --workflow --preset desktop-vcpkg
```

The preset uses `VCPKG_ROOT` when it is set. If it is not set, it automatically uses `$HOME/vcpkg` when that checkout exists.

That workflow is equivalent to:

```sh
export VCPKG_ROOT=$HOME/vcpkg
cmake --preset desktop-vcpkg
cmake --build --preset desktop-vcpkg
ctest --preset desktop-vcpkg
```

The executable is `build/boxing_trainer_cpp`.

## Release Binary

Before making a release binary, run the normal desktop workflow at least once so the tests pass:

```sh
cmake --workflow --preset desktop-vcpkg
```

Then build the size-optimized release preset:

```sh
cmake -E rm -rf build-release
cmake --workflow --preset desktop-release-vcpkg
```

The release executable is:

```sh
build-release/boxing_trainer_cpp
```

The `desktop-release-vcpkg` preset uses:

- `CMAKE_BUILD_TYPE=MinSizeRel`
- Homebrew LLVM `clang++`
- C++23 modules and `import std;`
- interprocedural optimization/LTO
- macOS linker dead stripping via `-Wl,-dead_strip`
- `BUILD_TESTING=OFF`

CMake does not strip the built executable automatically. Strip it explicitly with Homebrew LLVM:

```sh
/opt/homebrew/opt/llvm/bin/llvm-strip --strip-all build-release/boxing_trainer_cpp
```

Check the final size and dynamic library dependencies:

```sh
ls -lh build-release/boxing_trainer_cpp
otool -L build-release/boxing_trainer_cpp
```

If you need a distributable folder, remember that the app data lives in `resources/combinations.txt`; stripping only minimizes the executable.

Do not create `build/` manually. `cmake --preset desktop-vcpkg` creates and configures it. Running `cmake --build --preset desktop-vcpkg` first will fail with `not a CMake build directory` because no `CMakeCache.txt` exists yet.

If you already created `build/` manually, or if an earlier configure failed before vcpkg was ready, remove that partial directory and configure again:

```sh
cmake -E rm -rf build
export VCPKG_ROOT="$HOME/vcpkg"
cmake --workflow --preset desktop-vcpkg
```

If `VCPKG_ROOT` is not set, install vcpkg once:

```sh
git clone https://github.com/microsoft/vcpkg.git "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh"
export VCPKG_ROOT="$HOME/vcpkg"
```

The Homebrew `vcpkg` binary alone is not enough; `VCPKG_ROOT` must point at a vcpkg checkout containing `scripts/buildsystems/vcpkg.cmake`. The `desktop-vcpkg` preset checks this and prints a clear error if the environment is not ready.

The app intentionally depends on `fltk` with `default-features: false`. That avoids FLTK's optional OpenGL feature and therefore avoids vcpkg's `opengl-registry`/Khronos download path, which is not needed by this 2D widget UI.

## Behavior

The C++ app preserves the Go app's data format, favorites file, state file, filters, search ranking, video URL allowlist, main/info/defense views, and keyboard shortcuts:

- Left/Right: previous/next combination
- Space or F: toggle favorite on the main view
- H: open current video on the main view
- S: toggle search on the main view
- D: defense view
- I: info view
- C: close subview and return to the main view
