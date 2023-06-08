# Sprites

Sprites are read from sections between `S_START` / `S_END` or `SS_START` / `SS_END`. There can by any amount of these sections.

Yes, unlike original `doom2.exe`, sprite definitions do not have to be consecutive in WAD file(s).  
Further, duplicate definitions are skipped instead of throwing an error.

# Note

Tall patches are not supported.