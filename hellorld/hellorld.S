//// kgsws' DOOM2 arbitrary code execution
// Hellorld!

.section .text
.globl _start

_start:
	// get EIP (this value is random for each machine)
	mov	-4(%esp),%esi

	// get CODE base
	sub	$0x34615,%esi

	// clear (some) text
	mov	$0xB8640,%edi
	mov	$0x0F00,%ax
	mov	$1040,%ecx
	rep	stosw (%edi)

	// print the text
	mov	$0xB8860,%edi
	lea	_txt_hellorld(%esi),%ebp
text_loop:
	mov	(%ebp),%al
	test	%al,%al
	jz	text_end
	mov	%al,(%edi)
	inc	%edi
	inc	%edi
	inc	%ebp
	jmp	text_loop
text_end:

	// exit to DOS
	mov	$0x4c01,%ax
	int	$0x21

//
// strings

_txt_hellorld:
	.string	"Hellorld!"

