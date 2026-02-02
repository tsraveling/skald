# SKALD

A procedural narrative scripting language and toolset used for interactive fiction projects.

## Building

Requires CMake 3.24+ and a C++17 compiler.

### macOS

```bash
mkdir build && cd build
cmake ..
make
```

Outputs: `libskald.dylib`, `libskald_static.a`

### Linux

```bash
mkdir build && cd build
cmake ..
make
```

Outputs: `libskald.so`, `libskald_static.a`

### Windows (MSVC)

```powershell
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

> MSVC generates multi-configuration builds (Debug/Release in one project). `--config Release` selects which to build. On Unix, you'd set this at configure time with `-DCMAKE_BUILD_TYPE=Release` instead.

Outputs: `skald.dll`, `skald_static.lib`

## Cross-Compiling from macOS

### Linux (via Docker)

```bash
docker run --rm -v $(pwd):/src -w /src gcc:latest bash -c \
  "mkdir -p build-linux && cd build-linux && cmake .. && make"
```

> - `--rm` — Remove container after exit (cleanup)
> - `-v $(pwd):/src` — Mount current directory into container so build outputs persist on host
> - `-w /src` — Set working directory inside container
> - `gcc:latest` — Official GCC Docker image with full Linux toolchain

### Windows (via MinGW-w64)

```bash
brew install mingw-w64
mkdir build-win && cd build-win
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/mingw-w64.cmake \
         -DCMAKE_SYSTEM_NAME=Windows
make
```

> - `brew install mingw-w64` — Installs a Windows cross-compiler toolchain that runs on macOS
> - `-DCMAKE_TOOLCHAIN_FILE=...` — Points CMake to a file defining cross-compiler paths
> - `-DCMAKE_SYSTEM_NAME=Windows` — Tells CMake the target OS so it generates Windows-style outputs (`.dll`, `.lib`)

Create `cmake/mingw-w64.cmake`:
```cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
```

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `SKALD_BUILD_SHARED` | ON | Build shared library (C bindings) |
| `SKALD_BUILD_TEST_EXECUTABLE` | ON | Build test executable |
| `SKALD_BUILD_C_TEST` | OFF | Build C API test |

