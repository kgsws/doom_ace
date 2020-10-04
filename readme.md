# Arbitrary code execution in DOOM
This is an example of exploit for original DOS doom save game file.

## Version
This example only works in version 1.9 Doom 2.
That is, no Doom2, no The Final Doom or Anthology.

(Why is there so many different 1.9 versions?)

## Example
Example in provided in `savegame/doomsav0.dsg` is a simple snake game.

- it's a snake!
- disables wipe while running
- unloads itself on ESC key
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

#### Doom 2-loaders
While, The Ultimate Doom is compiled with memory map where state number 3191 action pointer overlaps `savebuffer` variable, Doom 2 does not.
Therefore, we need another nice entry point.
Fortunately, variable `sectors` happens to be usable as action pointer for frame 3283. Furthermore, most of the values that end up here are stored in savegame itself!

This is not for free though, `sectors` is not copied from saved game directly but processed a bit. First two 32bit values are loaded and shifted by 16 bits to the left from 16bit value.
This means that first two bytes will always end up as 0x00 0x00.
Fortunately, this translates to the opcode `add	%al,(%eax)` and sice `eax` contains valid address, we can execute it.
To simplify things i made pre-loader that just jumps to actual save buffer so i can use loader i have for The Ultimate Doom, with minimal modifications of course.

```
_start:
floorheight:
	add	%al,(%eax)	// 0x00 0x00 (forced by the engine - not present in savegame)
	jmp	ceilingheight + 2
ceilingheight:
	add	%al,(%eax)	// 0x00 0x00 (forced by the engine - not present in savegame)
it_starts_here: // 12B available
	mov	(%esp),%eax	// read CODE pointer // 0x0013d095
	mov	0x001311a4-0x0013d095(%eax),%eax	// get savebuffer variable location
	jmp	*(%eax)	// and jump to its destination
```
(Yes, this code is stored in sector 0. And only 1 byte was left unused.)

While save game buffer is freed using `Z_Free` before it can be executed, there is nothing that would overwrite it before code execution happens.
Any persistent code would have to be copied somewhere else though.

#### Other versions
There are way more entry points - if you use custom PWAD. Many lumps are cached before game (status bar, etc ...).
Many of these pointers are available as a state action. Each Doom version has different possibilities.
Code can be executed directly from any such lump.

### Loader
Provided loader is only supposed to load main code from freed savegame to memory it allocates.

### Binaries
There is currently only one example and that is SNAKE! game.

#### Loading custom code
You have to modify provided example savegame file.

- loader starts at offset 0x22DA
- if you do not need to modify the loader, game starts at 0x23C0

### Future
This allows for far advanced Doom mods than dehacked alone ever allowed.
It is possible to load savegame when game start if specified in command line parameters. Mods could provide exploited savegame to be used with it.
https://www.doomworld.com/forum/topic/117123-dos-doom-code-execution/

## Credits
Special thanks to `Randy87` for uploading their map files way back. This made it much quicker.

https://www.doomworld.com/forum/topic/86380-exe-hacking/?tab=comments#comment-1567318

