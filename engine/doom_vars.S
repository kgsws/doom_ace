// kgsws' ACE Engine
////
// This allows relocated acces to Doom game variables.
// Relocations to this section have to be calculated differently!

// Basicaly "register" everything as 8bit number, it does not really matter.
.macro	DOOM_VAR addr, name
.org \addr
.global \name
\name :
.byte
.endm

.section .doom.data

DOOM_VAR 0x00001A84, finetangent
DOOM_VAR 0x00005A84, finesine
DOOM_VAR 0x00007A84, finecosine
DOOM_VAR 0x0000FA84, tantoangle

DOOM_VAR 0x00011B40, wipegamestate

DOOM_VAR 0x00011B80, pars

DOOM_VAR 0x00011C20, cpars

DOOM_VAR 0x00012222, dtxt_skull_name
DOOM_VAR 0x00012247, dtxt_m_option

DOOM_VAR 0x000122B2, dtxt_m_epi1
DOOM_VAR 0x000122C3, dtxt_m_epi2
DOOM_VAR 0x000122D4, dtxt_m_epi3

DOOM_VAR 0x000122E4, EpiDef
DOOM_VAR 0x00012350, NewDef
DOOM_VAR 0x000124A8, LoadMenu
DOOM_VAR 0x00012510, LoadDef

DOOM_VAR 0x0001261C, rndtable
DOOM_VAR 0x00012720, prndindex

DOOM_VAR 0x00012C50, am_cheating
DOOM_VAR 0x00012C5C, automapactive
DOOM_VAR 0x00012CA8, am_lastlevel

DOOM_VAR 0x00012D70, maxammo
DOOM_VAR 0x00012D80, clipammo

DOOM_VAR 0x00012D90, deh_weaponinfo

DOOM_VAR 0x0001357C, fuzzpos
DOOM_VAR 0x00013580, validcount

DOOM_VAR 0x00013C04, mapnames
DOOM_VAR 0x00013CBC, mapnames2

DOOM_VAR 0x00014448, music_now
DOOM_VAR 0x0001444C, S_music

DOOM_VAR 0x0001488C, S_sfx

DOOM_VAR 0x00015800, spr_names

DOOM_VAR 0x00015A28, deh_states

DOOM_VAR 0x0001C3EC, deh_mobjinfo

DOOM_VAR 0x000204C0, dtxt_E1TEXT
DOOM_VAR 0x0002067C, dtxt_E2TEXT
DOOM_VAR 0x00020850, dtxt_E3TEXT
DOOM_VAR 0x00020A40, dtxt_C1TEXT
DOOM_VAR 0x00020BD8, dtxt_C2TEXT
DOOM_VAR 0x00020E44, dtxt_C3TEXT
DOOM_VAR 0x00020F80, dtxt_C4TEXT
DOOM_VAR 0x00021170, dtxt_C5TEXT
DOOM_VAR 0x00021218, dtxt_C6TEXT

DOOM_VAR 0x00021278, dtxt_slime16
DOOM_VAR 0x00021280, dtxt_rrock14
DOOM_VAR 0x00021288, dtxt_rrock07
DOOM_VAR 0x00021290, dtxt_rrock17
DOOM_VAR 0x00021298, dtxt_rrock13
DOOM_VAR 0x000212A0, dtxt_rrock19
DOOM_VAR 0x000212A8, dtxt_floor4_8
DOOM_VAR 0x000212B4, dtxt_sflr6_1
DOOM_VAR 0x000212BC, dtxt_mflr8_4

DOOM_VAR 0x000213D0, dtxt_help2
DOOM_VAR 0x000213D8, dtxt_victory2

DOOM_VAR 0x00022458, dtxt_m_loadg
DOOM_VAR 0x00022460, dtxt_m_lsleft
DOOM_VAR 0x0002246C, dtxt_m_lscntr
DOOM_VAR 0x00022478, dtxt_m_lsrght
DOOM_VAR 0x000224BC, dtxt_m_saveg

DOOM_VAR 0x0002265C, dtxt_m_episod

