// kgsws' Doom ACE
// this is an initialization code
//

.section .text
.global _start

// store .bss size at the top of binary
	.4byte	_size_of_bss

// store version at the top of binary
.global ace_git_version
ace_git_version:
	.4byte	0x00000BAD

//
// Entry point
// (%esp) - entry point address
// %esi - CODE segment base
// %edi - DATA segment base
_start:
	// get ACE base
	sub	$0x1008,%ecx	// compensate for VMA offset (set in linker script) and start offset
	mov	%ecx,ace_segment(%ecx)

	// get CODE base
	mov	%esi,doom_code_segment(%ecx)

	// get DATA base
	mov	%edi,doom_data_segment(%ecx)

	// offset for relocator
	add	$-0x1000000,%edi

	// relocate ACE
	// relocations have to be processed by python script
	lea	_bss_start(%ecx),%ebx
rel_ace_loop:
	mov	(%ebx),%eax
	test	%eax,%eax
	jz	rel_ace_end
	lea	(%ecx,%eax,1),%edx
	testb	$0x01,3(%edx)
	jz	_rel_normal
	add	%edi,(%edx)
	jmp	_rel_next
_rel_normal:
	add	%ecx,(%edx)
_rel_next:
	add	$4,%ebx
	jmp	rel_ace_loop
rel_ace_end:
	// everything after this point was relocated
	// everything from ACE should be accessible directly

	// relocate 012 table
	mov	$__reloc_code_012,%eax
	mov	$__reloc_code_012_end,%edx
rel_012_loop:
	cmp	%edx,%eax
	jge	rel_012_end
	add	%esi,1(%eax)	// add CODE base to the address
	add	$5,%eax	// each entry is 5 bytes long
	sub	%eax,-4(%eax)
	jmp	rel_012_loop
rel_012_end:

	// relocate arg3 table
	mov	$__reloc_code_arg3,%eax
	mov	$__reloc_code_arg3_end,%edx
rel_arg3_loop:
	cmp	%edx,%eax
	jge	rel_arg3_end
	add	%esi,6(%eax)	// add CODE base to the address
	add	$10,%eax	// 10 bytes
	sub	%eax,-4(%eax)
	add	$2,%eax	// 2 bytes (12 total)
	jmp	rel_arg3_loop
rel_arg3_end:

	// relocate arg4 table
	mov	$__reloc_code_arg4,%eax
	mov	$__reloc_code_arg4_end,%edx
rel_arg4_loop:
	cmp	%edx,%eax
	jge	rel_arg4_end
	add	%esi,11(%eax)	// add CODE base to the address
	add	$15,%eax	// 15 bytes
	sub	%eax,-4(%eax)
	add	$3,%eax	// 3 bytes (18 total)
	jmp	rel_arg4_loop
rel_arg4_end:

	// relocate arg5 table
	mov	$__reloc_code_arg5,%eax
	mov	$__reloc_code_arg5_end,%edx
rel_arg5_loop:
	cmp	%edx,%eax
	jge	rel_arg5_end
	add	%esi,16(%eax)	// add CODE base to the address
	add	$20,%eax	// 20 bytes
	sub	%eax,-4(%eax)
	add	$3,%eax	// 3 bytes (23 total)
	jmp	rel_arg5_loop
rel_arg5_end:

	// clear .bss
	mov	$_bss_start,%ebx
	mov	$_bss_end,%edx
	xor	%eax,%eax
bss_loop:
	cmp	%edx,%ebx
	jge	bss_end
	mov	%eax,(%ebx)
	add	$4, %ebx
	jmp	bss_loop
bss_end:

	// run custom code
	call	ace_main

	// continue from after 'M_LoadDefaults'
	mov	doom_data_segment,%eax
	add	$0x00085BF4,%eax
	mov	%eax,%esp
	mov	doom_code_segment,%eax
	add	$0x0001E4B1,%eax
	jmp	*%eax

	// error exit; I_Error wrapper
.global engine_error
engine_error:
	pop	%eax
	pop	%eax
	mov	%eax,error_module
	call	I_Error

	// exit
.global dos_exit
dos_exit:
	and	$0xFF,%eax
	mov	$0x4C,%ah
	int	$0x21

//
// skip menu cursor and items
.global skip_message_cancel
skip_message_cancel:
	pop	%eax
	sub	doom_code_segment,%eax
	cmp	$0x2342B,%eax
	jne	skip_message_cancel
	mov	doom_code_segment,%eax
	add	$0x2344B,%eax
	jmp	*%eax

//
// skip menu draw
.global skip_menu_draw
skip_menu_draw:
	pop	%eax
	sub	doom_code_segment,%eax
	cmp	$0x23D10,%eax
	jne	skip_menu_draw
	mov	doom_code_segment,%eax
	add	$0x23F4E,%eax
	jmp	*%eax

//
// I_FinishUpdate replacement
.global I_FinishUpdate
I_FinishUpdate:
	push	%edi
	push	%esi
	push	%ecx
	mov	$0x12345678,%esi	// this address will be overwritten
	mov	$0xA0000,%edi
	mov	$0x3E80,%ecx
	rep	movsl %ds:(%esi),%es:(%edi)
	pop	%ecx
	pop	%esi
	pop	%edi
	ret

