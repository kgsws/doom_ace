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

	// get ACE base
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

	// TODO: clear .bss

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
	ret

//
// Relocation table for functions with 0, 1 or 2 arguments.
// Every entry is 'jmp 0x........' opcode. Offsets are relocated to "CODE segment".
.section .text.__reloc_code_012
__reloc_code_012:
////////////////////////////////// sprintf
.section .text.sprintf
.global sprintf
sprintf:
	.byte	0xE9
	.4byte	0x0004101C
////////////////////////////////// I_Error
.section .text.I_Error
.global I_Error
I_Error:
	.byte	0xE9
	.4byte	0x0001AB10
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
////////////////////////////////// end
.section .text.__reloc_code_012_end
__reloc_code_012_end:
////////////////////////////////////////////////////////

//
// Relocation table for functions with 3 arguments.
// Similar to '012' but there is some stack handling code. Jump is replaced with call.
.section .text.__reloc_code_arg3
__reloc_code_arg3:
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
////////////////////////////////// end
.section .text.__reloc_code_arg3_end
__reloc_code_arg3_end:
////////////////////////////////////////////////////////

//
// Relocation table for functions with 4 arguments.
// Similar to 'arg3'.
.section .text.__reloc_code_arg4
__reloc_code_arg4:
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
////////////////////////////////// end
.section .text.__reloc_code_arg4_end
__reloc_code_arg4_end:
////////////////////////////////////////////////////////


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


