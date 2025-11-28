# Hasten

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

## Tooling

### clangd

For VSCode-like editors with clangd extension, create a symlink to `compile_commands.json` in project root:
```bash
ln -s build/<Debug|Release>/compile_commands.json
```