//
// P_DamageMobj hook
.global hook_mobj_damage
hook_mobj_damage:
	push	$0
	push	%ecx
	push	%ebx
	push	%edx
	push	%eax
	call	mobj_damage
	add	$20,%esp
	ret

//
// P_PathTraverse hook
.global hook_path_traverse
hook_path_traverse:
	push	%ecx
	push	%ebx
	push	%edx
	push	%eax
	mov	0x10(%esp),%eax
	mov	0x18(%esp),%ebx
	mov	%eax,0x18(%esp)
	mov	%ebx,0x10(%esp)
	call	path_traverse
	add	$24,%esp
	ret

//
// EV_DoLockedDoor hook
.global hook_obj_key
hook_obj_key:
	mov	%ebx,%edx
	jmp	check_obj_key

//
// S_AdjustSoundParams hook
.global hook_sound_adjust
hook_sound_adjust:
	push	%ecx
	push	%ebx
	push	%edx
	push	%eax
	call	sound_adjust
	add	$16,%esp
	ret	$4

//
// R_RenderMaskedSegRange hook
.global hook_masked_range_draw
hook_masked_range_draw:
	push	%ebx
	call	R_RenderMaskedSegRange
	add	$4,%esp
	ret

//
// I_Error hook
.global hook_bluescreen
hook_bluescreen:
	add	$8,%esp
	movl	$d_drawsegs,(%esp)
	call	doom_sprintf
	jmp	bluescreen

//
// V_CopyRect - this one is nasty
.global V_CopyRect
V_CopyRect:
	push	%eax
	push	%edx
	push	%ebx
	push	%ecx
	mov	0x30(%esp),%eax
	push	%eax
	mov	0x30(%esp),%eax
	push	%eax
	mov	0x30(%esp),%eax
	push	%eax
	mov	0x30(%esp),%eax
	push	%eax
	mov	0x24(%esp),%eax
	mov	0x28(%esp),%edx
	mov	0x2C(%esp),%ebx
	mov	0x30(%esp),%ecx
	call	doom_V_CopyRect
	pop	%ecx
	pop	%ebx
	pop	%edx
	pop	%eax
	ret

//
// new RNG
.global rng_asm_code
rng_asm_code:
	mov	prndindex,%eax
	inc	%eax
	cmp	rng_max,%eax
	jl	rng_skip
	xor	%eax,%eax
rng_skip:
	mov	%eax,prndindex
	movzb	rng_table(%eax),%eax
	ret

//
// Relocation table for functions with 0, 1 or 2 arguments.
// Every entry is 'jmp 0x........' opcode. Offsets are relocated to "CODE segment".
.section .text.__reloc_code_012
__reloc_code_012:
////////////////////////////////// doom_printf
.section .text.doom_printf
.global doom_printf
doom_printf:
	.byte	0xE9
	.4byte	0x0003FE40
////////////////////////////////// doom_sprintf
.section .text.doom_sprintf
.global doom_sprintf
doom_sprintf:
	.byte	0xE9
	.4byte	0x0004101C
////////////////////////////////// doom_sscanf
.section .text.doom_sscanf
.global doom_sscanf
doom_sscanf:
	.byte	0xE9
	.4byte	0x00040AD4
////////////////////////////////// doom_fprintf
.section .text.doom_fprintf
.global doom_fprintf
doom_fprintf:
	.byte	0xE9
	.4byte	0x0004104F
////////////////////////////////// doom_free
.section .text.doom_free
.global doom_free
doom_free:
	.byte	0xE9
	.4byte	0x0004066D
////////////////////////////////// doom_malloc
.section .text.doom_malloc
.global doom_malloc
doom_malloc:
	.byte	0xE9
	.4byte	0x00040602
////////////////////////////////// doom_realloc
.section .text.doom_realloc
.global doom_realloc
doom_realloc:
	.byte	0xE9
	.4byte	0x00040EE3
////////////////////////////////// doom_open
.section .text.doom_open
.global doom_open
doom_open:
	.byte	0xE9
	.4byte	0x00040691
////////////////////////////////// doom_close
.section .text.doom_close
.global doom_close
doom_close:
	.byte	0xE9
	.4byte	0x00040902
////////////////////////////////// doom_filelength
.section .text.doom_filelength
.global doom_filelength
doom_filelength:
	.byte	0xE9
	.4byte	0x000419EF
////////////////////////////////// doom_fopen
.section .text.doom_fopen
.global doom_fopen
doom_fopen:
	.byte	0xE9
	.4byte	0x00040217
////////////////////////////////// doom_fclose
.section .text.doom_fclose
.global doom_fclose
doom_fclose:
	.byte	0xE9
	.4byte	0x0004043A
////////////////////////////////// doom_unlink
.section .text.doom_unlink
.global doom_unlink
doom_unlink:
	.byte	0xE9
	.4byte	0x000444FE
