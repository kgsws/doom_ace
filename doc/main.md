# ACE Engine Documentation

## Preparation

ACE Engine uses these specialy crafted lumps to achieve code execution:

- `PNAMES` single link to `ACE_LDR0` fake patch
- `TEXTURE1` specialy crafted list of fake textures and multiple negative size allocations
- `ACE_LDR0` first stage of loader pretending to be doom graphics
- `ACE_LDR1` seconds stage of loader
- `ACE_CODE` the engine itself

These lumps must not be overidden by any duplicates, otherwise `doom2.exe` will read incorrect content instead.
Additionaly, `TEXTURE2` lump **must not** exist!

Refer to [textures](textures.md) for workaround.

## TOC

- [config](config.md)
- [fonts](fonts.md)
- [save](save.md)
- [textures](textures.md)
- [flats](flats.md)
- [sprites](sprites.md)
- [keyconf](keyconf.md)
- [sndinfo](sndinfo.md)
- [sndseq](sndseq.md)
- [trnslate](trnslate.md)
- [zmapinfo](zmapinfo.md)
- [animdefs](animdefs.md)
- [decorate](decorate.md)
- [line specials](lnspec.md)

## Expected lumps

ACE Engine adds new features that require custom graphics not present in `doom2.wad`.

- `A_LDING` custom loading screen with empty progress bar
  - if not present, `INTERPIC` is used instead
- `A_LDBAR` full progress bar of loading screen
  - width of this patch is used as 100% loading progress
  - use X and Y offset to position this patch to overlap progress bar in `A_LDING`
  - if not present, `TITLEPIC` is used instead
- `M_PCLASS` menu title for player class selection
  - if not present, `M_NGAME` is used instead
- `M_DISPL` menu title for display options
  - if not present, `M_OPTION` is used instead
- `M_CNTRL` menu title for customize controls
  - if not present, `M_OPTION` is used instead
- `M_MOUSE` menu title for mouse setup
  - if not present, `M_OPTION` is used instead
- `M_PLAYER` menu title for player setup
  - if not present, `M_OPTION` is used instead
- `WILOADIN` rendered on screen when loading level
  - optional
- `WIAUTOSV` rendered on screen when autosaving
  - optional
- `ARTIBOX` inventory background
  - if not present, `STFB1` is used instead
- `SELECTBO` inventory selection highlight
  - if not present, `STFB0` is used instead
- `XHAIR` custom, mod specific, crosshair
  - optional
  - ACE Engine contains a few built-in crosshairs
  - this will add one extra crosshair for players to select from
  - entire non-transparent area will be replaced with a single color
- `ACE_RNDR` rendering tables, like transparency
  - optional but recommended
  - ACE Engine can calculate these tables, but calculation takes ages on 486 CPU
  - use command line option `-dumptables` to generate these tables to file which you can then import to your WAD
  - **BEWARE**: these tables depend on `PLAYPAL` everytime you change palette you must re-generate this lump!
- `ACE_RNG` list of random numbers
  - optional, can enhance original randomization
  - contains list of 8bit values
  - maximum of 8192 bytes

For expected built-in sounds lumps refer to [sndinfo](sndinfo.md).