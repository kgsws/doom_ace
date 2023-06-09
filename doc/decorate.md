# DECORATE

A subset of [ZDoom](https://zdoom.org/wiki/DECORATE) `DECORATE` is supported.

- actor replacement is supported
- inheritance is supported
- most Doom actors are built-in

## normal definition

Definition of new actor with `doomednum` 12345:  
> actor newImp 12345

## replacement

You can replace any built-in actor using classic `decorate` definition:  
> actor newImp replaces DoomImp

## inheritance

You can create new actor with inherited values:  
> actor newImp : DoomImp

- you can **not** call original states using `super::`

List of built-in special classes for [inheritance](https://zdoom.org/wiki/Using_inheritance):

- [PlayerPawn](https://zdoom.org/wiki/Classes:PlayerPawn)
- [PlayerChunk](https://zdoom.org/wiki/Classes:PlayerChunk)
- [SwitchableDecoration](https://zdoom.org/wiki/Classes:SwitchableDecoration)
- [RandomSpawner](https://zdoom.org/wiki/Classes:RandomSpawner)
  - **different behavior** with `nomonsters`
  - use `ISMONSTER` flag as workaround
- [Health](https://zdoom.org/wiki/Classes:Health)
- [Inventory](https://zdoom.org/wiki/Classes:Inventory)
- [CustomInventory](https://zdoom.org/wiki/Classes:CustomInventory)
- [DoomWeapon](https://zdoom.org/wiki/Classes:DoomWeapon)
  - use this for **new weapons**
- [Ammo](https://zdoom.org/wiki/Classes:Ammo)
  - use this for **new ammo**
  - you can mofify original status bar ammo counters, see [config](config.md)
- [DoomKey](https://zdoom.org/wiki/Classes:DoomKey)
- [BasicArmorPickup](https://zdoom.org/wiki/Classes:BasicArmorPickup)
- [BasicArmorBonus](https://zdoom.org/wiki/Classes:BasicArmorBonus)
- [PowerupGiver](https://zdoom.org/wiki/Classes:PowerupGiver)
- [HealthPickup](https://zdoom.org/wiki/Classes:HealthPickup)

Supported powerups for inheritance:

- [PowerInvulnerable](https://zdoom.org/wiki/Classes:PowerInvulnerable)
- [PowerInvisibility](https://zdoom.org/wiki/Classes:PowerInvisibility)
- [PowerIronFeet](https://zdoom.org/wiki/Classes:PowerIronFeet)
- [PowerLightAmp](https://zdoom.org/wiki/Classes:PowerLightAmp)
- [PowerBuddha](https://zdoom.org/wiki/Classes:PowerBuddha)
- [PowerDoubleFiringSpeed](https://zdoom.org/wiki/Classes:PowerDoubleFiringSpeed)
- [PowerFlight](https://zdoom.org/wiki/Classes:PowerFlight)

NOTE: Powerup inheritance is a bit shaky. It mostly works, but there are some edge cases. For example, any `take inventory` action behaves differently than ZDoom.

## flags

List of supported [flags](https://zdoom.org/wiki/Actor_flags):

- `floorclip`
  - ignored
- `noskin`
  - ignored
- `noblooddecals`
  - ignored
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
- `ismonster`
- `noteleport`
- `randomize`
- `telestomp`
- `notarget`
- `quicktoretaliate`
- `seekermissile`
- `noforwardfall`
- `dontthrust`
- `nodamagethrust`
- `dontgib`
- `invulnerable`
- `buddha`
- `nodamage`
- `reflective`
- `boss`
- `noradiusdmg`
- `extremedeath`
- `noextremedeath`
- `skyexplode`
- `ripper`
- `dontrip`
- `pushable`
- `cannotpush`
- `puffonactors`
- `spectral`
- `ghost`
- `thrughost`
- `dormant`
- `painless`
- `iceshatter`
- `dontfall`
- `noicedeath`
- `icecorpse`
- `spawnsoundsource`
- `donttranslate`
- `notautoaimed`
- `puffgetsowner`
- `hittarget`
- `hitmaster`
- `hittracer`
- `movewithsector`
- `fullvoldeath`
- `oldradiusdmg`
- `bloodlessimpact`
- `synchronized`
- `dontmorph`
- `dontsplash`
- `thruactors`
- `bounceonfloors`
- `bounceonceilings`
- `noblockmonst`
- `dontcorpse`
- `stealth`
- `explodeonwater`
- `canbouncewater`
- `noverticalmeleerange`
- `forceradiusdmg`
- `nopain`
- `noliftdrop`
- `cantseek`

### inventory flags

- `inventory.quiet`
- `inventory.ignoreskill`
- `inventory.autoactivate`
- `inventory.autoactivate`
- `inventory.alwayspickup`
- `inventory.invbar`
- `inventory.hubpower`
- `inventory.bigpowerup`
- `inventory.neverrespawn`
- `inventory.additivetime`
- `inventory.noscreenflash`
- `inventory.noscreenblink`
- `inventory.untossable`

### weapon flags

- `weapon.noautofire`
- `weapon.noalert`
- `weapon.ammo_optional`
- `weapon.alt_ammo_optional`
- `weapon.ammo_checkboth`
- `weapon.primary_uses_both`
- `weapon.alt_uses_both`
- `weapon.noautoaim`
- `weapon.dontbob`
- `weapon.wimpy_weapon`
  - ignored
- `weapon.meleeweapon`
  - ignored

## properties

List of supported [properties](https://zdoom.org/wiki/Actor_properties):

- `spawnid`
- `health`
- `reactiontime`
- `painchance`
- `damage`
  - decorate **expression** can be used for exact damage or custom calculation
- `damagetype`
  - custom damage types can be defined in [config](config.md)
- `damagefactor`
- `speed`
- `radius`
- `height`
- `mass`
- `gravity`
- `cameraheight`
- `fastspeed`
- `vspeed`
- `bouncecount`
- `bouncefactor`
- `species`
- `telefogsourcetype`
- `telefogdesttype`
- `bloodtype`
- `bloodcolor`
  - maximum of 255 unique blood colors
- `dropitem`
  - `amount` for item drops is **ignored**
  - `amount` is only used for [RandomSpawner](https://zdoom.org/wiki/Classes:RandomSpawner)
- `activesound`
- `attacksound`
- `deathsound`
- `painsound`
- `seesound`
- `bouncesound`
- `maxstepheight`
- `maxdropoffheight`
- `meleerange`
- `scale`
- `args`
- `renderstyle`
- `alpha`
- `stealthalpha`
- `translation`
- `monster`
- `projectile`
- `states`
  - *obviously*
- `obituary`
  - ignored
- `hitobituary`
  - ignored
- `decal`
  - ignored

### player properties

- `player.viewheight`
- `player.attackzoffset`
- `player.jumpz`
- `player.viewbob`
- `player.displayname`
- `player.runhealth`
- `player.soundclass`
- `player.weaponslot`
- `player.startitem`
- `player.colorrange`
- `player.crouchsprite`
  - ignored

### inventory properties

- `inventory.amount`
- `inventory.maxamount`
- `inventory.interhubamount`
- `inventory.icon`
- `inventory.pickupflash`
  - ignored
- `inventory.respawntics`
- `usesound`
- `pickupsound`
- `pickupmessage`
- `althudicon`
  - ignored

### weapon properties

- `weapon.selectionorder`
- `weapon.kickback`
- `weapon.ammouse`
- `weapon.ammouse1`
- `weapon.ammouse2`
- `weapon.ammogive`
- `weapon.ammogive1`
- `weapon.ammogive2`
- `weapon.ammotype`
- `weapon.ammotype1`
- `weapon.ammotype2`
- `weapon.readysound`
- `weapon.upsound`

### ammo properties

- `ammo.backpackamount`
- `ammo.backpackmaxamount`

### armor properties

- `armor.saveamount`
- `armor.maxsaveamount`
- `armor.savepercent`

### powerup properties

- `powerup.duration`
- `powerup.type`
- `powerup.mode`
  - `reflective` or `translucent` or `cumulative`
  - otherwise default value is used
- `powerup.color`
- `powerup.strength`


## expressions

Limited support for [DECORATE expressions](https://zdoom.org/wiki/DECORATE_expressions) is implemented.

- mathematical expression without brackets is supported
  - logical AND / OR
  - arithmetical AND / OR / XOR
  - equality check `==`
  - inequality check `!=`
  - compare `<` `>` `<=` `>=`
  - addition
  - subtraction
  - multiplication
  - division
  - modulo
  - arithmetical negation
  - NOTE: internal numeric precision is signed fixed point 22.10
- only `random` function

List of supported variables:

- `x` `y` and `z`
- `angle`
- `ceilingz`
- `floorz`
- `pitch`
- `velx`
- `vely`
- `velz`
- `alpha`
- `args[0]` to `args[4]`
- `health`
- `height`
- `radius`
- `reactiontime`
- `scalex`
- `scaley`
- `special`
- `tid`
- `threshold`
- `waterlevel`
- `mass`
- `speed`

### constants

Various action functions use constants in string form.  
**BEWARE:** Defined constants have different values than in ZDoom.  
**Never** use these outside intended function arguments!

## functions

List of supported [action functions](https://zdoom.org/wiki/Action_functions):

- `A_Lower`
- `A_Raise`
- `A_GunFlash`
- `A_CheckReload`
- `A_Light0`
- `A_Light1`
- `A_Light2`
- `A_WeaponReady`
- `A_ReFire`
- `A_Pain`
- `A_Scream`
- `A_XScream`
- `A_PlayerScream`
- `A_ActiveSound`
- `A_StartSound`
  - `slot` only values `CHAN_BODY` `CHAN_WEAPON` and `CHAN_VOICE` are supported
  - `volume` must be se to 1
  - `attenuation` only values `ATTN_NORM` and `ATTN_NONE` are supported
  - `pitch` and `startTime` is **not** supported
- `A_FaceTarget`
  - **no** arguments are supported
- `A_FaceTracer`
  - **no** arguments are supported
- `A_FaceMaster`
  - **no** arguments are supported
- `A_NoBlocking`
  - **no** arguments are supported
- `A_Look`
- `A_Chase`
  - **no** arguments are supported
- `A_VileChase`
- `A_SpawnProjectile`
- `A_CustomBulletAttack`
  - only 7 arguments are supported; last is `flags`
- `A_CustomMeleeAttack`
  - `bleed` is ignored
- `A_VileTarget`
- `A_VileAttack`
- `A_FireProjectile`
- `A_FireBullets`
  - only 7 arguments are supported; last is `range`
- `A_CustomPunch`
  - only 5 arguments are supported; last is `range`
  - `CPF_DAGGER` and `CPF_STEALARMOR` is not supported
- `A_BFGSpray`
  - `vrange` is ignored and should be set to `32`
  - `flags` is not supported
- `A_SeekerMissile`
  - `SMF_CURSPEED` is not supported
- `A_SpawnItemEx`
- `A_DropItem`
  - `dropamount` is ignored and should be set to `0`
- `A_Burst`
- `A_SkullPop`
- `A_FreezeDeath`
- `A_GenericFreezeDeath`
- `A_FreezeDeathChunks`
- `A_SetTranslation`
- `A_SetScale`
  - only one argument is supported
- `A_SetRenderStyle`
- `A_FadeOut`
  - only one argument is supported
- `A_FadeIn`
  - only one argument is supported
- `A_CheckPlayerDone`
- `A_AlertMonsters`
  - no arguments are supported
- `A_SetAngle`
  - `flags` are ignored and should be set to `0`
- `A_SetPitch`
  - `flags` are ignored and should be set to `0`
- `A_ChangeFlag`
- `A_ChangeVelocity`
- `A_ScaleVelocity`
- `A_Stop`
- `A_SetTics`
- `A_RearrangePointers`
  - `flags` **must** be set to `PTROP_NOSAFEGUARDS`
- `A_BrainAwake`
- `A_BrainSpit`
  - no arguments are supported
- `A_SpawnFly`
  - no arguments are supported
- `A_BrainDie`
- `A_RaiseSelf`
  - no arguments are supported
- `A_KeenDie`
- `A_Warp`
  - only 6 arguments are supported; last is `flags`
  - supported flags
  - `WARPF_MOVEPTR`
  - `WARPF_USECALLERANGLE`
  - `WARPF_ABSOLUTEANGLE`
  - `WARPF_ABSOLUTEPOSITION`
  - `WARPF_ABSOLUTEOFFSET`
  - `WARPF_COPYVELOCITY`
  - `WARPF_COPYPITCH`
  - `WARPF_STOP`
- `A_SetArg`
- `A_DamageTarget`
  - only 2 arguments are supported; last is `damagetype`
- `A_DamageTracer`
  - only 2 arguments are supported; last is `damagetype`
- `A_DamageMaster`
  - only 2 arguments are supported; last is `damagetype`
- `A_Explode`
  - only 5 arguments are supported; last is `fulldamagedistance`
  - `XF_EXPLICITDAMAGETYPE` is not supported
- `A_SetHealth`
- `A_GiveInventory`
- `A_TakeInventory`
  - `flags` are ignored and should be set to `0`
- `A_SelectWeapon`
  - only `whichweapon` is supported
- `A_Print`
- `A_PrintBold`
- `A_Jump`
  - only up to jump 9 destinations
- `A_JumpIf`
- `A_JumpIfInventory`
- `A_JumpIfHealthLower`
- `A_JumpIfCloser`
- `A_JumpIfTracerCloser`
- `A_JumpIfMasterCloser`
- `A_JumpIfTargetInsideMeleeRange`
- `A_JumpIfTargetOutsideMeleeRange`
- `A_CheckFloor`
- `A_CheckFlag`
- `A_MonsterRefire`

## line specials

Most [line specials](lnspec.md) **can** be used as state functions.

**BEWARE:** ACE Engine does **not** check required number of arguments. **Always** test your DECORTE in GZDoom first!

Special case for `Line_SetTextureOffset` line special:  
Offsets have to be specified as `doom fixed point`, that is, offset `16` has to be set as `16 * 65536`.  
No change has to be specified as `32767 * 65536`.
**BEWARE:** Due to numeric precision limit, max positive offset is `31` and max negative offset is `-32`!
