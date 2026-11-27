# Development

## Prerequisites

The current project foundation requires:

- A C++20 compiler.
- CMake 3.24 or newer.
- Ninja.
- Qt 6.4 or newer development files, including Qt Core and Qt Test.
- Git.

The Kirigami UI, KDE Frameworks integration, KWin adapter, and D-Bus service are not part of this initial build target yet. Their additional development dependencies will be documented when those targets are introduced.

## Configure

From the repository root:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
```

## Build

```sh
cmake --build build --parallel
```

## Run tests

```sh
ctest --test-dir build --output-on-failure
```

The same configure, build, and test sequence runs in GitHub Actions on Ubuntu 24.04. The CI job installs the distribution packages `cmake`, `g++`, `ninja-build`, and `qt6-base-dev`.

## Build options

- Set `-DBUILD_TESTING=OFF` to omit the Qt Test dependency.
- Use a different build directory if you need separate configurations, for example `build-release` with `-DCMAKE_BUILD_TYPE=Release`.
