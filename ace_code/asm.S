// kgsws' Doom ACE
// this is an initialization code
//

.section .text
.global _start

// store .bss size at the top of binary
	.4byte	_size_of_bss
//
// Entry point
// %eax - entry point address
// %ebx - CODE segment base
// %ecx - DATA segment base
_start:
	// loader already backed-up all registers

	// get ACE base // 0x0024E038
	sub	$0x1004,%eax	// compensate for VMA offset (set in linker script) and '_size_of_bss'
	mov	%eax,%esi	// store base in ESI for use
	mov	%eax,ace_segment(%esi)

	// get CODE base
	mov	%ebx,doom_code_segment(%esi)

	// get DATA base
	mov	%ecx,doom_data_segment(%esi)

	// relocate 012 table
	lea	__reloc_code_012(%esi),%eax
	lea	__reloc_code_012_end(%esi),%edx
rel_012_loop:
	cmp	%edx,%eax
	jge	rel_012_end
	add	%ebx,1(%eax)	// add CODE base to the address
	add	$5,%eax	// each entry is 5 bytes long
	sub	%eax,-4(%eax)
	jmp	rel_012_loop
rel_012_end:

	// relocate arg3 table
	lea	__reloc_code_arg3(%esi),%eax
	lea	__reloc_code_arg3_end(%esi),%edx
rel_arg3_loop:
	cmp	%edx,%eax
	jge	rel_arg3_end
	add	%ebx,6(%eax)	// add CODE base to the address
	add	$10,%eax	// 10 bytes
	sub	%eax,-4(%eax)
	add	$2,%eax	// 2 bytes (12 total)
	jmp	rel_arg3_loop
rel_arg3_end:

	// relocate arg4 table
	lea	__reloc_code_arg4(%esi),%eax
	lea	__reloc_code_arg4_end(%esi),%edx
rel_arg4_loop:
	cmp	%edx,%eax
	jge	rel_arg4_end
	add	%ebx,11(%eax)	// add CODE base to the address
	add	$15,%eax	// 15 bytes
	sub	%eax,-4(%eax)
	add	$3,%eax	// 3 bytes (18 total)
	jmp	rel_arg4_loop
rel_arg4_end:

	// clear .bss
	// TODO: better code
	xor	%eax,%eax
	mov	$_bss_start,%ebx
	add	%esi,%ebx
	mov	$_size_of_bss,%ecx
bss_loop:
	mov	%ax,(%ebx)
	dec	%ecx
	jnz	bss_loop

	// run custom code
	call	ace_init

	// restore registers
	pop	%ebp
	pop	%edi
	pop	%esi
	pop	%edx
	pop	%ecx
	pop	%ebx
	// continue the game
	jmp	__return_to_game

//
// IRQ trough DPMI
.section .text.dpmi_irq
.global dpmi_irq
dpmi_irq:
	push	%edi
	push	%ebx
	mov	12(%esp),%ebx
	mov	$0x0300,%ax
	xor	%cx,%cx
	mov	16(%esp),%edi
	int	$0x31
	pop	%ebx
	pop	%edi
	ret

//
// skip map loading on error
.global bad_map_warning
bad_map_warning:
	add	$0x38,%esp
	jmp	__bad_map_skip

//
// Relocation table for functions with 0, 1 or 2 arguments.
// Every entry is 'jmp 0x........' opcode. Offsets are relocated to "CODE segment".
.section .text.__reloc_code_012
__reloc_code_012:
////////////////////////////////// special hooks
__return_to_game:
	.byte	0xE9
	.4byte	0x00020843
__bad_map_skip:
	.byte	0xE9
	.4byte	0x0002E997
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
////////////////////////////////// I_Error
.section .text.I_Error
.global I_Error
I_Error:
	.byte	0xE9
	.4byte	0x0001AB10
////////////////////////////////// I_FinishUpdate
.section .text.I_FinishUpdate
.global I_FinishUpdate
I_FinishUpdate:
	.byte	0xE9
	.4byte	0x00019F60
////////////////////////////////// I_SetPalette
.section .text.I_SetPalette
.global I_SetPalette
I_SetPalette:
	.byte	0xE9
	.4byte	0x00019CA0
////////////////////////////////// R_GenerateLookup
.section .text.R_GenerateLookup
.global R_GenerateLookup
R_GenerateLookup:
	.byte	0xE9
	.4byte	0x00033D40
////////////////////////////////// R_GenerateComposite
.section .text.R_GenerateComposite
.global R_GenerateComposite
R_GenerateComposite:
	.byte	0xE9
	.4byte	0x00033BB0
////////////////////////////////// R_DrawColumnInCache
.section .text.R_DrawColumnInCache // never called from C; must stay in this group
.global R_DrawColumnInCache
R_DrawColumnInCache:
	.byte	0xE9
	.4byte	0x00033B40
////////////////////////////////// P_InitSwitchList
.section .text.P_InitSwitchList
.global P_InitSwitchList
P_InitSwitchList:
	.byte	0xE9
	.4byte	0x000301F0
