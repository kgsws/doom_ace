# Save game

Save game files are now pretending to be bitmap files. After bitmap data a new block of ACE Engine stuff is added.

## Naming

Sice all saves are stored in `ace_save` direcotry, it is highly recommended to customize save preffix, see `game.save.name` in [config](config.md).

## Hubs

When you save in level that is part of hub, all hub level data is stored in single save game (bitmap) file.  
Unsaved hub files are not bitmaps and can be safely ignored.

## Autosave

Autosave is feature only available from ZDoom line specials (Hexen map format). This save does not occupy any slot and therefore is only temporary.

Since saving a file on old hardware takes a time, a special lump can be used to display save message. `WIAUTOSV`