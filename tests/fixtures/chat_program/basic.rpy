# Adapted from robo-barbie/chat_program commit
# 85ee08dd3e09ebf4492159f350b3a70de530b0ef (Unlicense).
default r = ChatCharacter("Robobarbie", icon="images/Robo.png", name_color="#E4C443")
default mc = ChatCharacter("Player", is_player=True)

label chat_scene:
    r "Hello!" (c="#general", ot=[mc], fastmode=0.1)
    r "Still here."
