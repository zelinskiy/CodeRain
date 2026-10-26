# Matrix Screensaver

A minimal Matrix-style screensaver for Windows 11.

## TODO
- find out why reading data from file peaks CPU usage (possibly fully read to RAM)
- performance optimizations
- also spawn above screen border?
- add more example settings
- settings validation with appropriate error messages (use alert windows)
- installation readme section
- windows screensaver params section
- csl: aspiration overlaps with accent
- csl: circumflexus centering
- csl: fix accented оук letter
- csl: negative top spacing for accented ять
- csl: цы/рцы + аз, ...: can add negative bottom spacing for them
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