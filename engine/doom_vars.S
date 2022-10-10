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

DOOM_VAR 0x00012720, prndindex

DOOM_VAR 0x00012C5C, automapactive
DOOM_VAR 0x00012CA8, am_lastlevel

DOOM_VAR 0x00013C04, mapnames
DOOM_VAR 0x00013CBC, mapnames2

DOOM_VAR 0x0002929C, finaleflat
DOOM_VAR 0x000292A0, finaletext
DOOM_VAR 0x000292A4, finalecount
DOOM_VAR 0x000292A8, finalestage

DOOM_VAR 0x00029730, wadfiles

DOOM_VAR 0x0002A384, demosequence
DOOM_VAR 0x0002A38C, advancedemo

DOOM_VAR 0x0002A3AC, gamemode_sw
DOOM_VAR 0x0002A3B4, gamemode_reg
DOOM_VAR 0x0002A3BC, gamemode
DOOM_VAR 0x0002A3C0, nomonsters
DOOM_VAR 0x0002A3C4, fastparm
DOOM_VAR 0x0002A3C8, respawnparm

DOOM_VAR 0x0002ADB0, wminfo

DOOM_VAR 0x0002AE78, players

DOOM_VAR 0x0002B2D8, playeringame

DOOM_VAR 0x0002B2E8, defdemoname
DOOM_VAR 0x0002B2EC, d_skill

DOOM_VAR 0x0002B2F8, secretexit

DOOM_VAR 0x0002B33C, mousey

DOOM_VAR 0x0002B388, netdemo

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

DOOM_VAR 0x0002B698, screenblocks

DOOM_VAR 0x0002B6CC, menuactive

DOOM_VAR 0x0002B840, activeceilings

DOOM_VAR 0x0002B960, spechit
DOOM_VAR 0x0002B990, nofit
DOOM_VAR 0x0002B994, crushchange
DOOM_VAR 0x0002B9D8, numspechit
DOOM_VAR 0x0002B9E4, tmdropoffz
DOOM_VAR 0x0002B9F0, tmfloorz
DOOM_VAR 0x0002B9F8, linetarget
DOOM_VAR 0x0002BA00, tmceilingz

DOOM_VAR 0x0002C02C, lowfloor
DOOM_VAR 0x0002C030, openbottom
DOOM_VAR 0x0002C034, opentop
DOOM_VAR 0x0002C038, openrange

DOOM_VAR 0x0002C040, activeplats

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
DOOM_VAR 0x0002CFA0, solidsegs

DOOM_VAR 0x000300A4, curline
DOOM_VAR 0x000300A8, ds_p
DOOM_VAR 0x000300AC, backsector
DOOM_VAR 0x000300B0, frontsector

DOOM_VAR 0x000322D8, dc_colormap
DOOM_VAR 0x000322E4, ds_source
DOOM_VAR 0x000322E8, dc_source
DOOM_VAR 0x000322EC, dc_x
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

DOOM_VAR 0x00038FFC, fixedcolormap

DOOM_VAR 0x00039010, colfunc
DOOM_VAR 0x00039014, extralight
DOOM_VAR 0x00039020, xtoviewangle

DOOM_VAR 0x00039528, viewangle
DOOM_VAR 0x0003952C, centeryfrac
DOOM_VAR 0x00039534, centerx
DOOM_VAR 0x00039538, centery
DOOM_VAR 0x00039540, viewx
DOOM_VAR 0x00039544, viewy
DOOM_VAR 0x00039548, viewz

DOOM_VAR 0x0004F420, ceilingclip
DOOM_VAR 0x0004F6A0, floorclip

DOOM_VAR 0x0004F920, yslope

DOOM_VAR 0x0005A150, planeheight
DOOM_VAR 0x0005A154, planezlight
DOOM_VAR 0x0005A164, skyflatnum
DOOM_VAR 0x0005A168, ceilingplane
DOOM_VAR 0x0005A16C, floorplane
DOOM_VAR 0x0005A170, skytexture
DOOM_VAR 0x0005A1E4, rw_angle1
DOOM_VAR 0x0005A1EC, markceiling
DOOM_VAR 0x0005A1F4, markfloor
DOOM_VAR 0x0005A1FC, rw_normalangle

DOOM_VAR 0x0005A200, rw_distance

DOOM_VAR 0x0005C888, sprtopscreen
DOOM_VAR 0x0005C88C, mceilingclip
DOOM_VAR 0x0005C890, mfloorclip
DOOM_VAR 0x0005C894, vissprite_p
DOOM_VAR 0x0005C8A0, spryscale
DOOM_VAR 0x0005C8A4, vsprsortedhead

DOOM_VAR 0x00074FC0, usegamma

///


