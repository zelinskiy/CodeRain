# Matrix Screensaver

(WIP) A minimal Matrix-style screensaver for Windows 11.

## Build
- `cl /std:c17 /W4 /TC /utf-8 main.c /Fe:coderain.scr /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib shell32.lib`

## Installation
- Copy `coderain.scr` to `C:\Windows\System32` (Requires admin privilege)
- Make sure `settings.ini` and specified `file_source.txt` are in `%localappdata%\Coderain`
- Set coderain as your screensaver via Control Panel

## Quick tips
- Make sure selected font is installed on your system
- Default settings for each available language provided in settings directory
- Errors logged in `%localappdata%\Coderain\err.log`

## TODO
- find out why reading data from file peaks CPU usage (possibly fully read to RAM)
- performance optimizations
- also spawn above screen border?
- z depth effect?
- allow streams collision?
- random char size for each stream?
- csl: aspiration overlaps with accent
- csl: circumflexus centering
- csl: fix accented оук letter
- csl: negative top spacing for accented ять
- csl: цы/рцы + аз, ...: can add negative bottom spacing for them
- csl: fix capital letters (or just convert them to lowercase?)

## Goals

- Minimal CPU usage
- No third-party dependencies
- Native Windows screensaver integration
- No extra visual effects