DOOM_VAR 0x00022C08, dtxt_PD_BLUEO
DOOM_VAR 0x00022C34, dtxt_PD_REDO
DOOM_VAR 0x00022C60, dtxt_PD_YELLOWO
DOOM_VAR 0x00022C90, dtxt_PD_BLUEK
DOOM_VAR 0x00022CB8, dtxt_PD_YELLOWK
DOOM_VAR 0x00022CE0, dtxt_PD_REDK

DOOM_VAR 0x00023484, dtxt_colormap

DOOM_VAR 0x00023E30, dtxt_STSTR_DQDON
DOOM_VAR 0x00023E48, dtxt_STSTR_DQDOFF
DOOM_VAR 0x00023E60, dtxt_STSTR_FAADDED
DOOM_VAR 0x00023E78, dtxt_STSTR_KFAADDED
DOOM_VAR 0x00023EB8, dtxt_STSTR_NCON
DOOM_VAR 0x00023ECC, dtxt_STSTR_NCOFF

DOOM_VAR 0x00024750, dtxt_interpic

DOOM_VAR 0x000247BC, dtxt_wif
DOOM_VAR 0x000247C0, dtxt_wienter

DOOM_VAR 0x00024908, dtxt_mus_pfx

DOOM_VAR 0x00024A18, dtxt_inter
DOOM_VAR 0x00024A30, dtxt_victor
DOOM_VAR 0x00024B40, dtxt_read_m
DOOM_VAR 0x00024B50, dtxt_dm2int

DOOM_VAR 0x000290F8, r_rdptr
DOOM_VAR 0x0002914C, r_fbptr

DOOM_VAR 0x000291E0, snd_sbdma
DOOM_VAR 0x000291E4, snd_mport
DOOM_VAR 0x000291EC, snd_sbirq
DOOM_VAR 0x00029200, snd_sfxdevice
DOOM_VAR 0x00029204, snd_musicdevice
DOOM_VAR 0x00029208, snd_sbport

DOOM_VAR 0x0002929C, finaleflat
DOOM_VAR 0x000292A0, finaletext
DOOM_VAR 0x000292A4, finalecount
DOOM_VAR 0x000292A8, finalestage

DOOM_VAR 0x00029730, wadfiles

DOOM_VAR 0x0002A384, demosequence
DOOM_VAR 0x0002A38C, advancedemo

DOOM_VAR 0x0002A398, startskill
DOOM_VAR 0x0002A39C, startmap
DOOM_VAR 0x0002A3A4, startepisode

DOOM_VAR 0x0002A3AC, gamemode_sw
DOOM_VAR 0x0002A3B4, gamemode_reg
DOOM_VAR 0x0002A3BC, gamemode
DOOM_VAR 0x0002A3C0, nomonsters
DOOM_VAR 0x0002A3C4, fastparm
DOOM_VAR 0x0002A3C8, respawnparm

DOOM_VAR 0x0002A780, savename

DOOM_VAR 0x0002A880, gamekeydown
DOOM_VAR 0x0002AC84, mousebuttons

DOOM_VAR 0x0002AC90, savedesc

DOOM_VAR 0x0002ADB0, wminfo

DOOM_VAR 0x0002AE78, players

DOOM_VAR 0x0002B2D8, playeringame

DOOM_VAR 0x0002B2E8, defdemoname
DOOM_VAR 0x0002B2EC, d_skill

DOOM_VAR 0x0002B2F8, secretexit

DOOM_VAR 0x0002B300, saveslot

DOOM_VAR 0x0002B33C, mousey

DOOM_VAR 0x0002B344, key_up
DOOM_VAR 0x0002B348, joyb_speed
DOOM_VAR 0x0002B34C, key_speed
DOOM_VAR 0x0002B350, joyb_use
DOOM_VAR 0x0002B354, key_use
DOOM_VAR 0x0002B358, mouseb_forward
DOOM_VAR 0x0002B35C, key_down
DOOM_VAR 0x0002B360, key_strafeleft
DOOM_VAR 0x0002B364, key_left
DOOM_VAR 0x0002B368, joyb_strafe
DOOM_VAR 0x0002B36C, mouseb_strafe
DOOM_VAR 0x0002B370, key_strafe
DOOM_VAR 0x0002B37C, joyb_fire
DOOM_VAR 0x0002B380, mouseb_fire
DOOM_VAR 0x0002B384, key_fire

