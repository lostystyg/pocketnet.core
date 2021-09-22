CMake build guide
======================

### command:
```code
mkdir build && cd build
cmake -DCMAKE_CXX_STANRDARD=17 .. 
```

### Available options:

- DISABLE_ZMQ
  - Disable ZMQ notifications (default is OFF).
  - [*NOTE*] - pure find_library is currently used to search for zmq. This means that zmq should be located in *default* libraries paths and include directories should be manually provided by -DCMAKE_CXX_FLAGS=-I\ /path/to/include
- WITH_SYSTEM_UNIVALUE
  - Default is **OFF**
  - Build with system UniValue (default is OFF)
- ENABLE_ASM
  - Enable assembly routines (default is ON)
- BDB_ROOT
  - Path to BerkleyDB 4.8
- DISABLE_WALLET
  - Disable wallet (enabled by default)
- ENABLE_GLIBC_BACK_COMPAT
  - enable backwards compatibility with glibc
- WITH_INCOMPATIBLE_BDB
  - Allow using a bdb version greater than 4.8
- EVENT_ROOT
  - Path to root of libevent library
  - If not specified - default library paths are used
- SQLITE_ROOT
  - Path to root of sqlite3 library
  - If not specified - default library paths are used
- OPENSSL_ROOT_DIR
  - Path to root if openssl library
  - If not specified - default library paths are used
- BOOST_ROOT
  - Path to root of boost
  - If not specified - default library paths are used
  - [*NOTE*] Pass here path to *path_to_boot/lib/cmake* folder instead of *path_to_boost/* if you have such
- MSVC_FORCE_STATIC
  - Build with MTd runtime linking. Use this if you want to statically link internal libraries. Ignored for non-MSVC build (default=ON)

There are also some hints options available for Boost and OpenSSL, see CMake module documentation for them. 

All options are specified like this: **-D**OPTION=VALUE\
If value is boolean - pass ON or OFF as a value

### Suggestions for Windows building
- Build Boost from source.
  - Use **b2** to build for msvc toolchain or **b2 toolset=gcc** for mingw/cygwin build
  - [*NOTE*] **b2** without arguments will build libraries with **dynamic runtime**. If you want to build boost for static runtime linkage call **b2 runtime-link=static runtime-debugging=on** 
  *runtime-debgging=on* should be specified because pocketnet.core requires runtime assertions that are available only on Debug build and mixing up debug and release runtime static linking with MSVC will result in link error.
  - After compilation use **b2 install --prefix path_to_boost/** and use this path_to_boost/ for with BOOST_ROOT option 

- Use **vcpkg** for other dependencies
  - ```code
    git clone https://github.com/Microsoft/vcpkg.git
    cd vcpkg
    ./bootstrap-vcpkg.sh
    ./vcpkg integrate install
    ```
  - E.x. **vcpkg install libevent:x64-windows-static** and specify **-DEVENT_ROOT=path_to_vcpkg/packages/libevent_x64-windows-static** for correct linking with libevent
  - [*NOTE*] If using non-statically build dependencies - consider copy dll files (*path_to_vcpkg*/packages/*package_name*/bin/) to place where binaries are located (${CMAKE_BINARY_DIR}/src). Also provide *-DMSVC_FORCE_STATIC=OFF* as a cmake option to allow dynamic linking
  - [**IMPORTANT**] Replace libraries in "*path_to_vcpkg*/packages/*package_name*/lib" with those which are located under "debug" folder of package. See boost notes for explaining.

- If building dependencies from source - specify MTd (Static debug runtime linker):
  - CMake:
    - There are libraries provided specific option for this (e.x. libevent)
    - Otherwise pass -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreadedDebug" as a cmake option
  - Others:
    - Pass "/MTd" as a compiler option (without "-")