////////////////////////////////// doom_access
.section .text.doom_access
.global doom_access
doom_access:
	.byte	0xE9
	.4byte	0x0004107F
////////////////////////////////// doom_mkdir
.section .text.doom_mkdir
.global doom_mkdir
doom_mkdir:
	.byte	0xE9
	.4byte	0x000413F1
////////////////////////////////// dpmi_get_ram
.section .text.dpmi_get_ram
.global dpmi_get_ram
dpmi_get_ram:
	.byte	0xE9
	.4byte	0x0001AC20
////////////////////////////////// _hack_update
.section .text._hack_update
.global _hack_update
_hack_update:
	.byte	0xE9
	.4byte	0x0001D50A
////////////////////////////////// FixedDiv
.section .text.FixedDiv
.global FixedDiv
FixedDiv:
	.byte	0xE9
	.4byte	0x0001D180
////////////////////////////////// I_StartupSound
.section .text.I_StartupSound
.global I_StartupSound
I_StartupSound:
	.byte	0xE9
	.4byte	0x0001B700
////////////////////////////////// I_WaitVBL
.section .text.I_WaitVBL
.global I_WaitVBL
I_WaitVBL:
	.byte	0xE9
	.4byte	0x00019C60
////////////////////////////////// I_GetTime
.section .text.I_GetTime
.global I_GetTime
I_GetTime:
	.byte	0xE9
	.4byte	0x00019C10
////////////////////////////////// I_Error
.section .text.I_Error
.global I_Error
I_Error:
	.byte	0xE9
	.4byte	0x0001AB10
////////////////////////////////// I_InitGraphics
.section .text.I_InitGraphics
.global I_InitGraphics
I_InitGraphics:
	.byte	0xE9
	.4byte	0x0001A010
////////////////////////////////// I_UpdateNoBlit
.section .text.I_UpdateNoBlit
.global I_UpdateNoBlit
I_UpdateNoBlit:
	.byte	0xE9
	.4byte	0x00019E60
////////////////////////////////// I_SetPalette
.section .text.I_SetPalette
.global I_SetPalette
I_SetPalette:
	.byte	0xE9
	.4byte	0x00019CA0
////////////////////////////////// M_CheckParm
.section .text.M_CheckParm
.global M_CheckParm
M_CheckParm:
	.byte	0xE9
	.4byte	0x00024110
////////////////////////////////// R_GenerateComposite
.section .text.R_GenerateComposite
.global R_GenerateComposite
R_GenerateComposite:
	.byte	0xE9
	.4byte	0x00033BB0
////////////////////////////////// R_GenerateLookup
.section .text.R_GenerateLookup
.global R_GenerateLookup
R_GenerateLookup:
	.byte	0xE9
	.4byte	0x00033D40
////////////////////////////////// R_PointToAngle
.section .text.R_PointToAngle
.global R_PointToAngle
R_PointToAngle:
	.byte	0xE9
	.4byte	0x00035510
////////////////////////////////// R_AddLine
.section .text.R_AddLine
.global R_AddLine
R_AddLine:
	.byte	0xE9
	.4byte	0x00033720
////////////////////////////////// R_ScaleFromGlobalAngle
.section .text.R_ScaleFromGlobalAngle
.global R_ScaleFromGlobalAngle
R_ScaleFromGlobalAngle:
	.byte	0xE9
	.4byte	0x000357D0
////////////////////////////////// R_PointToDist
.section .text.R_PointToDist
.global R_PointToDist
R_PointToDist:
	.byte	0xE9
	.4byte	0x00035760
////////////////////////////////// R_PointInSubsector
.section .text.R_PointInSubsector
.global R_PointInSubsector
R_PointInSubsector:
	.byte	0xE9
	.4byte	0x00035E20
////////////////////////////////// R_SortVisSprites
.section .text.R_SortVisSprites
.global R_SortVisSprites
R_SortVisSprites:
	.byte	0xE9
	.4byte	0x000382B0
////////////////////////////////// R_DrawSprite
.section .text.R_DrawSprite
.global R_DrawSprite
R_DrawSprite:
	.byte	0xE9
	.4byte	0x000383A0
////////////////////////////////// M_LoadDefaults
.section .text.M_LoadDefaults
.global M_LoadDefaults
M_LoadDefaults:
	.byte	0xE9
	.4byte	0x000243C0
////////////////////////////////// ST_Init
.section .text.ST_Init
.global ST_Init
ST_Init:
	.byte	0xE9
	.4byte	0x0003AFA0
////////////////////////////////// ST_Start
.section .text.ST_Start
.global ST_Start
ST_Start:
	.byte	0xE9
	.4byte	0x0003AF20
////////////////////////////////// ST_doPaletteStuff
.section .text.ST_doPaletteStuff
.global ST_doPaletteStuff
ST_doPaletteStuff:
	.byte	0xE9
	.4byte	0x0003A410
////////////////////////////////// ST_Drawer
.section .text.ST_Drawer
.global ST_Drawer
ST_Drawer:
	.byte	0xE9
	.4byte	0x0003A600
