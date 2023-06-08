# SNDINFO

A subset of [ZDoom](https://zdoom.org/wiki/SNDINFO) `SNDINFO` is supported.

## What does work

- normal sound definitions
- `$random`
  - with limitations of 4 sounds to choose from
- `$playersound`
  - gender **must** be set to `other`
  - `*death`
  - `*xdeath`
  - `*gibbed`
  - `*pain100`
  - `*pain75`
  - `*pain50`
  - `*pain25`
  - `*land`
  - `*usefail`
  - `*grunt` is not supported and should be set to `dsempty` for ZDoom compatibility

## Limitations

**There are no standard ZDoom built-in sounds!** You have to define every sound from the scratch!

You can redefine already defined sound, but only if it does not link to original lump.

> stuff/test0 dscustom  
> stuff/test0 dsmeh

This is a valid definition as `dscustom` is not doom built-in SFX name.

> stuff/test0 dsdoropn  
> stuff/test0 dsmeh

This is **not** valid definition as `doropn` **is** doom built-in SFX name.

## Built-in sounds

- `world/quake dsquake`
- `misc/freeze icedth1`
- `misc/icebreak icebrk1a`
- `misc/secret dssecret`