DOOM_VAR 0x0002B388, netdemo

DOOM_VAR 0x0002B390, key_straferight
DOOM_VAR 0x0002B394, key_right

DOOM_VAR 0x0002B3B4, singledemo
DOOM_VAR 0x0002B3B8, viewactive
DOOM_VAR 0x0002B3BC, gametic
DOOM_VAR 0x0002B3C8, totalsecret
DOOM_VAR 0x0002B3D0, totalitems
DOOM_VAR 0x0002B3D4, totalkills
DOOM_VAR 0x0002B3D8, displayplayer
DOOM_VAR 0x0002B3DC, consoleplayer
DOOM_VAR 0x0002B3E0, gameskill
DOOM_VAR 0x0002B3E4, demoplayback
DOOM_VAR 0x0002B3E8, gamemap
DOOM_VAR 0x0002B3EC, respawnmonsters
DOOM_VAR 0x0002B3F0, demorecording
DOOM_VAR 0x0002B3F4, gamestate
DOOM_VAR 0x0002B3F8, gameepisode
DOOM_VAR 0x0002B3FC, deathmatch

DOOM_VAR 0x0002B400, netgame
DOOM_VAR 0x0002B404, gameaction
DOOM_VAR 0x0002B408, paused
DOOM_VAR 0x0002B40C, usergame

DOOM_VAR 0x0002B67C, currentMenu
DOOM_VAR 0x0002B698, screenblocks
DOOM_VAR 0x0002B6AC, detaillevel
DOOM_VAR 0x0002B6B0, showMessages
DOOM_VAR 0x0002B6B4, snd_MusicVolume
DOOM_VAR 0x0002B6B8, snd_SfxVolume
DOOM_VAR 0x0002B6C8, mouseSensitivity
DOOM_VAR 0x0002B6CC, menuactive
DOOM_VAR 0x0002B6D4, menu_item_now

DOOM_VAR 0x0002B6E8, usemouse
DOOM_VAR 0x0002B6EC, usejoystick

DOOM_VAR 0x0002B840, activeceilings

DOOM_VAR 0x0002B960, spechit
DOOM_VAR 0x0002B990, nofit
DOOM_VAR 0x0002B994, crushchange
DOOM_VAR 0x0002B9A8, shootz
DOOM_VAR 0x0002B9AC, shootthing
DOOM_VAR 0x0002B9B0, la_damage
DOOM_VAR 0x0002B9B4, aimslope
DOOM_VAR 0x0002B9B8, attackrange
DOOM_VAR 0x0002B9C8, slidemo
DOOM_VAR 0x0002B9D0, bestslidefrac
DOOM_VAR 0x0002B9D4, bestslideline
DOOM_VAR 0x0002B9D8, numspechit
DOOM_VAR 0x0002B9DC, tmthing
DOOM_VAR 0x0002B9E0, tmflags
DOOM_VAR 0x0002B9E4, tmdropoffz
DOOM_VAR 0x0002B9F0, tmfloorz
DOOM_VAR 0x0002B9F8, linetarget
DOOM_VAR 0x0002BA00, tmceilingz
DOOM_VAR 0x0002BA10, trace
DOOM_VAR 0x0002BA20, intercepts

DOOM_VAR 0x0002C028, intercept_p
DOOM_VAR 0x0002C02C, lowfloor
DOOM_VAR 0x0002C030, openbottom
DOOM_VAR 0x0002C034, opentop
DOOM_VAR 0x0002C038, openrange

DOOM_VAR 0x0002C040, activeplats

DOOM_VAR 0x0002C0C0, bulletslope

DOOM_VAR 0x0002C0D0, playerstarts

DOOM_VAR 0x0002C104, bmaporgx
DOOM_VAR 0x0002C108, bmaporgy

