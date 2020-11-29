# ACE ENGINE
This is a doom engine for unmodified DOS Doom 2.
Initial execution is achieved using map exploit. No patching required.
Engine is then started using following command:
`doom2 -file file.wad -warp 1`

## DOS Version
This engine only works in version 1.9 Doom 2.
That is, no The Ultimate Doom, no The Final Doom or Anthology.

(Why is there so many different 1.9 versions?)

## Files
Directory 'wads' contains various test releases.

- acetest.wad is latest released testing version
- demo.wad is latest released testing map with engine included

## Technical info

### Custom mods
There are two possible ways to add ACE ENGINE into your mods.

#### Copy all the lumps
Simple solution, just copy everything starting at `ACE_CODE` up to `ACE_END` **to the end of your WAD**. Including these.
Everything between these markers are ignored by the engine. Do not place your custom graphics here.
This is solution for WADs that do not break PWAD lump limit.
https://doomwiki.org/wiki/PWAD_size_limit

#### WAD in WAD
Simple to execute but harder to edit. If you need more lumps and do not want to split your mod into multiple WADs.
Get ACE ENGINE WAD and create single new lump called `ACE_WAD`, outside of markers mentioned above.
Import your entire WAD as raw data into `ACE_WAD`.
Your WAD can have much more lumps inside.
Note: If you have modified `PLAYPAL`, it must be included too.

#### Develompent
For simple testing, before you include `ACE_WAD`, you can name your wad as `ace_wad.lmp` and start the game with following command:
`doom2 -file ace.wad ace_wad.lmp -warp 1`

### Custom engine graphics
While most of the lumps between `ACE_CODE` and `ACE_END` are ignored, there are a few engine specific.

#### TITLEPIC
This is getting displayed when exploit is not triggered. If someone runs your mod without `-warp 1`.
Any other `TITLEPIC` outside `ACE_CODE` and `ACE_END` will work as intented.

#### _LOADING
Full screen loading background. Where progress bar is at 0%.
Smaller resolution would result in a mess. And offsets should be zero.
There is no information about progress bar location.

#### _LOADBAR
Loading progress bar. This is graphics for the progress bar. It is gradually displayed as progress moves.
This patch can have any size, even fullscreen. Use patch offsets to position it where you want your progress bar.
Progress bar is always drawn from left to right.

#### _NOFLAT
This is texture for "unknown flat texture".

#### _NOTEX
This is texture for "unknown wall texture".

### Source ports
While it is possible for this exploit to coexist with source ports, it won't work out of the box.
Source ports would have to ignore every single lump between `ACE_CODE` and `ACE_END` markers.
Also, source ports would have to implement `WAD in WAD` solution mentioned above.

### DECORATE
ACE ENGINE implements a limited subset of DECORATE. This subset is compatible with ZDoom.
While parser is different and likely allows stuff that is broken in ZDoom, there is always specific coding-style that should work the same in ZDoom and ACE ENGINE.
Never request feature suport in ZDoom if something works in ACE ENGINE but not in ZDoom. You should always fix your DECORATE so it works in both.

#### What does not work
- expressions
- `replaces` for built-in class replacement
- `inheritance`, you always have to write your actors from the scratch
  - there are few basic classes that can be used for special types that require this
- "archvile frames" - frames can only be `A` to `Z`
- special sprite names, such as `####`
- and probably much more

#### inheritance
Only special ZDoom actors can be used as a base for new actors.

- `FakeInventory`
- `PlayerPawn`

#### supported properties
Only properties included it this list are supported. Any unknown property will cause an error.

- `SpawnID`
- `Health`
- `ReactionTime`
- `PainChance`
- `Damage`
- `Speed`
- `Radius`
- `Height`
- `Mass`
- `ActiveSound`
  - also `use fail` for `PlayerPawn`
- `AttackSound`
- `DeathSound`
- `PainSound`
  - also used for `PlayerPawn`
- `SeeSound`
  - also `land` for `PlayerPawn`
- `Obituary` contents are ignored
- `HitObituary` contents are ignored
- `DropItem` without `amount` parameter
- `Monster` flag combo
- `Projectile` flag combo

Properties for `FakeInventory`:

- `Inventory.PickupSound`
- `Inventory.PickupMessage`

Properties for `PlayerPawn`:

- `Player.AttackZOffset`
  - will not work with original action pointers
- `Player.ViewHeight`
- `Player.JumpZ`
  - there is no `jump` key, yet
- `Player.MaxHealth`
- `Player.SpawnClass`
  - only numeric variant can be used (0 to 3)
- `Player.SoundClass`
  - ignored; only present for ZDoom compatibility; use sounds listed above

#### supported flags
- `special`
- `solid`
- `shootable`
- `nosector`
- `noblockmap`
- `ambush`
- `justhit`
- `justattacked`
- `spawnceiling`
- `nogravity`
- `dropoff`
- `pickup`
- `noclip`
- `slidesonwalls`
- `float`
- `teleport`
- `missile`
- `dropped`
- `shadow`
- `noblood`
- `corpse`
- `countkill`
- `countitem`
- `skullfly`
- `notdmatch`
- `noteleport`
- `ismonster`
- `boss`
- `seekermissile`
- `randomize`
- `notarget`
- `floorclip` ignored
- `noskin` ignored
- `dormant`
- `telestomp`

#### states
List of supported engine states:

- `spawn`
- `see`
- `pain`
- `melee`
- `missile`
- `death`
- `xdeath`
- `raise`

Custom states are supported in specific way and in limited amount.
Every custom state must start with `_` (underscore) character.
Custom names can have only up to 16 characters, not counting first underscore.

#### action pointers
List of supported action pointers:
Many original moster attacks are supported but those are not listed.

- `A_Look`
- `A_Chase` no parameters
- `A_Pain`
- `A_Fall`
- `A_Scream`
- `A_XScream`
- `A_PlayerScream`
- `A_FaceTarget` no parameters

More action pointers:

- `A_NoBlocking` with parameters
- `A_SpawnProjectile` with most parameters (`ptr` is not supported and flag `CMF_BADPITCH` is not supported)
- `A_JumpIfCloser` with parameters
- `A_Jump` with up to 5 different jump destinations

### Sounds
Currently no new sounds are supported. All sounds in DECORATE have to be specified by lump name, like `dspistol`.
For ZDoom compatibility you have to include your own SNDINFO.
In future, sound table will likely be generated from SNDINFO in very limited way - only 8 characters and logical name same as lump name.

### Exploit
TODO

