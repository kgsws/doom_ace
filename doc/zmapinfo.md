# ZMAPINFO

A subset of [ZDoom](https://zdoom.org/wiki/MAPINFO) `ZMAPINFO` is supported.

## What does work

### defaultmap

Set default values for all maps that follow. Uses same format as *map* definition.

### map

[ZDoom link](https://zdoom.org/wiki/MAPINFO/Map_definition)

- `levelnum`
  - **not** generated automaticaly from lump name
- `cluster`
- `par`
- `titlepatch`
  - **no** `hideauthorname`
- `music`
  - **no** `tracknum`
- `intermusic`
- `sky1`
  - **no** `scrollspeed`
- `sky2`
  - **no** `scrollspeed`
- `next`
  - `endgamec`
  - `endgame3`
  - `endtitle`
- `secretnext`
  - `endgamec`
  - `endgame3`
  - `endtitle`
- `nointermission`
- `fallingdamage`
- `monsterfallingdamage`
- `propermonsterfallingdamage`
- `nofreelook`
- `allowrespawn`
- `noinfighting`
- `totalinfighting`
- `resetinventory`
- `spawnwithweaponraised`
- `strictmonsteractivation`

### cluster

[ZDoom link](https://zdoom.org/wiki/MAPINFO/Cluster_definition)

- `music`
- `flat`
- `entertext`
- `exittext`
- `hub`

### episode

[ZDoom link](https://zdoom.org/wiki/MAPINFO/Episode_definition)

- `picname`
- `noskillmenu`
