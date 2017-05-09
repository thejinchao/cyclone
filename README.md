# Cyclone
Cyclone is a lightweight network library, It is cross-platform and high-performance
- Cross platform (Windows, Mac OS X, Linux, Android)
- Non-blocking IO + IO multiplexing, support `epoll`, `kqueue`, `select`
- One loop per thread + reactor model
- Mostly wait-free multi-threaded design
- Support vectored I/O and timerfd api(Linux)
- Usefull utility support(DH key exchange, AES, adler32, and more)
- Unit test and samples
   
# Dependencies
Cyclone depends on following libraries 
- [CMake](http://cmake.org/), at least version 2.8
- [jemalloc](http://jemalloc.net/) , Optional
- To run unit test:
  - [Google Test](https://github.com/google/googletest)
- To enable debug
  - [hiredis](https://github.com/redis/hiredis)
- To run samples
  - [SimpleOpt](https://github.com/brofield/simpleopt), Have been included in the code base
 
 Cyclone has been tested with Clang (under Mac OS X), GCC 4.8+ (under Linux), Android Build Tools 25.0 and Microsoft Visual Studio 2015(under Windows 10).
 
 # Build & Test
 ## On Linux or Mac OS X:
 ```
 git clone https://github.com/thejinchao/cyclone
 mkdir _build && cd _build
 cmake -G "Unix Makefiles" ../cyclone
 make
 make test
 ```
## On Windows
1. Open CMake-GUI, enter the correct directory for source code and build. Then click *Configure*, choose your installed version of the Microsoft Visual Studio.
2. Click generate after fixing all missing variables to generate your Visual Studio solution.
3. Open the solution and compile the code.
4. Right click the project *RUN TESTS* and slect *Build* to run unit test

## On Android
Build on the windows host machine, make sure the flowing envionment variables have been set correctly. `ANDROID_SDK_ROOT`, `ANDROID_NDK_ROOT`
```
git clone https://github.com/thejinchao/cyclone
mkdir _build && cd _build
%ANDROID_SDK_ROOT%/cmake/3.6.3155560/bin/cmake.exe -G "Android Gradle - Ninja" -DANDROID_ABI=armeabi-v7a -DANDROID_NDK=%ANDROID_NDK_ROOT% -DCMAKE_TOOLCHAIN_FILE=%ANDROID_NDK_ROOT%/build/cmake/android.toolchain.cmake -DANDROID_NATIVE_API_LEVEL=23 -DCMAKE_MAKE_PROGRAM=%ANDROID_SDK_ROOT%/cmake/3.6.3155560/bin/ninja.exe ../cyclone
%ANDROID_SDK_ROOT%/cmake/3.6.3155560/bin/ninja.exe
```
To run unit test, an android device with root authority is required.
```
adb push test/unit/cyt_unit /data/local
adb shell chmod +x /data/local/cyt_unit
adb shell /data/local/cyt_unit
```
# Samples
- **echo** a typical client/server program
- **timer** just show how to use timer function
- **chat** a simple chat room program
- **socks5** a [Socks5](http://www.ietf.org/rfc/rfc1928.txt) proxy server(only support tcp protocol)
- **relay** a interesting socket tunnel utility, support n:1, 1:n, key-exchange and aes encrypt.
