# Hasten

![Build and unit tests](https://github.com/oleh-synelnykov/hasten/actions/workflows/ci.yml/badge.svg)

Hasten (/ˈheɪ.s<sup>ə</sup>n/) is the local RPC framework for C++.

## Requirements

- CMake 3.30+
- Conan 2.0+
- C++23 compiler (with Clang libc++ 17+ or GCC libstdc++ 13+)

## Building

### Dependencies

See `requirements()` method in conanfile.py for exact versions.

- Boost 1.89+ (older version will probably work)
- spdlog 1.16+
- nlohmann_json 3.11+
- gtest 1.17+

### Install dependencies

Release:
```bash
conan install . --build=missing
```

Debug:
```bash
conan install . -s build_type=Debug --build=missing
```
Use `--build=missing` to build any dependencies, if the binaries are not present in local conan cache or conancenter.

### Build

#### Configure

By default, conan generates two presets: `conan-release` and `conan-debug`. You can use them to configure the build.

```bash
cmake --preset=conan-<release|debug> .
```

#### Build

```bash
cmake --build --preset=conan-<release|debug>
```

#### Run tests

```bash
./build/<Debug|Release>/tests/test_hasten
```

### Examples

#### Echo

See the echo example in `examples/echo/` for a trivial echo-server setup using the runtime.
To build it, first create a hasten_runtime package:

```bash
cd src/runtime
conan create .
```
Add `-s build_type=Debug` for the debug builds. If any dependencies are missing, add `--build=missing`.

Once created, the package will be available in your local Conan cache and can be used by other projects. Now the echo example can be built:

```bash
cd examples/echo     # add ../.. if you're still in src/runtime
conan install .      # add -s build_type=Debug for debug build
cmake --preset=conan-release .         # or conan-debug for debug build
cmake --build --preset=conan-release   # or conan-debug for debug build
```
Once build succeeds the `echo_client` and `echo_server` executables will be available in the build directory (i.e. ./build/<Debug|Release>).

## Tooling

### clangd

For VSCode-like editors with clangd extension, create a symlink to `compile_commands.json` in project root:
```bash
ln -s build/<Debug|Release>/compile_commands.json
```