////////////////////////////////// HU_Start
.section .text.HU_Start
.global HU_Start
HU_Start:
	.byte	0xE9
	.4byte	0x0003B530
////////////////////////////////// HU_dequeueChatChar
.section .text.HU_dequeueChatChar
.global HU_dequeueChatChar
HU_dequeueChatChar:
	.byte	0xE9
	.4byte	0x0003B920
////////////////////////////////// HU_queueChatChar
.section .text.HU_queueChatChar
.global HU_queueChatChar
HU_queueChatChar:
	.byte	0xE9
	.4byte	0x0003B8E0
////////////////////////////////// HUlib_drawTextLine
.section .text.HUlib_drawTextLine
.global HUlib_drawTextLine
HUlib_drawTextLine:
	.byte	0xE9
	.4byte	0x0003BDA0
////////////////////////////////// P_LoadBlockMap
.section .text.P_LoadBlockMap
.global P_LoadBlockMap
P_LoadBlockMap:
	.byte	0xE9
	.4byte	0x0002E4F0
////////////////////////////////// P_LoadSectors
.section .text.P_LoadSectors
.global P_LoadSectors
P_LoadSectors:
	.byte	0xE9
	.4byte	0x0002DF40
////////////////////////////////// P_LoadSideDefs
.section .text.P_LoadSideDefs
.global P_LoadSideDefs
P_LoadSideDefs:
	.byte	0xE9
	.4byte	0x0002E3E0
////////////////////////////////// P_LoadSubsectors
.section .text.P_LoadSubsectors
.global P_LoadSubsectors
P_LoadSubsectors:
	.byte	0xE9
	.4byte	0x0002DE90
////////////////////////////////// P_LoadNodes
.section .text.P_LoadNodes
.global P_LoadNodes
P_LoadNodes:
	.byte	0xE9
	.4byte	0x0002E030
////////////////////////////////// P_LoadSegs
.section .text.P_LoadSegs
.global P_LoadSegs
P_LoadSegs:
	.byte	0xE9
	.4byte	0x0002DD50
////////////////////////////////// doom_LoadLineDefs
.section .text.doom_LoadLineDefs
.global doom_LoadLineDefs
doom_LoadLineDefs:
	.byte	0xE9
	.4byte	0x0002E220
////////////////////////////////// G_DeathMatchSpawnPlayer
.section .text.G_DeathMatchSpawnPlayer
.global G_DeathMatchSpawnPlayer
G_DeathMatchSpawnPlayer:
	.byte	0xE9
	.4byte	0x00020BC0
////////////////////////////////// P_GroupLines
.section .text.P_GroupLines
.global P_GroupLines
P_GroupLines:
	.byte	0xE9
	.4byte	0x0002E5B0
////////////////////////////////// P_InitThinkers
.section .text.P_InitThinkers
.global P_InitThinkers
P_InitThinkers:
	.byte	0xE9
	.4byte	0x00032EB0
////////////////////////////////// R_RenderPlayerView
.section .text.R_RenderPlayerView
.global R_RenderPlayerView
R_RenderPlayerView:
	.byte	0xE9
	.4byte	0x00035FB0
////////////////////////////////// R_SetupFrame
.section .text.R_SetupFrame
.global R_SetupFrame
R_SetupFrame:
	.byte	0xE9
	.4byte	0x00035E80
////////////////////////////////// R_DrawPSprite
.section .text.R_DrawPSprite
.global R_DrawPSprite
R_DrawPSprite:
	.byte	0xE9
	.4byte	0x00038010
////////////////////////////////// W_CheckNumForName
.section .text.W_CheckNumForName
.global W_CheckNumForName
W_CheckNumForName:
	.byte	0xE9
	.4byte	0x00038AF0
////////////////////////////////// W_GetNumForName
.section .text.W_GetNumForName
.global W_GetNumForName
W_GetNumForName:
	.byte	0xE9
	.4byte	0x00038B80
////////////////////////////////// W_CacheLumpName
.section .text.W_CacheLumpName
.global W_CacheLumpName
W_CacheLumpName:
	.byte	0xE9
	.4byte	0x00038CF0
////////////////////////////////// W_CacheLumpNum
.section .text.W_CacheLumpNum
.global W_CacheLumpNum
W_CacheLumpNum:
	.byte	0xE9
	.4byte	0x00038D00
////////////////////////////////// W_LumpLength
.section .text.W_LumpLength
.global W_LumpLength
W_LumpLength:
	.byte	0xE9
	.4byte	0x00038BB0
////////////////////////////////// W_ReadLump
.section .text.W_ReadLump
.global W_ReadLump
W_ReadLump:
	.byte	0xE9
	.4byte	0x00038BF0
////////////////////////////////// Z_ChangeTag2
.section .text.Z_ChangeTag2
.global Z_ChangeTag2
Z_ChangeTag2:
	.byte	0xE9
	.4byte	0x000399B0
////////////////////////////////// Z_Free
.section .text.Z_Free
.global Z_Free
Z_Free:
	.byte	0xE9
	.4byte	0x00039600
