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

## Presentation skins

Two vendored skins render the same message engine:

- `discord` (default): the upstream `chat_messages_view` screen with a channel
  sidebar.
- `kik`: `say_count_kik_view` in `say_count_chat_kik.rpy`, a portrait phone
  with left/right chat bubbles and a tap-to-open thread list. Name channels
  after contacts (`c="Eileen"`) so they read as direct-message threads.
  Messages from any `ChatCharacter(..., is_player=True)` align right.

Select per scene with `call say_count_chat_begin("Eileen", skin="kik")`, or set
the project default in `say_count_chat_config.rpy` via `say_count_chat_skin`.
The Kik skin uses solid-color placeholder art; adjust its style constants by
editing a copy of the styles in your own files rather than the vendored screen.

The upstream framework also customizes Ren'Py's `choice` screen to implement
chat-specific player auto-send behavior. Say Count does not replace
`screens.rpy`. Copy/adapt the `choice` screen from the pinned upstream project
when the game needs that behavior; ordinary menus and `auto_send=False` do not
require Say Count to overwrite project UI code.

To remove the integration, remove `game/vendor/chat_program/` and, if it has no
project customizations you want to keep, `game/say_count_chat_config.rpy`.
