# Arbitrary code execution in DOOM 2
This is a set of Doom 2 enhancements by using code execution exploit.

## Version
This code only works in Doom 2 version 1.9.
That is, no The Ultimate Doom, no The Final Doom, no Anthology.
The same version is distributed with SHAREWARE.

(Why are there so many different 1.9 versions?)

## How to run this
Generated binary is called ACE. It has to be provided as a command line parameter to the game as response file.

That is `doom2 @ACE`. This parameter **must** be first, more parameters are optional and allowed.

## Code
Code is split into three distinct parts.

### Exploit
This file is used as a `response file` and a `config file` at the same time.

### Code
TODO

### Engine
TODO

## Exploit
This enhancement exploits a stack overflow in function `M_LoadDefaults`. However, due to the random memory layout the exploit chain is a bit more complicated.

TODO: explain