////////////////////////////////// Z_FreeTags
.section .text.Z_FreeTags
.global Z_FreeTags
Z_FreeTags:
	.byte	0xE9
	.4byte	0x00039780
////////////////////////////////// AM_Stop
.section .text.AM_Stop
.global AM_Stop
AM_Stop:
	.byte	0xE9
	.4byte	0x00024F90
////////////////////////////////// I_StartTic
.section .text.I_StartTic
.global I_StartTic
I_StartTic:
	.byte	0xE9
	.4byte	0x0001A1B0
////////////////////////////////// D_CheckNetGame
.section .text.D_CheckNetGame
.global D_CheckNetGame
D_CheckNetGame:
	.byte	0xE9
	.4byte	0x0001F5C0
////////////////////////////////// HSendPacket
.section .text.HSendPacket
.global HSendPacket
HSendPacket:
	.byte	0xE9
	.4byte	0x0001EC30
////////////////////////////////// HGetPacket
.section .text.HGetPacket
.global HGetPacket
HGetPacket:
	.byte	0xE9
	.4byte	0x0001ED70
////////////////////////////////// F_StartCast
.section .text.F_StartCast
.global F_StartCast
F_StartCast:
	.byte	0xE9
	.4byte	0x0001C6F0
////////////////////////////////// WI_Start
.section .text.WI_Start
.global WI_Start
WI_Start:
	.byte	0xE9
	.4byte	0x0003EB70
////////////////////////////////// S_Start
.section .text.S_Start
.global S_Start
S_Start:
	.byte	0xE9
	.4byte	0x0003F600
////////////////////////////////// S_StartSound
.section .text.S_StartSound
.global S_StartSound
S_StartSound:
	.byte	0xE9
	.4byte	0x0003F370
////////////////////////////////// doom_S_StopSound
.section .text.doom_S_StopSound
.global doom_S_StopSound
doom_S_StopSound:
	.byte	0xE9
	.4byte	0x0003F010
////////////////////////////////// S_ResumeSound
.section .text.S_ResumeSound
.global S_ResumeSound
S_ResumeSound:
	.byte	0xE9
	.4byte	0x0003EFE0
////////////////////////////////// S_StopMusic
.section .text.S_StopMusic
.global S_StopMusic
S_StopMusic:
	.byte	0xE9
	.4byte	0x0003EC40
////////////////////////////////// S_ChangeMusic
.section .text.S_ChangeMusic
.global S_ChangeMusic
S_ChangeMusic:
	.byte	0xE9
	.4byte	0x0003ECF0
////////////////////////////////// P_Random
.section .text.P_Random
.global P_Random
P_Random:
	.byte	0xE9
	.4byte	0x00024160
////////////////////////////////// M_Random
.section .text.M_Random
.global M_Random
M_Random:
	.byte	0xE9
	.4byte	0x00024180
////////////////////////////////// P_AddThinker
.section .text.P_AddThinker
.global P_AddThinker
P_AddThinker:
	.byte	0xE9
	.4byte	0x00032ED0
////////////////////////////////// P_RemoveThinker
.section .text.P_RemoveThinker
.global P_RemoveThinker
P_RemoveThinker:
	.byte	0xE9
	.4byte	0x00032EF0
////////////////////////////////// P_RunThinkers
.section .text.P_RunThinkers
.global P_RunThinkers
P_RunThinkers:
	.byte	0xE9
	.4byte	0x00032F10
////////////////////////////////// P_SetThingPosition
.section .text.P_SetThingPosition
.global P_SetThingPosition
P_SetThingPosition:
	.byte	0xE9
	.4byte	0x0002C460
////////////////////////////////// P_UnsetThingPosition
.section .text.P_UnsetThingPosition
.global P_UnsetThingPosition
P_UnsetThingPosition:
	.byte	0xE9
	.4byte	0x0002C3B0
////////////////////////////////// P_SlideMove
.section .text.P_SlideMove
.global P_SlideMove
P_SlideMove:
	.byte	0xE9
	.4byte	0x0002B560
////////////////////////////////// P_HitSlideLine
.section .text.P_HitSlideLine
.global P_HitSlideLine
P_HitSlideLine:
	.byte	0xE9
	.4byte	0x0002B3D0
////////////////////////////////// PIT_ChangeSector
.section .text.PIT_ChangeSector
.global PIT_ChangeSector
PIT_ChangeSector:
	.byte	0xE9
	.4byte	0x0002BEA0
////////////////////////////////// P_KillMobj
.section .text.P_KillMobj
.global P_KillMobj
P_KillMobj:
	.byte	0xE9
	.4byte	0x0002A2B0
////////////////////////////////// P_PlayerInSpecialSector
.section .text.P_PlayerInSpecialSector
.global P_PlayerInSpecialSector
P_PlayerInSpecialSector:
	.byte	0xE9
	.4byte	0x0002FB20
////////////////////////////////// P_ShootSpecialLine
.section .text.P_ShootSpecialLine
.global P_ShootSpecialLine
P_ShootSpecialLine:
	.byte	0xE9
	.4byte	0x0002FA70
