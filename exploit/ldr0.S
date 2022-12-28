//// kgsws' DOOM2 ACE engine
// First stage loader.

#define _printf	0x3FE40
#define _CheckNumForName	0x38AF0
#define _ReadLump	0x38BF0

.section .text
.globl _start

_start:
	// get EIP
	mov	-4(%esp),%esi

	// get CODE base
	sub	$0x34615,%esi

	// get pointer to 'visplanes'
	mov	0x361F1(%esi),%ebx

	// get DATA base
	mov	%ebx,%edi
	sub	$0x3A1E0,%edi

	//
	// intro text

	lea	_line_text(%esi),%ebp

	push	%edi
	push	%esi
	push	%ebp
	push	%ebp

	lea	_code_text(%esi),%ebp
	push	%ebp

	// printf
	lea	_printf(%esi),%ebp
	call	*%ebp
	add	$24,%esp

	//
	// find second stage

	// W_CheckNumForName
	lea	_lump_name(%esi),%eax
	lea	_CheckNumForName(%esi),%ebp
	call	*%ebp

	// check
	test	%eax,%eax
	jns	do_load

	// error message
	lea	_lump_error(%esi),%ebp
	push	%ebp
	lea	_newline(%esi),%ebp
	push	%ebp
	lea	_printf(%esi),%ebp
	call	*%ebp

	// exit to DOS
	mov	$0x4c01,%ax
	int	$0x21

do_load:
	// info text

	push	%eax
	lea	_lump_name(%esi),%ebp
	push	%ebp
	lea	_lump_text(%esi),%ebp
	push	%ebp

	// printf
	lea	_printf(%esi),%ebp
	call	*%ebp
	pop	%eax
	pop	%eax
	pop	%eax

	//
	// read second stage to 'visplanes'

	mov	%ebx,%edx
	lea	_ReadLump(%esi),%ebp
	call	*%ebp

	// jump to second stage
	// EDI = data base
	// ESI = code base
	jmp	*%ebx

//
// strings

	// error message
_lump_error:
	.ascii	"- missing " // fall trough to '_lump_name'

	// lump name
_lump_name:
	.string	"ACE_LDR1"

	// intro text
_line_text:
	.string	"+--------------------+\n"
_code_text:
	.string	"%s| Loading ACE Engine |\n%s- CODE 0x%08X\n- DATA 0x%08X\n"

	// info string
_lump_text:
	.string	"- %s lump %u\n"

	// error string
_newline:
	.string	"%s!\n"