////////////////////////////////// R_InitSkyMap
.section .text.R_InitSkyMap
.global R_InitSkyMap
R_InitSkyMap:
	.byte	0xE9
	.4byte	0x00036000
////////////////////////////////// R_InitLightTables
.section .text.R_InitLightTables
.global R_InitLightTables
R_InitLightTables:
	.byte	0xE9
	.4byte	0x000359B0
////////////////////////////////// D_StartTitle
.section .text.D_StartTitle
.global D_StartTitle
D_StartTitle:
	.byte	0xE9
	.4byte	0x0001D810
////////////////////////////////// ST_Init
.section .text.ST_Init
.global ST_Init
ST_Init:
	.byte	0xE9
	.4byte	0x0003AFA0
////////////////////////////////// HU_Init
.section .text.HU_Init
.global HU_Init
HU_Init:
	.byte	0xE9
	.4byte	0x0003B4B0
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
////////////////////////////////// M_StringHeight
.section .text.M_StringHeight
.global M_StringHeight
M_StringHeight:
	.byte	0xE9
	.4byte	0x00022F50
////////////////////////////////// M_StringWidth
.section .text.M_StringWidth
.global M_StringWidth
M_StringWidth:
	.byte	0xE9
	.4byte	0x00022F00
////////////////////////////////// M_StartControlPanel
.section .text.M_StartControlPanel
.global M_StartControlPanel
M_StartControlPanel:
	.byte	0xE9
	.4byte	0x00023CD0
////////////////////////////////// S_StartSound
.section .text.S_StartSound
.global S_StartSound
S_StartSound:
	.byte	0xE9
	.4byte	0x0003F370
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
////////////////////////////////// W_CacheLumpNum
.section .text.W_CacheLumpNum
.global W_CacheLumpNum
W_CacheLumpNum:
	.byte	0xE9
	.4byte	0x00038D00
////////////////////////////////// W_CacheLumpName
.section .text.W_CacheLumpName
.global W_CacheLumpName
W_CacheLumpName:
	.byte	0xE9
	.4byte	0x00038CF0
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
////////////////////////////////// Z_Free
.section .text.Z_Free
.global Z_Free
Z_Free:
	.byte	0xE9
	.4byte	0x00039600
////////////////////////////////// R_RenderPlayerView
.section .text.R_RenderPlayerView
.global R_RenderPlayerView
R_RenderPlayerView:
	.byte	0xE9
	.4byte	0x00035FB0
////////////////////////////////// R_DrawPlayerSprites
.section .text.R_DrawPlayerSprites
.global R_DrawPlayerSprites
R_DrawPlayerSprites:
	.byte	0xE9
	.4byte	0x00038210
////////////////////////////////// R_SetupFrame
.section .text.R_SetupFrame
.global R_SetupFrame
R_SetupFrame:
	.byte	0xE9
	.4byte	0x00035E80
////////////////////////////////// R_ExecuteSetViewSize
.section .text.R_ExecuteSetViewSize
.global R_ExecuteSetViewSize
R_ExecuteSetViewSize:
	.byte	0xE9
	.4byte	0x00035A70
////////////////////////////////// G_BuildTiccmd
.section .text.G_BuildTiccmd
.global G_BuildTiccmd
G_BuildTiccmd:
	.byte	0xE9
	.4byte	0x0001FB10
////////////////////////////////// P_SpawnPlayer
.section .text.P_SpawnPlayer
.global P_SpawnPlayer
P_SpawnPlayer:
	.byte	0xE9
	.4byte	0x000317F0
////////////////////////////////// P_SpawnSpecials
.section .text.P_SpawnSpecials
.global P_SpawnSpecials
P_SpawnSpecials:
	.byte	0xE9
	.4byte	0x0002FFF0
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
////////////////////////////////// P_SetMobjState
.section .text.P_SetMobjState
.global P_SetMobjState
P_SetMobjState:
	.byte	0xE9
	.4byte	0x00030EA0
////////////////////////////////// P_PlayerInSpecialSector
.section .text.P_PlayerInSpecialSector
.global P_PlayerInSpecialSector
P_PlayerInSpecialSector:
	.byte	0xE9
	.4byte	0x0002FB20
////////////////////////////////// P_TouchSpecialThing
.section .text.P_TouchSpecialThing
.global P_TouchSpecialThing
P_TouchSpecialThing:
	.byte	0xE9
	.4byte	0x00029AE0
////////////////////////////////// P_ChangeSwitchTexture
.section .text.P_ChangeSwitchTexture
.global P_ChangeSwitchTexture
P_ChangeSwitchTexture:
	.byte	0xE9
	.4byte	0x00030310
////////////////////////////////// P_ChangeSector
.section .text.P_ChangeSector
.global P_ChangeSector
P_ChangeSector:
	.byte	0xE9
	.4byte	0x0002BF90
////////////////////////////////// P_RemoveThinker
.section .text.P_RemoveThinker
.global P_RemoveThinker
P_RemoveThinker:
	.byte	0xE9
	.4byte	0x00032EF0
