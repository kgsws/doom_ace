// kgsws' ACE Engine
////

.global R_DrawColumn
R_DrawColumn:
	pusha

	mov	dc_yh,%ebp
	mov	dc_yl,%eax

	sub	%eax,%ebp
	jl	skip_dc

	// dst pointer
	mov	ylookup(,%eax,4),%edi
	mov	dc_x,%ebx
	add	columnofs(,%ebx,4),%edi

	// end pointer
	mov	%ebp,%ebx
	shl	$8,%ebx
	shl	$6,%ebp
	add	%ebx,%ebp
	add	%edi,%ebp
	add	$320,%ebp

	// texture setup
	mov	dc_iscale,%ecx
	sub	centery,%eax
	imul	%ecx
	mov	dc_texturemid,%edx
	add	%eax,%edx
	shl	$9,%edx
	shl	$9,%ecx
	mov	dc_source,%esi
	mov	dc_colormap,%eax
	xor	%ebx,%ebx
	shld	$7,%edx,%ebx

	// loop
loop_dc:
	mov	(%esi,%ebx,1),%al
	add	%ecx,%edx
	mov	(%eax),%al
	mov	%edx,%ebx
	shr	$25,%ebx
	mov	%al,(%edi)
	add	$320,%edi
	cmp	%edi,%ebp
	jnz	loop_dc

	// end
skip_dc:
	popa
	ret