////////////////////////////////// P_UseLines
.section .text.P_UseLines
.global P_UseLines
P_UseLines:
	.byte	0xE9
	.4byte	0x0002BD10
////////////////////////////////// P_DeathThink
.section .text.P_DeathThink
.global P_DeathThink
P_DeathThink:
	.byte	0xE9
	.4byte	0x00033270
////////////////////////////////// P_SpawnPlayerMissile
.section .text.P_SpawnPlayerMissile
.global P_SpawnPlayerMissile
P_SpawnPlayerMissile:
	.byte	0xE9
	.4byte	0x00031D50
////////////////////////////////// P_NightmareRespawn
.section .text.P_NightmareRespawn
.global P_NightmareRespawn
P_NightmareRespawn:
	.byte	0xE9
	.4byte	0x00031390
////////////////////////////////// P_LineOpening
.section .text.P_LineOpening
.global P_LineOpening
P_LineOpening:
	.byte	0xE9
	.4byte	0x0002C340
////////////////////////////////// P_BulletSlope
.section .text.P_BulletSlope
.global P_BulletSlope
P_BulletSlope:
	.byte	0xE9
	.4byte	0x0002D7C0
////////////////////////////////// P_NoiseAlert
.section .text.P_NoiseAlert
.global P_NoiseAlert
P_NoiseAlert:
	.byte	0xE9
	.4byte	0x00027020
////////////////////////////////// P_LookForPlayers
.section .text.P_LookForPlayers
.global P_LookForPlayers
P_LookForPlayers:
	.byte	0xE9
	.4byte	0x00027570
////////////////////////////////// M_ClearMenus
.section .text.M_ClearMenus
.global M_ClearMenus
M_ClearMenus:
	.byte	0xE9
	.4byte	0x00023F60
////////////////////////////////// M_Drawer
.section .text.M_Drawer
.global M_Drawer
M_Drawer:
	.byte	0xE9
	.4byte	0x00023D00
////////////////////////////////// R_ExecuteSetViewSize
.section .text.R_ExecuteSetViewSize
.global R_ExecuteSetViewSize
R_ExecuteSetViewSize:
	.byte	0xE9
	.4byte	0x00035A70
////////////////////////////////// P_AddActivePlat
.section .text.P_AddActivePlat
.global P_AddActivePlat
P_AddActivePlat:
	.byte	0xE9
	.4byte	0x0002CFA0
////////////////////////////////// P_AddActiveCeiling
.section .text.P_AddActiveCeiling
.global P_AddActiveCeiling
P_AddActiveCeiling:
	.byte	0xE9
	.4byte	0x00026690
////////////////////////////////// P_SpawnSpecials
.section .text.P_SpawnSpecials
.global P_SpawnSpecials
P_SpawnSpecials:
	.byte	0xE9
	.4byte	0x0002FFF0
////////////////////////////////// P_InterceptVector
.section .text.P_InterceptVector
.global P_InterceptVector
P_InterceptVector:
	.byte	0xE9
	.4byte	0x0002C2D0
////////////////////////////////// P_TraverseIntercepts
.section .text.P_TraverseIntercepts
.global P_TraverseIntercepts
P_TraverseIntercepts:
	.byte	0xE9
	.4byte	0x0002C810
////////////////////////////////// R_PrecacheLevel
.section .text.R_PrecacheLevel
.global R_PrecacheLevel
R_PrecacheLevel:
	.byte	0xE9
	.4byte	0x00034780
////////////////////////////////// G_BuildTiccmd
.section .text.G_BuildTiccmd
.global G_BuildTiccmd
G_BuildTiccmd:
	.byte	0xE9
	.4byte	0x0001FB10
////////////////////////////////// G_CheckDemoStatus
.section .text.G_CheckDemoStatus
.global G_CheckDemoStatus
G_CheckDemoStatus:
	.byte	0xE9
	.4byte	0x00021C40
////////////////////////////////// M_ClearBox
.section .text.M_ClearBox
.global M_ClearBox
M_ClearBox:
	.byte	0xE9
	.4byte	0x000241C0
////////////////////////////////// EV_DoDoor
.section .text.EV_DoDoor
.global EV_DoDoor
EV_DoDoor:
	.byte	0xE9
	.4byte	0x00026AB0
////////////////////////////////// doom_A_Chase
.section .text.doom_A_Chase
.global doom_A_Chase
doom_A_Chase:
	.byte	0xE9
	.4byte	0x00027790
////////////////////////////////// doom_A_VileChase
.section .text.doom_A_VileChase
.global doom_A_VileChase
doom_A_VileChase:
	.byte	0xE9
	.4byte	0x00028100
////////////////////////////////// doom_A_BrainAwake
.section .text.doom_A_BrainAwake
.global doom_A_BrainAwake
doom_A_BrainAwake:
	.byte	0xE9
	.4byte	0x00028AC0
////////////////////////////////// doom_A_BrainSpit
.section .text.doom_A_BrainSpit
.global doom_A_BrainSpit
doom_A_BrainSpit:
	.byte	0xE9
	.4byte	0x00028C60
