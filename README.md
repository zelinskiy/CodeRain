# Matrix Screensaver

A minimal Matrix-style screensaver for Windows 11.

## TODO
- csl: aspiration overlaps with accent
- csl: circumflexus centering
- csl: цы/рцы + аз, ...: can add negative spacing for them
- csl: fix capital letters (or just convert them to lowercase?)
- read characters until the end of word
- User-defined settings file: charset, font, cell_width, cell_height, etc.
- various modes: 100% filled screen, gradual filling, console (?), "Wake up, Neo...", etc.


## Tech stack

- C17
- MSVC
- Win32 API
- GDI

## Goals

- Minimal CPU usage
- No third-party dependencies
- Native Windows screensaver integration
- Configurable animation speed
- No unnecessary visual effects