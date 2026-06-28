# AGENTS.md

Guidance for automated agents and contributors working in this repository.

## Project Context

This is a modern C++23 port of the Go/Fyne boxing trainer. The project uses:

- CMake presets with Ninja.
- Homebrew LLVM on macOS arm64.
- C++23 named modules and `import std;`.
- wxWidgets for the desktop UI, provided through vcpkg.
- A small in-repo test executable driven by CTest.

Treat the repository as a C++23 codebase first. Do not rewrite code toward older C++ standards for portability unless the user explicitly asks for that tradeoff.

## Build And Test

Use the checked-in CMake presets instead of hand-written configure commands.

Core parser/model tests without GUI dependencies:

```sh
cmake --workflow --preset core-tests
```

Desktop build and tests with vcpkg-managed wxWidgets:

```sh
export VCPKG_ROOT="$HOME/vcpkg"
cmake --workflow --preset desktop-vcpkg
```

Release desktop build:

```sh
export VCPKG_ROOT="$HOME/vcpkg"
cmake --workflow --preset desktop-release-vcpkg
```

When CMake state looks stale, especially after moving the repository, regenerate it:

```sh
cmake --fresh --preset core-tests
cmake --fresh --preset desktop-vcpkg
```

For a full clean build:

```sh
cmake -E rm -rf build build-core build-release build-gui-probe
cmake --workflow --preset core-tests
```

Do not commit generated CMake or build output: `build/`, `build-*`, `CMakeCache.txt`, `CMakeFiles/`, Ninja logs, object files, or vcpkg installed trees.

## C++23 Style

Prefer current, idiomatic C++23 facilities where they make the code clearer, safer, or simpler.

- Keep targets at `cxx_std_23`; do not downgrade target features.
- Prefer modules for project interfaces. Put exported declarations in `.cppm` files and implementation details in `.cpp` files.
- Use `import std;` consistently with the existing code instead of adding legacy standard-library includes to module-aware translation units.
- Prefer standard-library types and algorithms over custom helpers when the C++23 facility is clear: `std::ranges`, `std::views`, `std::span`, `std::string_view`, `std::optional`, `std::variant`, `std::expected`, `std::filesystem`, and `std::chrono`.
- Prefer `std::format` for building strings and `std::print`/`std::println` for direct console output. Avoid `std::ostringstream` for new formatting code unless stream formatting is materially clearer or required by an API.
- Prefer `std::from_chars`/`std::to_chars` for locale-independent numeric parsing and formatting.
- Prefer range-based algorithms and projections over manual loops when they improve readability. Do not force ranges into code where a direct loop is simpler.
- Prefer value semantics, RAII, and standard containers. Avoid raw owning pointers and manual resource management.
- Use `constexpr`, `const`, `[[nodiscard]]`, and narrow scopes where they express intent.
- Preserve exception behavior where existing APIs throw. For new APIs, choose between exceptions, `std::optional`, and `std::expected` based on whether the caller can reasonably recover.
- Keep string ownership explicit: use `std::string_view` for non-owning inputs, `std::string` for stored values, and avoid returning views into temporaries.

## Code Organization

- Keep parser/domain logic in `src/combo` and `src/model`.
- Keep UI-specific behavior in `src/view`.
- Keep small shared utilities in `src/common.cppm` only when they are genuinely shared.
- Keep tests in `tests/test_main.cpp` unless the test surface grows enough to justify splitting files.
- Preserve the existing module namespace style: `boxing_trainer.*` modules and `boxing_trainer::...` namespaces.

## CMake And Dependencies

- Add source files to the appropriate `target_sources` file set when creating new module units.
- Keep vcpkg dependencies declared in `vcpkg.json`.
- Keep macOS arm64 vcpkg triplet behavior in `vcpkg-triplets/arm64-osx-homebrew-llvm.cmake`.
- Avoid absolute source paths in checked-in files. Presets should use `${sourceDir}` or relative paths.
- If a dependency is only needed by the GUI, keep the core test preset independent of it.

## Testing Expectations

For changes to parser, model, URL handling, or shared utilities, run:

```sh
cmake --workflow --preset core-tests
```

For changes that affect wxWidgets UI code, CMake/vcpkg configuration, or the final executable, also run:

```sh
cmake --workflow --preset desktop-vcpkg
```

For release-build settings, run:

```sh
cmake --workflow --preset desktop-release-vcpkg
```

If a command cannot be run because a tool or vcpkg checkout is missing, state that clearly and include the exact command that should be run after the environment is ready.

## Editing Rules

- Keep changes scoped to the user request.
- Do not reformat unrelated files.
- Do not rewrite working C++23 module code into header/source style.
- Do not add compatibility shims for older compilers unless requested.
- Prefer small, focused functions over broad abstractions.
- Preserve existing behavior and data formats unless the user explicitly asks for a change.
- Update README or this file when build, dependency, or workflow commands change.