////////////////////////////////// doom_A_SpawnFly
.section .text.doom_A_SpawnFly
.global doom_A_SpawnFly
doom_A_SpawnFly:
	.byte	0xE9
	.4byte	0x00028D20
////////////////////////////////// doom_V_CopyRect
.section .text.doom_V_CopyRect
.global doom_V_CopyRect
doom_V_CopyRect:
	.byte	0xE9
	.4byte	0x00038F10
////////////////////////////////// P_BoxOnLineSide
.section .text.P_BoxOnLineSide
.global P_BoxOnLineSide
P_BoxOnLineSide:
	.byte	0xE9
	.4byte	0x0002C0D0
////////////////////////////////// P_FindLowestCeilingSurrounding
.section .text.P_FindLowestCeilingSurrounding
.global P_FindLowestCeilingSurrounding
P_FindLowestCeilingSurrounding:
	.byte	0xE9
	.4byte	0x0002F170
////////////////////////////////// P_FindLowestFloorSurrounding
.section .text.P_FindLowestFloorSurrounding
.global P_FindLowestFloorSurrounding
P_FindLowestFloorSurrounding:
	.byte	0xE9
	.4byte	0x0002F020
////////////////////////////////// end
.section .text.__reloc_code_012_end
__reloc_code_012_end:
////////////////////////////////////////////////////////

//
// Relocation table for functions with 3 arguments.
// Similar to '012' but there is some stack handling code. Jump is replaced with call.
.section .text.__reloc_code_arg3
__reloc_code_arg3:
////////////////////////////////// doom_read
.section .text.doom_read
.global doom_read
doom_read:
	push	%ebx
	mov	8(%esp),%ebx
	.byte	0xE8
	.4byte	0x00040935
	pop	%ebx
	ret
////////////////////////////////// doom_write
.section .text.doom_write
.global doom_write
doom_write:
	push	%ebx
	mov	8(%esp),%ebx
	.byte	0xE8
	.4byte	0x0004159a
	pop	%ebx
	ret
////////////////////////////////// doom_lseek
.section .text.doom_lseek
.global doom_lseek
doom_lseek:
	push	%ebx
	mov	8(%esp),%ebx
	.byte	0xE8
	.4byte	0x000408c0
	pop	%ebx
	ret
////////////////////////////////// Z_Malloc
.section .text.Z_Malloc
.global Z_Malloc
Z_Malloc:
	push	%ebx
	mov	8(%esp),%ebx
	.byte	0xE8
	.4byte	0x00039690
	pop	%ebx
	ret
////////////////////////////////// P_TryMove
.section .text.P_TryMove
.global P_TryMove
P_TryMove:
	push	%ebx
	mov	8(%esp),%ebx
	.byte	0xE8
	.4byte	0x0002B200
	pop	%ebx
	ret
////////////////////////////////// P_CheckPosition
.section .text.P_CheckPosition
.global P_CheckPosition
P_CheckPosition:
	push	%ebx
	mov	8(%esp),%ebx
	.byte	0xE8
	.4byte	0x0002B060
	pop	%ebx
	ret
////////////////////////////////// M_AddToBox
.section .text.M_AddToBox
.global M_AddToBox
M_AddToBox:
	push	%ebx
	mov	8(%esp),%ebx
	.byte	0xE8
	.4byte	0x000241E0
	pop	%ebx
	ret
////////////////////////////////// M_StartMessage
.section .text.M_StartMessage
.global M_StartMessage
M_StartMessage:
	push	%ebx
	mov	8(%esp),%ebx
	.byte	0xE8
	.4byte	0x00022EA0
	pop	%ebx
	ret
////////////////////////////////// G_DeferedInitNew
.section .text.G_DeferedInitNew
.global G_DeferedInitNew
G_DeferedInitNew:
	push	%ebx
	mov	8(%esp),%ebx
	.byte	0xE8
	.4byte	0x00021540
	pop	%ebx
	ret
////////////////////////////////// doom_P_AimLineAttack
.section .text.doom_P_AimLineAttack
.global doom_P_AimLineAttack
doom_P_AimLineAttack:
	push	%ebx
	mov	8(%esp),%ebx
	.byte	0xE8
	.4byte	0x0002BB80
	pop	%ebx
	ret
////////////////////////////////// P_BlockThingsIterator
.section .text.P_BlockThingsIterator
.global P_BlockThingsIterator
P_BlockThingsIterator:
	push	%ebx
	mov	8(%esp),%ebx
	.byte	0xE8
	.4byte	0x0002C5A0
	pop	%ebx
	ret
////////////////////////////////// P_PointOnDivlineSide
.section .text.P_PointOnDivlineSide
.global P_PointOnDivlineSide
P_PointOnDivlineSide:
	push	%ebx
	mov	8(%esp),%ebx
	.byte	0xE8
	.4byte	0x0002C1E0
	pop	%ebx
	ret
