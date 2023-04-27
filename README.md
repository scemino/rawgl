
# raw - Another World Interpreter

raw is a re-implementation of the engine used in the game Another World.

![Screenshot Intro Amiga](docs/screenshot-intro-amiga.png)

## Supported Versions

The program requires the original data files.

- Amiga (Bank*)
- Atari (Bank*)
- DOS (Bank*, memlist.bin)
- DOS demo (Demo*, memlist.bin)

## Running

Drag'n'drop a zip containing the data files.

```text
  Usage: raw [OPTIONS]...
    file=PATH   Path to a zip file
    lang=LANG   Language (fr,us)
    part=NUM    Game part to start from (0-35 or 16001-16009)
    use_ega     Use EGA palette with DOS version
```

In game hotkeys :

```text
  Arrow Keys      move Lester
  Enter/Space     run/shoot
  C               enter a code to start at a specific position
  P               pause the game
```

## Features

- load/save snapshot
- resources viewer (WIP)
- video window: with current palette and 4 framebuffers updated in realtime
- CPU debugger

## Technical Details

- [Amiga/DOS](docs/Amiga_DOS.md)
- [3DO](docs/3DO.md)
- [WiiU](docs/WiiU.md)
