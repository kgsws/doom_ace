# SNDSEQ

A subset of [ZDoom](https://zdoom.org/wiki/SNDSEQ) `SNDSEQ` is supported.

## What does work

- `door` and `platform` definitions
  - index `zero` can **not** be used
- combined `door` definition
  - different sounds for each direction
- `playuntildone`
- `playtime`
- `playloop`
- `playrepeat`
- `stopsound`
- `nostopcutoff`

## Timing

Since i did not touch sound library, it is not possible to detect when sound sample ended. Thus, `playuntildone` and `playrepeat` attempts to calculate sound duration.  
This calculation will **not** work for **random** sounds!