DOOM_VAR 0x0002C118, sides
DOOM_VAR 0x0002C11C, numsides
DOOM_VAR 0x0002C120, lines
DOOM_VAR 0x0002C12C, segs
DOOM_VAR 0x0002C134, numlines
DOOM_VAR 0x0002C138, vertexes
DOOM_VAR 0x0002C140, subsectors
DOOM_VAR 0x0002C148, sectors
DOOM_VAR 0x0002C14C, numsectors

DOOM_VAR 0x0002C150, deathmatch_p
DOOM_VAR 0x0002C154, deathmatchstarts

DOOM_VAR 0x0002CF74, thinkercap
DOOM_VAR 0x0002CF80, leveltime
DOOM_VAR 0x0002CF90, onground
DOOM_VAR 0x0002CFA0, solidsegs

DOOM_VAR 0x0002D0A0, d_drawsegs

DOOM_VAR 0x000300A4, curline
DOOM_VAR 0x000300A8, ds_p
DOOM_VAR 0x000300AC, backsector
DOOM_VAR 0x000300B0, frontsector

DOOM_VAR 0x000300B4, linedef
DOOM_VAR 0x000300B8, sidedef

DOOM_VAR 0x000300D0, texturecolumnofs
DOOM_VAR 0x000300D4, texturecompositesize
DOOM_VAR 0x000300D8, texturewidthmask
DOOM_VAR 0x000300DC, numflats
DOOM_VAR 0x000300E0, textures
DOOM_VAR 0x000300E4, numtextures
DOOM_VAR 0x000300E8, texturecomposite
DOOM_VAR 0x000300F4, texturecolumnlump

DOOM_VAR 0x00030100, spritewidth
DOOM_VAR 0x00030104, colormaps
DOOM_VAR 0x0003010C, texturetranslation
DOOM_VAR 0x00030110, flattranslation
DOOM_VAR 0x00030114, spritetopoffset
DOOM_VAR 0x00030118, spriteoffset

DOOM_VAR 0x00030124, textureheight

DOOM_VAR 0x00030130, columnofs
DOOM_VAR 0x000312B0, ylookup

DOOM_VAR 0x000322B8, dc_translation
DOOM_VAR 0x000322C4, ds_ystep
DOOM_VAR 0x000322C8, ds_xstep
DOOM_VAR 0x000322CC, ds_x1
DOOM_VAR 0x000322D0, ds_x2
DOOM_VAR 0x000322D4, ds_colormap
DOOM_VAR 0x000322D8, dc_colormap
DOOM_VAR 0x000322DC, ds_yfrac
DOOM_VAR 0x000322E0, ds_xfrac
DOOM_VAR 0x000322E4, ds_source
DOOM_VAR 0x000322E8, dc_source
DOOM_VAR 0x000322EC, dc_x
DOOM_VAR 0x000322F0, ds_y
DOOM_VAR 0x000322F4, dc_yh
DOOM_VAR 0x000322F8, dc_yl
DOOM_VAR 0x000322FC, dc_texturemid
DOOM_VAR 0x00032300, dc_iscale
DOOM_VAR 0x00032304, viewheight
DOOM_VAR 0x0003230C, viewwidth
DOOM_VAR 0x00032310, viewwindowx
DOOM_VAR 0x00032314, viewwindowy

DOOM_VAR 0x000323E0, zlight

DOOM_VAR 0x000343E0, scalelight

DOOM_VAR 0x00038FE0, r_setblocks

DOOM_VAR 0x00038FFC, fixedcolormap

DOOM_VAR 0x00039004, spanfunc
DOOM_VAR 0x00039010, colfunc
DOOM_VAR 0x00039014, extralight
DOOM_VAR 0x0003901C, viewplayer
DOOM_VAR 0x00039020, xtoviewangle

DOOM_VAR 0x00039528, viewangle
DOOM_VAR 0x0003952C, centeryfrac
DOOM_VAR 0x00039534, centerx
DOOM_VAR 0x00039538, centery
DOOM_VAR 0x00039540, viewx
DOOM_VAR 0x00039544, viewy
DOOM_VAR 0x00039548, viewz

