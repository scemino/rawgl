
# raw - Another World Interpreter

raw is a re-implementation of the engine used in the game Another World.

![Screenshot Intro Amiga](docs/screenshot-intro-amiga.png)

## Supported Versions

The program requires the original data files.

- Amiga (Bank*)
- Atari (Bank*)
- DOS (Bank*, memlist.bin)
- DOS demo (Demo*, memlist.bin)

## Run it

Drag'n'drop a zip containing the data files.

```text
  Usage: raw [OPTIONS]...
    file=PATH       Path to a zip file
    lang=LANG       Language (fr,us)
    part=NUM        Game part to start from (0-35 or 16001-16009)
    use_ega=true    Use EGA palette with DOS version
    protec=true     Enable game protection
```

In game hotkeys :

```text
  Arrow Keys      move Lester
  Enter/Space     run/shoot
  C               enter a code to start at a specific position
```

## Try it

You can play it online [here](https://scemino.github.io/raw_wp/) and drag'n'drop a zip containing the data files.

## Features

- load/save snapshot
- resources viewer (WIP)
- video window: with current palette and 4 framebuffers updated in realtime
- CPU debugger

## Credits

- **Gregory Montoir** for his awesome project [rawgl](https://github.com/cyxx/rawgl)
- **flooh** for his library [sokol](https://github.com/floooh/sokol)
- **Omar** for [dearimgui](https://github.com/ocornut/imgui)
- **Eric Chahi** for [Another world](http://www.anotherworld.fr/another_world.htm)
