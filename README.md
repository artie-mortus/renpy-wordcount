# Say Count Native

Say Count Native is a Linux C++17/wxWidgets conversion of the Say Count Ren'Py editor and dialogue word-counting application.

The last clean workspace is restored automatically, including open files,
caret/scroll positions, active tab, connected project, and dock layout. Use
`Ctrl+P` to search project files, labels, and characters, or `Ctrl+Shift+P` to
search application commands.

Use **File → Home** to start a story, open a game, open a loose script, or
return to recent work. New stories default to a familiar local folder, preview
their destination before creation, and never replace existing files silently.
The **Story Library** combines the project cast, image previews, and audio
previews in one searchable pane; its writing actions insert correctly indented
dialogue, backgrounds, images, music, and sounds at the cursor.

## Chat scenes (prose ↔ messenger)

Use **Edit → Turn Writing Into a Chat Scene…** to turn selected prose/dialogue
(or the whole active tab) into syntax for
[`robo-barbie/chat_program`](https://github.com/robo-barbie/chat_program).
Everything can be written as natural language: `Me:` marks the player,
screenplay-style stage directions handle chat mechanics (`[in #ops]`,
`[Robo is typing]`, `[fast]`, `[normal speed]`, `[I am Eileen]`), and a
`Choice:` line followed by `- option` lines (with indented follow-up) becomes a
player-reply menu — the converter writes all of the code. Pick
a look (Discord-style channels or Kik-style messenger), check the
**What players will see** preview tab — the raw generated code is on the
**Script code** tab — and click **Use this** to replace the source as one
undoable edit. By default the output is a complete scene that opens the chat
app and returns to normal VN dialogue at the end; narrator handling and
character short-code mapping live under **Advanced options**. Use
**Turn Chat Scene Back Into Dialogue…** for the reverse direction, either as
readable `Name: text` manuscript or ordinary Ren'Py say statements; the preview
lists chat-only channel, typing, timing, and choice metadata that regular
dialogue cannot keep. The in-app walkthrough is at
**Help → Chat Format Guide…**.

For a connected Ren'Py project, **Edit → Install/Update Chat App Files…**
installs the pinned, licensed upstream runtime under `game/vendor/chat_program`
(Say Count also offers this automatically after a conversion when the runtime
is missing). Existing files that differ are treated as local customizations and
are never overwritten. When the runtime itself is customized, Say Count can
export the pinned upgrade beside the project for manual comparison; it does not
place a second active runtime inside `game/`.
Say Count does not edit `screens.rpy`; projects using chat-specific choice-screen
behavior should apply the upstream screen customization manually.

Use `call say_count_chat_begin("#general")` to enter the installed chat screen
and `call say_count_chat_end` to hide it and resume normal VN dialogue. See the
complete transition example in `resources/chat_program/INTEGRATION.md`. The
skin argument (`say_count_chat_begin(..., skin="kik")`) selects the messenger
look per scene; the project default lives in `game/say_count_chat_config.rpy`
(`say_count_chat_skin`).

## Build

Install CMake, a C++17 compiler, Git, wxWidgets 3.2 or newer with the `core`,
`base`, `stc`, `aui`, `adv`, `html`, `xml`, and `media` components, and network
access for CMake to fetch Catch2 the first time. Then run:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

If Catch2 cannot be downloaded, configure again after restoring network access or provide a pre-populated FetchContent source/cache for Catch2 v3. The application binary is `build/say-count`.

Use **Edit → Vim Motions (Built-in)** for normal, operator-pending, visual,
search, and command-line editing. The modal engine runs entirely inside Say
Count, with no Neovim installation or subprocess. Insert mode remains in the
native editor so autocomplete, snippets, and Ren'Py indentation work normally.

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

## Git repositories

Open **File → Git Repository** or use the **Git** command-bar button. The
repository desk works with standard Git remotes rather than a hosting-service
API, so the same workflow supports GitHub, GitLab, Bitbucket, Codeberg,
self-hosted servers, local repositories, and other Git-compatible services.

- **Open local repository** connects a repository that is already on this PC
  and uses its configured push remote. **Clone repository** accepts HTTPS, SSH,
  `file://`, and local repository paths, then connects the cloned Ren'Py project
  folder.
- **Initialize** turns the connected project folder into a repository.
- **Set origin** adds or changes the default remote without restricting its
  host.
- **Fetch** updates references from every configured remote. **Pull changes**
  accepts clean, fast-forward updates only, avoiding implicit merge commits.
- **Commit & push** saves open scripts, stages all repository changes, creates
  one commit, and pushes the current branch to its configured push remote. The
  first push uses the repository's default remote and establishes its upstream.

Private access is handled by the installed Git client. Configure an SSH agent
and authorized key, or a system Git credential helper and access token, using
the normal instructions for the chosen host. Say Count neither requests nor
stores repository credentials. Commit creation also uses the standard Git
`user.name` and `user.email` configuration.

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
