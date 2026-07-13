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

To build wxWidgets 3.2.11 from source and link its static libraries into Say
Count (system GTK and multimedia libraries remain dynamic), use:

```sh
cmake -B build-portable -DSAY_COUNT_BUNDLED_WX=ON -DBUILD_TESTING=OFF
cmake --build build-portable -j
```

The normal build continues to use system wxWidgets. For offline bundled builds,
provide `FETCHCONTENT_SOURCE_DIR_WXWIDGETS=/path/to/wxWidgets` at configure time.

## Install and package

Install the executable, desktop entry, application icon, and Ren'Py MIME type
under a chosen prefix:

```sh
cmake --install build-release --prefix "$HOME/.local"
cmake --build build-release --target uninstall
```

Installed `.rpy` files can be opened through the desktop entry or passed on the
command line. To stage and build an AppImage, install `linuxdeploy` and
`appimagetool`, then run `tools/build-appimage.sh`. Use `--appdir-only` to
inspect the staged AppDir without those packaging tools. Tagged builds can also
produce the artifact through the included GitHub Actions workflow.

## Reference implementation

`/home/artemis/Projects/words-til-vn` is the behavioral reference and is **strictly read-only**. Never edit, format, generate files in, or commit from that repository.
