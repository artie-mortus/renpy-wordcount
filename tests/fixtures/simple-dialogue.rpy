label start:
    e "Hello there."
    "Narration counts."
    extend "And continues."
    scene bg room
    $ debug = "not dialogue"
    jump ending

label ending:
    "Done."
