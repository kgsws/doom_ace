// kgsws' ACE Engine
////
// This file contains various engine bug fixes and updates.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "ldr_texture.h"
#include "render.h"

//
// 'medusa' effect fix

static __attribute((regparm(2),no_caller_saved_registers))
void draw_masked_column(uint32_t texture, uint32_t column)
{
	void *data;
	void *composite;

	// get texture data, as usual
	data = R_GetColumn(texture, column);

	// check if this column is composite
	composite = (*texturecomposite)[texture];
	if(composite && data >= composite && data < composite + (*texturecompositesize)[texture])
	{
		// it is; draw solid
		int32_t bot, xx, yy;

		xx = *dc_x;

		bot = *sprtopscreen + *spryscale * ((*textureheight)[texture] >> FRACBITS);

		yy = (*sprtopscreen + FRACUNIT - 1) >> FRACBITS;
		if(yy <= (*mceilingclip)[xx])
			yy = (*mceilingclip)[xx] + 1;
		*dc_yl = yy;

		yy = (bot - 1) >> FRACBITS;
		if(yy >= (*mfloorclip)[xx])
			yy = (*mfloorclip)[xx] - 1;
		*dc_yh = yy;

		*dc_source = data;
		(*colfunc)();
	} else
		// it is not; draw as usual
		R_DrawMaskedColumn(data - 3);
}

//
// hooks
static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// disable 'store demo' feature
	{0x0001D113, CODE_HOOK | HOOK_UINT16, 0x4CEB},
	// fix blaze door double closing sound
	{0x0002690A, CODE_HOOK | HOOK_UINT16, 0x0BEB},
	// fix 'OUCH' face; this intentionaly fixes only 'OUCH' caused by enemies
	{0x0003A079, CODE_HOOK | HOOK_UINT8, 0xD7},
	{0x0003A081, CODE_HOOK | HOOK_UINT8, 0xFF},
	{0x0003A085, CODE_HOOK | HOOK_JMP_DOOM, 0x0003A136},
	{0x0003A137, CODE_HOOK | HOOK_UINT8, 8},
	{0x0003A134, CODE_HOOK | HOOK_UINT8, 0xEB},
	// fix 'medusa' effect
	{0x0003692D, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)draw_masked_column},
	{0x00036932, CODE_HOOK | HOOK_SET_NOPS, 8},
};

