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
	// get ACE base
	mov	(%esp),%ecx
	sub	$0x1004,%ecx	// compensate for VMA offset (set in linker script) and bss_size

	// get CODE base
	mov	%esi,doom_code_segment(%ecx)

	// get DATA base
	mov	%edi,doom_data_segment(%ecx)

	// run custom code
	push	%ecx
	call	ace_main
	pop	%ecx

	// continue loading Doom
	mov	doom_code_segment(%ecx),%eax
	add	$0x1E4AC,%eax
	jmp	*%eax

	// exit
.global dos_exit
dos_exit:
	mov	4(%esp),%eax
	and	$0xff,%eax
	mov	$0x4c,%ah
	int	$0x21

//
// Stored variables for later.
.global doom_code_segment
doom_code_segment:
	.4byte	0
.global doom_data_segment
doom_data_segment:
	.4byte	0

