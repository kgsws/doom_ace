# Arbitrary code execution in DOOM
This is an example of exploit for original DOS doom save game file.

## Version
This example only works in version 1.9 The Ultimate Doom.
That is, no Doom2, no The Final Doom or Anthology.

(Why is there so many different 1.9 versions?)

## Example
Provided example does minor modifications to the code, just as an example.

- replace TITLEPIC
- replace menu skull
- invert the function of 'run key' (you can use run key to slow down now)

## Possibilities
This exploit has full access to the game code. It allows way more advanced modifications than dehacked.

## Technical info

### Exploit
Exploit uses PSPR code pointer to run the code present in save game file. First instruction run is the first byte in the file.

### Bug
Function `P_UnArchivePlayers` does no bounds checking on player sprite state. Therefore, by modifying saved `player_t` structure it is possible to interpret any memory offset as animation frames.

### Action pointer
State number stored in `player_t` is only used as an reference since it has already been executed prior to saving the game. Thus, its action code pointer will not be executed.
However, this state is used as an lookup for for the next state. Action pointer of next state, if set, will be executed.

### Memory
Doom HEAP is allocated at random location. Therefore, it is not possible to know the state number of allocated save data buffer.

However, function `P_UnArchivePlayers` always copies saved `player_t` structure to `players[0]` variable, which has known location. Furthermore, this location has known offset from `states[NUMSTATES]` array.

States 3144 to 3152 are located in memory we control.

### More memory
Doom CODE and DATA is also allocated at random location. The only possible fixed location is video RAM. This would require custom PWAD with executable code hidden in graphics (like STDISK or STBAR).

#### The Ultimate Coincidence
Coincidentally, The Ultimate Doom is compiled with memory map where state number 3191 action pointer overlaps `savebuffer` variable. This variable holds pointer to the full contents of save game file.
Therefore, save game file itself can be executed as an PSPR action code pointer.

While save game buffer is freed using `Z_Free` before it can be executed, there is nothing that would overwrite it before code execution happens.
Any persistent code would have to be copied somewhere else though.

### Modifying example
This repository contains example code and example binary save file. You can test save game as provided, it has example code already included.

If you want to modify the code, just truncate save size to 0x22DA (8922 bytes) and append your own code at the end.

## Credits
Special thanks to `Randy87` for uploading their map files way back. This made it much quicker.

https://www.doomworld.com/forum/topic/86380-exe-hacking/?tab=comments#comment-1567318

