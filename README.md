# Cyclone

Cyclone is a lightweight, cross-platform, high-performance C++ network library built with modern C++11 features. It provides a comprehensive set of networking utilities with an event-driven architecture based on the Reactor pattern.

## Features

- ✅ **Cross-platform**: Windows, macOS, Linux, Android
- ✅ **High-performance I/O**: Non-blocking I/O with IO multiplexing (`epoll`/`kqueue`/`select`)
- ✅ **Event-driven**: Reactor pattern with one loop per thread
- ✅ **Lock-free design**: Mostly wait-free multi-threaded data structures
- ✅ **Advanced I/O**: Vectored I/O support (`readv`/`writev`) and `timerfd` API (Linux and Android)
- ✅ **Cryptographic utilities**: DH key exchange, AES encryption, Adler32 checksum, and more
- ✅ **Comprehensive testing**: Full unit test suite using Catch2
- ✅ **Rich samples**: Multiple example applications demonstrating various use cases

## Dependencies

- [CMake](http://cmake.org/) 3.15 or later
- C++11 compatible compiler
- [Catch2](https://github.com/catchorg/Catch2) v2.13.10 (included as single header file, no external dependency)

## Build & Test

### On Linux or macOS:
 ```
 git clone https://github.com/thejinchao/cyclone
 mkdir _build && cd _build
 cmake -G "Unix Makefiles" ../cyclone
 make
 ```
### On Windows

```cmd
git clone https://github.com/thejinchao/cyclone
mkdir _build && cd _build
cmake -G "Visual Studio 17 2022" -A x64 ..\cyclone
cmake --build . --config Release
```

### On Android

Build on the host machine (Windows/Linux/macOS). Make sure the following environment variables are set correctly:
- `ANDROID_SDK_ROOT`
- `ANDROID_NDK_ROOT`

#### Using CMake from Android SDK:

```bash
git clone https://github.com/thejinchao/cyclone
mkdir _build && cd _build
%ANDROID_SDK_ROOT%/cmake/<version>/bin/cmake.exe -G "Ninja" -DANDROID_ABI=arm64-v8a ^
 -DANDROID_NDK=%ANDROID_NDK_ROOT% -DCMAKE_TOOLCHAIN_FILE=%ANDROID_NDK_ROOT%/build/cmake/android.toolchain.cmake ^
 -DANDROID_NATIVE_API_LEVEL=34 -DCMAKE_MAKE_PROGRAM=%ANDROID_SDK_ROOT%/cmake/<version>/bin/ninja.exe ^
 ../cyclone

%ANDROID_SDK_ROOT%/cmake/<version>/bin/ninja.exe
```

#### Running Tests on Android Device:
```bash
adb push test/unit/cyt_unit /data/local/tmp/
adb shell chmod +x /data/local/tmp/cyt_unit
adb shell /data/local/tmp/cyt_unit
```
## Testing

Cyclone uses [Catch2](https://github.com/catchorg/Catch2) v2.13.10 for unit testing. The test framework is included as a single header file, so no external dependencies are required.

Cyclone has been tested with:
- Microsoft Visual Studio 2022 (under Windows 11)
- Clang 17 (under MacOS 26)
- GCC 11.5 (under AlmaLinux 9.7)
- Android NDK 27, API level 34

### Test Coverage

The test suite covers:
- ✅ Core utilities (Signal, Lock-Free Queue, Ring Buffer, System API)
- ✅ Event loop (Basic events, Timers, Socket events)
- ✅ Cryptographic utilities (AES, DH, Adler32, XorShift128)
- ✅ Utility classes (Statistics, Ring Queue, Pipe, Packet)

## Samples
- **echo** - A typical client/server program demonstrating basic TCP communication
- **timer** - Demonstrates how to use timer functionality in event loop
- **chat** - A simple chat room program with multiple clients
- **socks5** - A [SOCKS5](http://www.ietf.org/rfc/rfc1928.txt) proxy server (TCP protocol only)
- **relay** - An interesting socket tunnel utility, supports n:1, 1:n connections, key-exchange and AES encryption
- **filetransfer** - High-performance file transfer utility

## License

Copyright(C) thecodeway.com
