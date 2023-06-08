# Configuration

ACE Engine uses different configuration file as it adds new options. Refer to **Engine** section.

ACE Engine features (optional) mod configuration which is present in WAD file. This lump, `ACE_CONF` is intended for mod authors to tweak specific features.
This configuration **can not** be changed by players.

### Color values

Colors are in BGR444 format. Channel ranges are from 0 to 15.
Config options are expecting color as a single integer from 0 to 4095.

Use this formula: r + g * 16 + b * 256

## Mod

Create new lump called `ACE_CONF` in your WAD file. There are supported options:

- `render.visplane.count`
  - change static limit of visplanes
  - default and minimum value is 128
- `render.vissprite.count`
  - change static limit of vissprites
  - default and minimum value is 128
- `render.drawseg.count`
  - change static limit of drawsegs
  - default and minimum value is 256
- `render.e3dplane.count`
  - change static limit of *3D floor* visplanes
  - default and minimum value is 16
- `decorate.enable` default: enabled
- `dehacked.enable` default: enabled
- `doomtex.workaround` default: disabled
  - this will enable texture lump workaround
  - for more info refer to [textures](textures.md)
- `display.wipe` override wipe style
  - this will take precendence over which style player has set in configuration
- `menu.font.height` override height of `SMALLFNT`
  - for more info refer to [fonts](fonts.md)
- `menu.color.save.empty` text color for empty save slot
- `menu.color.save.valid` text color for valid (loadable) save slot
- `menu.color.save.error` text color for invalid save slot
  - different ACE Engine version
- `menu.color.save.mismatch` text color for invalid save slot
  - different MOD version (DECORATE mismatch)
- `ram.min` minimum required RAM in megabytes; default: 1
- `game.mode` override game mode
  - `0` doom2
  - `1` doom1 with **3** episodes
- `game.save.name` save file name preffix
  - default `save*`
  - asterisk symbol will be replaced with slot number and **exactly one must** be present
  - 8 characters maximum
  - for more info refer to [save](save.md)
- `sound.channels.min` force minimum amount of sound MIX channels
  - overrides player configuration
- `zdoom.light.fullbright` enable fullbright colors in colored sectors; default: enabled
  - disable this option to replicate ZDoom behavior
- `stbar.ammo[0].type` override `BULL` ammo type in status bar
- `stbar.ammo[1].type` override `SHEL` ammo type in status bar
- `stbar.ammo[2].type` override `RCKT` ammo type in status bar
- `stbar.ammo[3].type` override `CELL` ammo type in status bar
- `damage[0].name` set custom damage type name
  - damages from `0` up to `6` can be defined
  - for more info refer to [decorate](decorate.md)
- `automap.lockdefs` enable custom *locked door* line colors; defualt: disabled
  - for more info refer to [lockdefs](lockdefs.md)
- `automap.color[0]` background color
- `automap.color[1]` grid color
- `automap.color[2]` one-sided line color
- `automap.color[3]` *floor change* color
- `automap.color[4]` *ceiling change* color
- `automap.color[5]` *gray line* (automap) color
- `automap.color[6]` *gray line* (cheating) color
- `automap.color[7]` player color
- `automap.color[8]` thing (cheating) color

## Engine

ACE Engine uses `ace.cfg` file to store configuration.
If present, this file is parsed **after** original code parsed `default.cfg`.

- `input.key.*`
  - various control keys
  - *i am not gonna list all of them*
  - use 1 for *no key*; **don't use 0**
- `input.mouse.enable`
- `input.mouse.sensitivity`
- `input.mouse.atk.pri` primary attack
  - `-1` no button
  - `0` left button
  - `1` right button
  - `2` middle button
- `input.mouse.atk.sec` secondary attack
- `input.mouse.use` same as *use key*
- `input.mouse.inv.use` use selected inventory item
- `input.mouse.strafe` strafe mode
- `input.joy.enable` joystick enable
- `input.joy.fire`
  - original values, i have no idea
- `input.joy.strafe`
- `input.joy.use`
- `input.joy.speed`
- `sound.channels` number of MIX channels
  - this is how many sounds can be playing at the same time
- `sound.device.sfx` sound device
  - original values; i know that `3` is sound blaster
- `sound.device.music` music device
  - original values
- `sound.volume.sfx` sound effect volume
- `sound.volume.music` music volume
- `sound.sb.port`
- `sound.sb.irq`
- `sound.sb.dma`
- `sound.m.port`
- `display.messages` this is what F8 key does
- `display.size`
  - i have removed all *window* modes for now, sorry
- `display.gamma` this is what F11 key does
- `display.fps` enable FPS counter
- `display.wipe` wipe style
- `display.xhair.type` crosshair graphics
- `display.xhair.color` crosshair color
- `player.color` player color (in game)
- `player.autorun`
- `player.mouselook`
  - `0` disabled
  - `1` enabled
  - `2` *fake mode*; mouse aim is enabled but crosshair moves instead of the screen
- `player.weapon.switch` switch weapon when getting new one
- `player.weapon.center` weapon sprite attack behavior
  - `0` don't change; keep attacking from last offset
  - `1` force to center; like ZDoom does
  - `2` keep *bouncing* even when attacking
- `player.quickitem` *quick use* item type
  - contains item type alias
  - can't be reasonably changed from text file
