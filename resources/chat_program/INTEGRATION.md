# chat_program integration

The runtime in this directory is pinned by Say Count. Keep project-specific
settings in `game/say_count_chat_config.rpy`; the installer never overwrites that
file after creating it.

If Say Count detects edits in the vendored runtime, its upgrade export is placed
beside the project's `game` directory. Compare and merge that export manually;
do not keep two active copies of `chat_program.rpy` under `game`.

Enter a chat scene, then return to normal visual-novel dialogue with:

```renpy
label example_scene:
    e "This uses the normal VN dialogue window."

    call say_count_chat_begin("#general")
    r "This appears in the chat screen." (c="#general")
    mc "So does this."
    call say_count_chat_end

    e "The chat screen is gone and normal VN dialogue is back."
```

`say_count_chat_begin` clears prior messages by default. Pass `clear=False` to
keep the current history. The optional channel argument selects the starting
channel. `say_count_chat_end` hides the chat screen and restores Ren'Py's
automatic dialogue-window behavior.

The upstream framework also customizes Ren'Py's `choice` screen to implement
chat-specific player auto-send behavior. Say Count does not replace
`screens.rpy`. Copy/adapt the `choice` screen from the pinned upstream project
when the game needs that behavior; ordinary menus and `auto_send=False` do not
require Say Count to overwrite project UI code.

To remove the integration, remove `game/vendor/chat_program/` and, if it has no
project customizations you want to keep, `game/say_count_chat_config.rpy`.
