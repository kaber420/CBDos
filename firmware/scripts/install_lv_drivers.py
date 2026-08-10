Import("env")

if env["PIOENV"] == "emulator":
    env.Append(
        LIBS=["SDL2", "SDL2main"],
        LINKFLAGS=["-lSDL2", "-lSDL2main"],
    )