# ACE Engine for DOOM 2

This is a DOS Doom2 enhancement contained in a single WAD file.

## Version

This code only works in Doom 2 version 1.9.
That is, no The Ultimate Doom, no The Final Doom, no Anthology.
The same version is distributed with SHAREWARE.

(Why are there so many different 1.9 versions?)

## How to run this

Compile `exploit` and `engine`.
Engine is the main thing. After compiling you get `code.lmp`.
Exploit generates a WAD file with multiple entries. Replace `ACE_CODE` with generated `code.lmp` in any WAD editor.

Resulting WAD file is run just like any other WAD, using command `doom2 -file ace.wad`.

## Documentation

[read here](doc/main.md)

## Code

Code is split into two distinct parts.

### Exploit

There are two bugs used. First is negative memory allocation in ZONE, second is out of bounds read from stack.
This allows specially crafted WAD file to inject new code into already running game.
TODO: explain the exploit chain

### Engine

This is a source of the entire ACE Engine. Resulting binary `code.lmp` has to be placed into generated WAD as `ACE_CODE`.  
NOTE: Everytime you pull new version make sure you use command `make clean` before building.
