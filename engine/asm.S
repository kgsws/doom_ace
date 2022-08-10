// kgsws' Doom ACE
// this is an initialization code
//

.section .text
.global _start

// store .bss size at the top of binary
	.4byte	_size_of_bss

//
// Entry point
// (%esp) - entry point address
// %esi - CODE segment base
// %edi - DATA segment base
_start:
	// loader already backed-up all registers

	// get ACE base
	mov	(%esp),%ecx
	sub	$0x1004,%ecx	// compensate for VMA offset (set in linker script) and bss_size

	// get CODE base
	mov	%esi,doom_code_segment(%ecx)

	// get DATA base
	mov	%edi,doom_data_segment(%ecx)

	// relocate ACE
	// this takes data from ELF
	// this is only handles one type of relocation
	lea	_reloc_start(%ecx),%ebx
	lea	_reloc_end(%ecx),%edx
rel_ace_loop:
	cmp	%edx,%ebx
	jge	rel_ace_end
	mov	(%ebx),%eax
	test	%eax,%eax
	jz	rel_ace_end
	add	%ecx,(%ecx,%eax,1)
	add	$8,%ebx
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

	// continue loading Doom
	mov	doom_code_segment,%eax
	add	$0x1e4ac,%eax
	jmp	*%eax

	// exit
.global dos_exit
dos_exit:
	and	$0xff,%eax
	mov	$0x4c,%ah
	int	$0x21

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
////////////////////////////////// I_Error
.section .text.I_Error
.global I_Error
I_Error:
	.byte	0xE9
	.4byte	0x0001AB10
////////////////////////////////// R_RenderPlayerView
.section .text.R_RenderPlayerView
.global R_RenderPlayerView
R_RenderPlayerView:
	.byte	0xE9
	.4byte	0x00035FB0
////////////////////////////////// ST_Init
.section .text.ST_Init
.global ST_Init
ST_Init:
	.byte	0xE9
	.4byte	0x0003AFA0
////////////////////////////////// W_CheckNumForName
.section .text.W_CheckNumForName
.global W_CheckNumForName
W_CheckNumForName:
	.byte	0xE9
	.4byte	0x00038AF0
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
////////////////////////////////// Z_Free
.section .text.Z_Free
.global Z_Free
Z_Free:
	.byte	0xE9
	.4byte	0x00039600
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
////////////////////////////////// end
.section .text.__reloc_code_arg5_end
__reloc_code_arg5_end:
////////////////////////////////////////////////////////

//
// Stored variables for later.
.global doom_code_segment
doom_code_segment:
	.4byte	0
.global doom_data_segment
doom_data_segment:
	.4byte	0

