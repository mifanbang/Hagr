# Hagr

Bridging Nintendo Switch Pro controller and XInput

## Introduction

Nintendo Switch Pro controller is a very well designed but also expensive product. Unfortunately it doesn't work on PC out of the box. The reason is that most modern PC games use [XInput](https://en.wikipedia.org/wiki/DirectInput#XInput) to communicate with gamepads but XInput doesn't support Pro controller. Games on Steam may support the device but that's not good enough.

*Hagr*, meaning skilled or handy in Old Norse, is a tool that communicates with the device and mimics XInput interface so that games can pick up signals from a Pro controller without any modification.

## How to Use

Hagr comes in the form of a set of DLLs. There are four DLLs in total and you have to copy them to the folder of your game, *i.e.*, the folder where you can see the EXE file of the game. Remember to back up any `xinput*.dll` file that already exists. Normally a game only needs one or two of the DLLs. Nevertheless, as it's tedious to check which ones your game really needs, just copy all of them and we can call it a day.

Please note that you DO need to check beforehand whether the game in question is 32-bit or 64-bit. Here's a [quick guide](https://superuser.com/questions/358434/how-to-check-if-a-binary-is-32-or-64-bit-on-windows#889267) of doing it. A 32-bit EXE is unable to load a 64-bit DLL and vise versa.

## Building the Code

Just build `hagr.sln` with Visual Studio, preferably 2019. Output binaries will then be located inside `bin/` folder. In the output folder you will also be able to see `TestMe.exe`. It's a simple test program which sends queries to XInput and shows results at a rate of about 60 ticks per second.

## Limitations

Hagr is an experimental project. I hope it works for as many use cases as possible but please be expecting situations where it doesn't. There are several limitations which may or may not be resolved in the future.

- Only **one** controller is supported, and it has to be a Pro controller.
- Only wired connection is supported.
- Thumbstick calibration values are currently hard-coded.
- Vibration via `XInputSetState()` is currently not implemented.
- There's no guarantee that every game using XInput will load the DLLs. Some games have unique ways to start up. 

## An Incomplete List of Working Games

This is a non-exhaustive list. It only shows those that have been tested by me. 

- Borderlands 3 (64-bit; in `OakGame/Binaries/Win64/` folder)
- Overwatch (64-bit; in `_retail_/` folder)

## TODO List

- `XInputSetState()` support.
- Better ways of displaying Hagr status.

## Acknowledgement

This project wouldn't be useful in any way without the following pioneer works:

- [SDL](https://hg.libsdl.org/SDL/file/tip/src/joystick/hidapi/SDL_hidapi_switch.c)
- [dekuNukem/Nintendo_Switch_Reverse_Engineering](https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering)

## Copyright

Copyright (C) 2020 Mifan Bang <https://debug.tw>.

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program.  If not, see <https://www.gnu.org/licenses/>.
