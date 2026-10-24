# Matrix Screensaver

A minimal Matrix-style screensaver for Windows 11.

## TODO
- csl: ц + а, ...
- fix csl capital letters (or just convert them to lowercase?)
- read characters until the end of word
- Split into separate procedures
- rotators (single characters which change every tick)
- various modes: 100% filled screen, gradual filling, console (?), etc.
- User-defined: charset, font, cell_width, cell_height, etc.
- User-defined source of streams: books, quotes, etc.

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