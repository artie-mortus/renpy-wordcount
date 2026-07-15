# Say Count Native

Say Count Native is a Linux C++17/wxWidgets conversion of the Say Count Ren'Py editor and dialogue word-counting application.

The last clean workspace is restored automatically, including open files,
caret/scroll positions, active tab, connected project, and dock layout. Use
`Ctrl+P` to search project files, labels, and characters, or `Ctrl+Shift+P` to
search application commands.

## Build

Install CMake, a C++17 compiler, wxWidgets 3.2 or newer with the `core`, `base`, `stc`, `aui`, `adv`, `html`, `xml`, and `media` components, libcurl, OpenSSL, a Secret Service keyring implementation such as GNOME Keyring, and network access for CMake to fetch Catch2 the first time. Then run:

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
cmake --build build-portable -j2
```

The normal build continues to use system wxWidgets. For offline bundled builds,
provide `FETCHCONTENT_SOURCE_DIR_WXWIDGETS=/path/to/wxWidgets` at configure time.

## Google Drive cloud saves

Open **File → Google Drive Cloud Saves** or use the **Cloud** command-bar
button. Say Count saves one complete project bundle per project name in Google
Drive's private `appDataFolder`. These backups don't appear among normal Drive
documents and only this OAuth application can read them. Restoring validates the
bundle, snapshots the outgoing local editor state, and leaves restored tabs
unsaved so disk files are never overwritten silently.

Google Drive access needs a Desktop app OAuth client:

1. In [Google Cloud Console](https://console.cloud.google.com/), create or select
   a project and enable the Google Drive API.
2. Configure the OAuth consent screen. While the app is in testing, add each
   Google account that will use cloud saves as a test user.
3. Create an OAuth client with application type **Desktop app**, download its
   JSON file, then choose **Import OAuth client...** in the cloud-save dialog.
4. Choose **Connect Google Drive** and complete consent in the browser.

The imported client file is stored with owner-only permissions under the Say
Count data directory. The refresh token is stored in the operating system
keyring, never in `settings.json`. For development or managed deployments, set
`SAY_COUNT_GOOGLE_CLIENT_ID` and optional `SAY_COUNT_GOOGLE_CLIENT_SECRET`
instead of importing a file. The native OAuth flow uses PKCE, a random-state
check, and a temporary `127.0.0.1` callback.

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

## Parity notes

The final audit is recorded in `PARITY-CHECKLIST.md`. Browser-only storage and
download APIs are intentionally replaced by atomic native disk saves, project
folders, and snapshots. Native dockable panes replace the browser splitter,
and application chrome follows the desktop theme while editor themes remain
selectable. The audit classifies every reference feature; none remain pending.

## Reference implementation

`/home/artemis/Projects/words-til-vn` is the behavioral reference and is **strictly read-only**. Never edit, format, generate files in, or commit from that repository.