DOOM_VAR 0x0003A1E0, d_visplanes

DOOM_VAR 0x0004F420, ceilingclip
DOOM_VAR 0x0004F6A0, floorclip

DOOM_VAR 0x0004F920, yslope

DOOM_VAR 0x0005A140, lastvisplane
DOOM_VAR 0x0005A144, skytexturemid

DOOM_VAR 0x0005A150, planeheight
DOOM_VAR 0x0005A154, planezlight
DOOM_VAR 0x0005A164, skyflatnum
DOOM_VAR 0x0005A168, ceilingplane
DOOM_VAR 0x0005A16C, floorplane
DOOM_VAR 0x0005A170, skytexture
DOOM_VAR 0x0005A180, bottomstep
DOOM_VAR 0x0005A184, topstep
DOOM_VAR 0x0005A188, pixlowstep
DOOM_VAR 0x0005A18C, pixhighstep
DOOM_VAR 0x0005A190, walllights
DOOM_VAR 0x0005A194, pixlow
DOOM_VAR 0x0005A198, bottomfrac
DOOM_VAR 0x0005A19C, topfrac
DOOM_VAR 0x0005A1A0, maskedtexturecol
DOOM_VAR 0x0005A1A4, rw_scalestep
DOOM_VAR 0x0005A1AC, rw_centerangle
DOOM_VAR 0x0005A1B0, midtexture
DOOM_VAR 0x0005A1B4, bottomtexture
DOOM_VAR 0x0005A1B8, toptexture
DOOM_VAR 0x0005A1BC, maskedtexture
DOOM_VAR 0x0005A1C0, pixhigh
DOOM_VAR 0x0005A1CC, rw_bottomtexturemid
DOOM_VAR 0x0005A1D0, rw_toptexturemid
DOOM_VAR 0x0005A1D4, rw_midtexturemid
DOOM_VAR 0x0005A1DC, rw_offset
DOOM_VAR 0x0005A1E0, rw_scale
DOOM_VAR 0x0005A1E4, rw_angle1
DOOM_VAR 0x0005A1E8, rw_x
DOOM_VAR 0x0005A1EC, markceiling
DOOM_VAR 0x0005A1F0, segtextured
DOOM_VAR 0x0005A1F4, markfloor
DOOM_VAR 0x0005A1F8, rw_stopx
DOOM_VAR 0x0005A1FC, rw_normalangle

DOOM_VAR 0x0005A200, rw_distance

DOOM_VAR 0x0005A210, d_vissprites

DOOM_VAR 0x0005C554, sprtemp

DOOM_VAR 0x0005C884, spr_maxframe

DOOM_VAR 0x0005C888, sprtopscreen
DOOM_VAR 0x0005C88C, mceilingclip
DOOM_VAR 0x0005C890, mfloorclip
DOOM_VAR 0x0005C894, vissprite_p
DOOM_VAR 0x0005C898, pspriteiscale
DOOM_VAR 0x0005C8A0, spryscale
DOOM_VAR 0x0005C8A4, vsprsortedhead

DOOM_VAR 0x0005C8E0, numsprites
DOOM_VAR 0x0005C8E4, sprites

DOOM_VAR 0x00074F94, lumpcache
DOOM_VAR 0x00074FA0, numlumps
DOOM_VAR 0x00074FA4, lumpinfo

DOOM_VAR 0x00074FC0, usegamma

DOOM_VAR 0x00074FF0, w_maxammo
DOOM_VAR 0x00075070, w_ammo
DOOM_VAR 0x000750F0, w_arms
DOOM_VAR 0x000751C8, w_ready

DOOM_VAR 0x000752C8, shortnum
DOOM_VAR 0x000752F0, tallnum

DOOM_VAR 0x000753C0, keyboxes

DOOM_VAR 0x00075458, tallpercent

DOOM_VAR 0x000756F0, message_is_important

DOOM_VAR 0x00075C94, numChannels

DOOM_VAR 0x00075CA0, volume_val

///