////////////////////////////////// P_PointOnLineSide
.section .text.P_PointOnLineSide
.global P_PointOnLineSide
P_PointOnLineSide:
	push	%ebx
	mov	8(%esp),%ebx
	.byte	0xE8
	.4byte	0x0002C010
	pop	%ebx
	ret
////////////////////////////////// R_ClipPassWallSegment
.section .text.R_ClipPassWallSegment
.global R_ClipPassWallSegment
R_ClipPassWallSegment:
	push	%ebx
	mov	8(%esp),%ebx
	.byte	0xE8
	.4byte	0x00033670
	pop	%ebx
	ret
////////////////////////////////// R_ClipSolidWallSegment
.section .text.R_ClipSolidWallSegment
.global R_ClipSolidWallSegment
R_ClipSolidWallSegment:
	push	%ebx
	mov	8(%esp),%ebx
	.byte	0xE8
	.4byte	0x000335A0
	pop	%ebx
	ret
////////////////////////////////// R_FindPlane
.section .text.R_FindPlane
.global R_FindPlane
R_FindPlane:
	push	%ebx
	mov	8(%esp),%ebx
	.byte	0xE8
	.4byte	0x00036270
	pop	%ebx
	ret
////////////////////////////////// P_TeleportMove
.section .text.P_TeleportMove
.global P_TeleportMove
P_TeleportMove:
	push	%ebx
	mov	8(%esp),%ebx
	.byte	0xE8
	.4byte	0x0002AC00
	pop	%ebx
	ret////////////////////////////////// end
.section .text.__reloc_code_arg3_end
__reloc_code_arg3_end:
////////////////////////////////////////////////////////

//
// Relocation table for functions with 4 arguments.
// Similar to 'arg3'.
.section .text.__reloc_code_arg4
__reloc_code_arg4:
////////////////////////////////// V_DrawPatch
.section .text.V_DrawPatch
.global V_DrawPatch
V_DrawPatch:
	push	%ebx
	push	%ecx
	mov	12(%esp),%ebx
	mov	16(%esp),%ecx
	.byte	0xE8
	.4byte	0x00039020
	pop	%ecx
	pop	%ebx
	ret
////////////////////////////////// R_InstallSpriteLump
.section .text.R_InstallSpriteLump
.global R_InstallSpriteLump
R_InstallSpriteLump:
	push	%ebx
	push	%ecx
	mov	12(%esp),%ebx
	mov	16(%esp),%ecx
	.byte	0xE8
	.4byte	0x00037650
	pop	%ecx
	pop	%ebx
	ret
////////////////////////////////// R_PointToAngle2
.section .text.R_PointToAngle2
.global R_PointToAngle2
R_PointToAngle2:
	push	%ebx
	push	%ecx
	mov	12(%esp),%ebx
	mov	16(%esp),%ecx
	.byte	0xE8
	.4byte	0x00035740
	pop	%ecx
	pop	%ebx
	ret
////////////////////////////////// P_SpawnMobj
.section .text.P_SpawnMobj
.global P_SpawnMobj
P_SpawnMobj:
	push	%ebx
	push	%ecx
	mov	12(%esp),%ebx
	mov	16(%esp),%ecx
	.byte	0xE8
	.4byte	0x00031540
	pop	%ecx
	pop	%ebx
	ret
////////////////////////////////// V_MarkRect
.section .text.V_MarkRect
.global V_MarkRect
V_MarkRect:
	push	%ebx
	push	%ecx
	mov	12(%esp),%ebx
	mov	16(%esp),%ecx
	.byte	0xE8
	.4byte	0x00038EE0
	pop	%ecx
	pop	%ebx
	ret
////////////////////////////////// end
.section .text.__reloc_code_arg4_end
__reloc_code_arg4_end:
////////////////////////////////////////////////////////

//
// Relocation table for functions with 5 arguments.
// Similar to 'arg4' but 5th argument is placed at correct location on the stack.
.section .text.__reloc_code_arg5
__reloc_code_arg5:
////////////////////////////////// P_LineAttack
.section .text.P_LineAttack
.global P_LineAttack
P_LineAttack:
	push	%ebx
	push	%ecx
	mov	20(%esp),%ecx
	push	%ecx
	mov	16(%esp),%ebx
	mov	20(%esp),%ecx
	.byte	0xE8
	.4byte	0x0002BC20
	pop	%ecx
	pop	%ebx
	ret
////////////////////////////////// R_MakeSpans
.section .text.R_MakeSpans
.global R_MakeSpans
R_MakeSpans:
	push	%ebx
	push	%ecx
	mov	20(%esp),%ecx
	push	%ecx
	mov	16(%esp),%ebx
	mov	20(%esp),%ecx
	.byte	0xE8
	.4byte	0x000363E0
	pop	%ecx
	pop	%ebx
	ret
////////////////////////////////// end
.section .text.__reloc_code_arg5_end
__reloc_code_arg5_end:
////////////////////////////////////////////////////////

//
// Stored variables for later.
.global ace_segment
ace_segment:
	.4byte	0
.global doom_code_segment
doom_code_segment:
	.4byte	0
.global doom_data_segment
doom_data_segment:
	.4byte	0

