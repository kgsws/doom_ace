// kgsws' ACE Engine
////

//
// column

.global R_DrawColumn
R_DrawColumn:
	pusha

	mov	dc_yh,%ecx
	mov	dc_yl,%eax

	mov	%ecx,%ebx

	sub	%eax,%ecx
	jl	skip_dc

	// dst pointer
	mov	ylookup(,%ebx,4),%edi
	mov	dc_x,%ebx
	add	columnofs(,%ebx,4),%edi

	// calculate jump
	mov	r_dc_jump(,%ecx,4),%ecx
	mov	%ecx,loop_jump_dst

	// texture setup
	mov	dc_iscale,%ebp
	sub	centery,%eax
	imul	%ebp
	mov	dc_texturemid,%edx
	add	%eax,%edx
	shl	$9,%edx	// +1
	shl	$9,%ebp	// +1
	mov	dc_source,%esi
	mov	dc_colormap,%eax
	xor	%ebx,%ebx
	shld	$7,%edx,%ebx	// -1

	mov	$25,%cl // +1

	jmp	*(loop_jump_dst)

	// loop
.global loop_dc_start
loop_dc_start:
	mov	(%esi,%ebx,1),%al
	add	%ebp,%edx
	mov	(%eax),%al
	mov	%edx,%ebx
	shr	%cl,%ebx
	mov	%al,0xC0DEFEED(%edi)
loop_dc_stop:

	// end
skip_dc:
	popa
	ret

//
// span

.global R_DrawSpan
R_DrawSpan:
	pusha

	mov	ds_x2,%ebp
	mov	ds_x1,%eax

	mov	%ebp,%ebx

	sub	%eax,%ebp

	// dst pointer
	mov	ds_y,%eax
	mov	ylookup(,%eax,4),%edi
	add	columnofs(,%ebx,4),%edi

	// calculate jump
	mov	r_ds_jump(,%ebp,4),%ecx
	mov	%ecx,loop_jump_dst

	// texture setup
	mov	ds_xfrac,%ecx
	shl	$10,%ecx
	and	$0xFFFF0000,%ecx
	mov	ds_yfrac,%eax
	shr	$6,%eax
	and	$0x0000FFFF,%eax
	or	%eax,%ecx

	mov	ds_xstep,%edx
	shl	$10,%edx
	and	$0xFFFF0000,%edx
	mov	ds_ystep,%eax
	shr	$6,%eax
	and	$0x0000FFFF,%eax
	or	%eax,%edx

	mov	ds_source,%esi
	mov	ds_colormap,%eax

	mov	$0x0FFF,%ebp
	shld	$22,%ecx,%ebx
	shld	$6,%ecx,%ebx
	and	%ebp,%ebx

	jmp	*(loop_jump_dst)

	// loop
.global loop_ds_start
loop_ds_start:
	mov	(%esi,%ebx),%al
	shld	$22,%ecx,%ebx
	shld	$6,%ecx,%ebx
	mov	(%eax),%al
	and	%ebp,%ebx
	add	%edx,%ecx
	mov	%al,0xC0DEFEED(%edi)
loop_ds_stop:

//
// tables

.section .bss

loop_jump_dst:
	.4byte	0

.global r_dc_jump
r_dc_jump:
	.fill 200*4

.global r_dc_unroll
r_dc_unroll:
	.fill (loop_dc_stop-loop_dc_start)*200+2

.global r_ds_jump
r_ds_jump:
	.fill 320*4

.global r_ds_unroll
r_ds_unroll:
	.fill (loop_ds_stop-loop_ds_start)*320+2

