default r = ChatCharacter("Robo")
default mc = ChatCharacter("Player", is_player=True)

label start:
    return

label chat_demo:
    "This is normal visual-novel dialogue."
    call say_count_chat_begin("#general")
    r "Hello!" (c="#general", ot=mc, fastmode=0.1)
    menu:
        "Reply" (auto_send=False):
            mc "Hi."
        "Ignore" (auto_send=False):
            pass
    call say_count_chat_end
    "This is normal visual-novel dialogue again."
    return

label kik_demo:
    call say_count_chat_begin("Robo", skin="kik")
    r "hey, you up?"
    mc "yeah what's up"
    call say_count_chat_end
    "Back to normal visual-novel dialogue."
    return

label chat_runtime_test_target:
    $ chat_speed = 100
    call say_count_chat_begin("#general")
    $ assert renpy.get_screen("chat_messages_view") is not None
    $ r("Hello!", c="#general", fastmode=0)
    $ assert len(channels["#general"]) == 1 and channels["#general"][0]["message"] == "Hello!"
    call say_count_chat_end
    $ assert renpy.get_screen("chat_messages_view") is None
    call say_count_chat_begin("Robo", skin="kik", clear=True)
    $ assert renpy.get_screen("say_count_kik_view") is not None
    $ assert renpy.get_screen("chat_messages_view") is None
    $ r("kik bubble", c="Robo", fastmode=0)
    $ assert say_count_kik_is_player("Player") and not say_count_kik_is_player("Robo")
    call say_count_chat_end
    $ assert renpy.get_screen("say_count_kik_view") is None
    "Normal VN gameplay restored."
    $ last_window = "#side"
    $ r("Side channel", c="#side", fastmode=0)
    $ assert active_window == "#side" and len(channels["#side"]) == 1
    $ say_count_chat_initial_channel = "#configured"
    $ say_count_reset_chats()
    $ assert current_window == "#configured" and "#configured" in channels
    menu:
        "Choose A":
            $ runtime_choice = "a"
        "Choose B":
            $ runtime_choice = "b"
    $ assert runtime_choice == "a"
    "Chat runtime smoke complete."
    return
