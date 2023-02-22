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
	mov	r_dc_mask,%cl
	shl	%cl,%edx	// +1
	shl	%cl,%ebp	// +1
	mov	dc_source,%esi
	mov	dc_colormap,%eax
	xor	%ebx,%ebx
	mov	r_dc_mask+1,%cl
	shld	%cl,%edx,%ebx	// -1

	mov	r_dc_mask+2,%cl

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
	// this version is slower (27 vs 24 FPS) but much more precise

	mov	ds_xstep,%ebx
	shl	$10,%ebx
	mov	%ebx,ds_xstep

	mov	ds_ystep,%ebx
	shl	$10,%ebx
	mov	%ebx,ds_ystep

	mov	ds_xfrac,%ebx
	shl	$10,%ebx
	mov	ds_yfrac,%ecx
	shl	$10,%ecx

	mov	ds_source,%esi
	mov	ds_colormap,%eax

	jmp	*(loop_jump_dst)

	// loop
.global loop_ds_start
loop_ds_start:
	mov	%ecx,%ebp
	shr	$26,%ebp

	mov	%ebx,%edx
	shrd	$26,%ebp,%edx

//	and	$0x0FFF,%edx
	mov	(%esi,%edx),%al
	mov	(%eax),%al
	add	ds_xstep,%ebx
	add	ds_ystep,%ecx
	mov	%al,0xC0DEFEED(%edi)
loop_ds_stop:

//
// tables & stuff

.section .bss

loop_jump_dst:
	.4byte	0

.global r_dc_mask
r_dc_mask:
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

