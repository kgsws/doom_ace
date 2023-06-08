# Textures

ACE Engine will now read textures between `TX_START` and `TX_END` lumps. There can by any amount of these sections.

## Resolution

Unlike in `doom2.exe`, textures can have any power-of-two **height**. Original game limited texture height to exactly 128px.  
Further, *medusa-bug* was fixed. You can use multipatch textures on middle walls.

Original power-of-two **width** limitation **is still present**.

## Exploit workaround

If you don't care about compatibility with other source ports, and don't intend to use original texture definition system, you can just enable `doomtex.workaround` [config](config.md) option.  
This option will invalidate all conflicting lumps that exploit uses, thus original `doom2.wad` lumps will be found instead.

Alternatively, you can rename `PNAMES`, `TEXTURE1` and `TEXTURE2` using `DEHACKED`. This should allow you to make mod compatible with other source ports.  
Except, this does not work in GZDoom!

## Transparent flats

Sometimes, for 3D floors, it is good to have transparent texture, like metal grid. Doom flat system does not support transparency for flats. Thus a new workaround was made.

Any textures with resolution of 64x64 can be used as flats. Though, this doubles the memory requirements as texture for wall and flat has to be stored separately.

Also note that textures on flats can't be animated using [animdefs](animdefs.md).