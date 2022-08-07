# Arbitrary code execution in DOOM 2
This is a set of Doom 2 enhancements by using code execution exploit.

## Version
This code only works in Doom 2 version 1.9.
That is, no The Ultimate Doom, no The Final Doom, no Anthology.
The same version is distributed with SHAREWARE.

(Why are there so many different 1.9 versions?)

## How to run this
Compile `exploit` and `code`.
Exploit generates a WAD file with two entries, `ACE_LDR` and `ACE_CODE`. Replace `ACE_CODE` with generated `code.bin` in any WAD editor.

Resulting WAD file must be run with command `doom2 -config ace.wad`.

## Code
Code is split into two distinct parts.

### Exploit
This file is used as a `config file`. It is specially crafted so it appears as a `WAD` file too.

#### BEWARE
Resulting WAD file must not contain byte 0x1A in the header. You have to check if `directory offset` or `entry count` does not contain this value.

### Engine
This is a source of the entire ACE Engine. Resulting binary `code.bin` has to be placed into generated WAD as `ACE_CODE`.

## Exploit
This enhancement exploits a stack overflow in function `M_LoadDefaults`. However, due to the random memory layout the exploit chain is a bit more complicated.

TODO: explain
