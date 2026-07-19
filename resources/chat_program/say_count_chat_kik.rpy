# Kik-style messenger skin for the chat_program runtime, vendored by Say Count.
# Renders the same message stores as chat_messages_view in a portrait phone
# layout: one conversation at a time, chat bubbles, and a thread list.
# Placeholder art is generated from solid colors; replace the style constants
# below from say_count_chat_config.rpy or your own script if desired.

default say_count_kik_player_cache = None

init 2 python:
    def say_count_kik_refresh_players():
        """Collect display names of every ChatCharacter marked is_player."""
        names = set()
        for key in dir(store):
            value = getattr(store, key, None)
            if isinstance(value, ChatCharacter) and getattr(value, "is_player", False):
                names.add(value.name)
        store.say_count_kik_player_cache = names
        return names

    def say_count_kik_is_player(name):
        cache = store.say_count_kik_player_cache
        if cache is None:
            cache = say_count_kik_refresh_players()
        return name in cache

screen say_count_kik_view():
    default show_threads = False
    zorder 90

    frame:
        xalign 0.5
        yalign 0.5
        xsize 540
        ysize min(1020, config.screen_height - 40)
        background Solid("#101418")
        padding (10, 10)

        vbox:
            spacing 8

            frame:
                background Solid("#1B2530")
                xfill True
                ysize 84
                hbox:
                    yalign 0.5
                    spacing 16
                    textbutton "≡":
                        yalign 0.5
                        text_size 34
                        text_color "#8FE388"
                        text_hover_color "#D6FF1B"
                        action ToggleScreenVariable("show_threads")
                    vbox:
                        yalign 0.5
                        text current_window size 30 color "#FFFFFF"
                        if current_window == active_window and who_is_typing:
                            text who_is_typing size 18 color "#8FE388"

            if show_threads:
                viewport:
                    ysize 760
                    scrollbars "vertical"
                    mousewheel True
                    has vbox
                    spacing 6
                    for thread in channels.keys():
                        textbutton thread:
                            xfill True
                            text_size 28
                            text_color ("#EAC119" if channels_new_message[thread] else "#FFFFFF")
                            text_hover_color "#8FE388"
                            action [SetDict(channels_new_message, thread, False),
                                    SetVariable("current_window", thread),
                                    SetScreenVariable("show_threads", False)]
            else:
                viewport yadjustment yadj:
                    ysize 760
                    scrollbars "vertical"
                    mousewheel True
                    has vbox
                    box_wrap True
                    spacing 14
                    for chat in channels[current_window]:
                        if say_count_kik_is_player(chat["name"]):
                            frame:
                                xalign 1.0
                                xmaximum 360
                                background Solid("#2E7D32")
                                padding (14, 10)
                                text chat["message"] size 24 color "#FFFFFF"
                        else:
                            hbox:
                                xalign 0.0
                                spacing 10
                                if chat["include_icon"] and chat["icon"]:
                                    add chat["icon"] yalign 0.0
                                else:
                                    null width 70
                                frame:
                                    xmaximum 340
                                    background Solid("#232B36")
                                    padding (14, 10)
                                    vbox:
                                        if chat["include_name"]:
                                            text chat["name"] size 20 color chat["name_color"] bold True
                                        text chat["message"] size 24 color "#FFFFFF"

            frame:
                background Solid("#1B2530")
                xfill True
                ysize 72
                hbox:
                    xalign 0.5
                    yalign 0.5
                    spacing 34
                    textbutton ("Resume" if is_paused else "Pause"):
                        yalign 0.5
                        text_size 24
                        text_color "#FFFFFF"
                        text_hover_color "#D6FF1B"
                        action If(is_paused,
                                  [SetVariable("is_paused", False), SetVariable("_preferences.afm_enable", True)],
                                  [SetVariable("is_paused", True), SetVariable("_preferences.afm_enable", False)])
                    textbutton "1x":
                        yalign 0.5
                        text_size 24
                        text_color ("#D6FF1B" if chat_speed == 1 else "#FFFFFF")
                        action SetVariable("chat_speed", 1)
                    textbutton "2x":
                        yalign 0.5
                        text_size 24
                        text_color ("#D6FF1B" if chat_speed == 2 else "#FFFFFF")
                        action SetVariable("chat_speed", 2)
                    textbutton "Skip":
                        yalign 0.5
                        text_size 24
                        text_color ("#D6FF1B" if chat_speed == 100 else "#FFFFFF")
                        action SetVariable("chat_speed", 100)
