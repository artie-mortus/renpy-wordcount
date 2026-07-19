# Say Count bridge for entering and leaving chat_program scenes.
# This file is vendored and may be updated independently of project settings.

init 2 python:
    def say_count_select_chat_channel(channel):
        if not isinstance(channel, str) or not channel:
            raise Exception("Chat channels must be non-empty strings.")
        store.current_window = channel
        store.active_window = channel
        store.last_window = channel
        if channel not in store.channels:
            store.channels[channel] = []
        if channel not in store.channels_new_message:
            store.channels_new_message[channel] = False
        if channel not in store.channels_last_sender:
            store.channels_last_sender[channel] = None

label say_count_chat_begin(channel=None, clear=True):
    if channel is not None:
        $ say_count_chat_initial_channel = channel
    if clear:
        $ say_count_reset_chats()
    elif channel is not None:
        $ say_count_select_chat_channel(channel)
    window hide
    show screen chat_messages_view
    return

label say_count_chat_end:
    hide screen chat_messages_view
    window auto
    return
