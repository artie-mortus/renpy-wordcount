# Say Count Native

Say Count Native is a Linux C++17/wxWidgets conversion of the Say Count Ren'Py editor and dialogue word-counting application.

## Build

Install CMake, a C++17 compiler, wxWidgets 3.2 or newer with the `core`, `base`, `stc`, `aui`, `adv`, `html`, and `xml` components, and network access for CMake to fetch Catch2 the first time. Then run:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

If Catch2 cannot be downloaded, configure again after restoring network access or provide a pre-populated FetchContent source/cache for Catch2 v3. The application binary is `build/say-count`.

For an optimized stripped build:

```sh
cmake -B build-release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-release -j
```

Release binaries embed the CMake project version. When no build type is given,
CMake defaults to `Release`; pass `-DCMAKE_BUILD_TYPE=Debug` for development.

## Reference implementation

`/home/artemis/Projects/words-til-vn` is the behavioral reference and is **strictly read-only**. Never edit, format, generate files in, or commit from that repository.