////////////////////////////////// P_AddThinker
.section .text.P_AddThinker
.global P_AddThinker
P_AddThinker:
	.byte	0xE9
	.4byte	0x00032ED0
////////////////////////////////// P_FindLowestCeilingSurrounding
.section .text.P_FindLowestCeilingSurrounding
.global P_FindLowestCeilingSurrounding
P_FindLowestCeilingSurrounding:
	.byte	0xE9
	.4byte	0x0002F170
////////////////////////////////// G_ExitLevel
.section .text.G_ExitLevel
.global G_ExitLevel
G_ExitLevel:
	.byte	0xE9
	.4byte	0x00020D40
////////////////////////////////// R_PointToAngle
.section .text.R_PointToAngle
.global R_PointToAngle
R_PointToAngle:
	.byte	0xE9
	.4byte	0x00035510
////////////////////////////////// P_ExplodeMissile
.section .text.P_ExplodeMissile
.global P_ExplodeMissile
P_ExplodeMissile:
	.byte	0xE9
	.4byte	0x00030F00
////////////////////////////////// FixedDiv
.section .text.FixedDiv
.global FixedDiv
FixedDiv:
	.byte	0xE9
	.4byte	0x0001D180
////////////////////////////////// end
.section .text.__reloc_code_012_end
__reloc_code_012_end:
////////////////////////////////////////////////////////

//
// Relocation table for functions with 3 arguments.
// Similar to '012' but there is some stack handling code. Jump is replaced with call.
.section .text.__reloc_code_arg3
__reloc_code_arg3:
////////////////////////////////// read
.section .text.read
.global read
read:
	push	%ebx
	mov	8(%esp),%ebx
	.byte	0xE8
	.4byte	0x00040935
	pop	%ebx
	ret
////////////////////////////////// write
.section .text.write
.global write
write:
	push	%ebx
	mov	8(%esp),%ebx
	.byte	0xE8
	.4byte	0x0004159a
	pop	%ebx
	ret
////////////////////////////////// lseek
.section .text.lseek
.global lseek
lseek:
	push	%ebx
	mov	8(%esp),%ebx
	.byte	0xE8
	.4byte	0x000408c0
	pop	%ebx
	ret
////////////////////////////////// M_WriteText
.section .text.M_WriteText
.global M_WriteText
M_WriteText:
	push	%ebx
	mov	8(%esp),%ebx
	.byte	0xE8
	.4byte	0x00022FA0
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
////////////////////////////////// P_SpawnPuff
.section .text.P_SpawnPuff
.global P_SpawnPuff
P_SpawnPuff:
	push	%ebx
	mov	8(%esp),%ebx
	.byte	0xE8
	.4byte	0x00031B00
	pop	%ebx
	ret
////////////////////////////////// end
.section .text.__reloc_code_arg3_end
__reloc_code_arg3_end:
////////////////////////////////////////////////////////

//
// Relocation table for functions with 4 arguments.
// Similar to 'arg3'.
.section .text.__reloc_code_arg4
__reloc_code_arg4:
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
////////////////////////////////// V_DrawPatchDirect
.section .text.V_DrawPatchDirect
.global V_DrawPatchDirect
V_DrawPatchDirect:
	push	%ebx
	push	%ecx
	mov	12(%esp),%ebx
	mov	16(%esp),%ecx
	.byte	0xE8
	.4byte	0x000392A0
	pop	%ecx
	pop	%ebx
	ret
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
////////////////////////////////// I_UpdateBox
.section .text.I_UpdateBox
.global I_UpdateBox
I_UpdateBox:
	push	%ebx
	push	%ecx
	mov	12(%esp),%ebx
	mov	16(%esp),%ecx
	.byte	0xE8
	.4byte	0x00019d20
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
////////////////////////////////// P_DamageMobj
.section .text.P_DamageMobj
.global P_DamageMobj
P_DamageMobj:
	push	%ebx
	push	%ecx
	mov	12(%esp),%ebx
	mov	16(%esp),%ecx
	.byte	0xE8
	.4byte	0x0002A460
	pop	%ecx
	pop	%ebx
	ret
////////////////////////////////// end
.section .text.__reloc_code_arg4_end
__reloc_code_arg4_end:
////////////////////////////////////////////////////////

//
// ASM engine hooks

.section .text.medusa_cache_fix
.global medusa_cache_fix
medusa_cache_fix:
	push	%eax
	push	%edx
	mov	(%edx),%al
	dec	%edx
	mov	%al,(%edx)	// padding pixel
	dec	%edx
	mov	%cl,(%edx)	// height
	dec	%edx
	mov	%ch,(%edx)	// should be zero
	add	%ecx,%edx
	add	$4,%edx
	movb	$255,(%edx)	// ending
	pop	%edx
	pop	%eax
	jmp	R_DrawColumnInCache

.section .rodata

//
// Stored variables for later.
.global doom_code_segment
doom_code_segment:
	.4byte	0
.global doom_data_segment
doom_data_segment:
	.4byte	0
.global ace_segment
ace_segment:
	.4byte	0


