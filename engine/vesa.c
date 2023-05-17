// kgsws' Doom ACE
// VESA graphic modes.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "vesa.h"

typedef struct
{
	uint32_t signature;		// must be "VESA" to indicate valid VBE support
	uint16_t version;		// VBE version; high byte is major version, low byte is minor version
	uint32_t oem;			// segment:offset pointer to OEM
	uint32_t capabilities;		// bitfield that describes card capabilities
	uint32_t video_modes;		// segment:offset pointer to list of supported video modes
	uint16_t video_memory;		// amount of video memory in 64KB blocks
	uint16_t software_rev;		// software revision
	uint32_t vendor;			// segment:offset to card vendor string
	uint32_t product_name;		// segment:offset to card model name
	uint32_t product_rev;		// segment:offset pointer to product revision
	char reserved[222];		// reserved for future expansion
	char oem_data[256];		// OEM BIOSes store their strings in this area
} __attribute__ ((packed)) vesa_info_t;

typedef struct
{
	uint16_t attributes;		// deprecated, only bit 7 should be of interest to you, and it indicates the mode supports a linear frame buffer.
	uint8_t window_a;		// deprecated
	uint8_t window_b;		// deprecated
	uint16_t granularity;		// deprecated; used while calculating bank numbers
	uint16_t window_size;
	uint16_t segment_a;
	uint16_t segment_b;
	uint32_t win_func_ptr;		// deprecated; used to switch banks from protected mode without returning to real mode
	uint16_t pitch;			// number of bytes per horizontal line
	uint16_t width;			// width in pixels
	uint16_t height;		// height in pixels
	uint8_t w_char;			// unused...
	uint8_t y_char;			// ...
	uint8_t planes; // 1
	uint8_t bpp; // 8		// bits per pixel in this mode
	uint8_t banks;			// deprecated; total number of banks in this mode
	uint8_t memory_model;	// 4?
	uint8_t bank_size;		// deprecated; size of a bank, almost always 64 KB but may be 16 KB...
	uint8_t image_pages;		// double-buffer (0 = single screen)
	uint8_t reserved0;
	uint8_t red_mask;
	uint8_t red_position;
	uint8_t green_mask;
	uint8_t green_position;
	uint8_t blue_mask;
	uint8_t blue_position;
	uint8_t reserved_mask;
	uint8_t reserved_position;
	uint8_t direct_color_attributes;
	void *framebuffer;		// physical address of the linear frame buffer; write here to draw to the screen
	uint32_t off_screen_mem_off;
	uint16_t off_screen_mem_size;	// size of memory in the framebuffer but not being displayed on the screen
	uint8_t reserved1[206];
} __attribute__ ((packed)) vesa_mode_info_t;

//

static uint8_t *framebase;
uint8_t *framebuffer;
uint8_t *wipebuffer;
int32_t vesa_offset = -1;

//
// hooks

static void swap_buffers();

static const hook_t patch_vesa[] =
{
	// replace framebuffer pointer in 'R_InitBuffer'
	{0x00034E77, CODE_HOOK | HOOK_UINT32, (uint32_t)&framebase},
	// replace 'I_FinishUpdate'
	{0x0001D4A6, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)swap_buffers},
	{0x0001D4FC, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)swap_buffers},
	// disable mode set in 'I_InitGraphics'
	{0x0001A04E, CODE_HOOK | HOOK_SET_NOPS, 5},
	// terminator
	{0}
};

//
// buffering calls

//
// update screen

static void swap_buffers()
{
	wipebuffer = framebase + vesa_offset;
	vesa_offset ^= SCREENWIDTH * SCREENHEIGHT;
	framebuffer = framebase + vesa_offset;

	dpmiregs.eax.x = 0x4F07; // Display Start Control
	dpmiregs.ebx.x = 0x80; // Set Display Start during Vertical Retrace
	dpmiregs.ecx.x = 0;
	dpmiregs.edx.x = vesa_offset ? 0 : SCREENHEIGHT;
	dpmi_irq(0x10);
}

//
// API

void vesa_check()
{
	vesa_info_t *info;
	vesa_mode_info_t *mode;
	uint16_t *mode_list;
	uint32_t do_list = M_CheckParm("-vesalist");

	info = (void*)screen_buffer;
	mode = (void*)screen_buffer;

	// get VESA info
	if(!M_CheckParm("-novesa"))
	{
		memset(info, 0, sizeof(vesa_info_t));
		info->signature = 0x32454256;

		dpmiregs.es = (uint32_t)screen_buffer >> 4;
		dpmiregs.ds = dpmiregs.es;
		dpmiregs.ss = dpmiregs.es;
		dpmiregs.edi.ex = 0;
		dpmiregs.eax.x = 0x4F00; // get VESA info
		dpmi_irq(0x10);
	} else
		dpmiregs.eax.x = 0;

	if(dpmiregs.eax.x == 0x004F && info->signature == 0x41534556)
	{
		doom_printf("[VESA] version %u.%u\n", info->version >> 8, info->version & 0xFF);
		if(info->version >= 0x0200)
		{
			mode_list = (uint16_t*)(((info->video_modes & 0xFFFF0000) >> 12) + (info->video_modes & 0xFFFF));
			while(1)
			{
				if(*mode_list == 0xFFFF)
					break;

				dpmiregs.es = (uint32_t)screen_buffer >> 4;
				dpmiregs.ds = dpmiregs.es;
				dpmiregs.ss = dpmiregs.es;
				dpmiregs.edi.ex = 0;
				dpmiregs.eax.x = 0x4F01; // get mode info
				dpmiregs.ecx.x = *mode_list;
				dpmi_irq(0x10);

				if(dpmiregs.eax.x == 0x004F)
				{
					if(do_list)
						doom_printf("[VESA] mode 0x%04X: %ux%ux%u %u 0x%04X 0x%08X\n", *mode_list, mode->width, mode->height, mode->bpp, mode->image_pages, mode->attributes, (uint32_t)mode->framebuffer);

					if(	mode->width == SCREENWIDTH &&
						mode->height == SCREENHEIGHT &&
						mode->bpp == 8 &&
						mode->memory_model == 4 &&
						mode->image_pages > 0 &&
						(mode->attributes & 0x90) == 0x90
					){
						framebase = mode->framebuffer;
						vesa_offset = *mode_list;
						doom_printf("[VESA] found mode 0x%04X\n", vesa_offset);
						if(!do_list)
							break;
					}
				}

				mode_list++;
			}
		}
	} else
		doom_printf("[VESA] not detected\n");
}

void vesa_init()
{
	uint32_t vesa_on = 0;

	if(vesa_offset >= 0)
	{
		dpmiregs.eax.x = 0x4F02; // set mode
		dpmiregs.ebx.x = vesa_offset | (1 << 14);
		dpmi_irq(0x10);

		if(dpmiregs.eax.x == 0x004F)
		{
			doom_printf("[VESA] activated\n");
			utils_install_hooks(patch_vesa, 0);
			vesa_on = 1;
			// setup buffering
			vesa_offset = 0;
			framebuffer = framebase;
			wipebuffer = framebase;
		}
	}

	if(!vesa_on)
	{
		vesa_offset = 0;
		wipebuffer = (void*)0x000A0000;
		framebuffer = screen_buffer;
		framebase = screen_buffer;
	}

	I_InitGraphics();
}

void vesa_copy()
{
	if(wipebuffer == (void*)0x000A0000)
		return;

	dwcopy(framebuffer, wipebuffer, (SCREENWIDTH * SCREENHEIGHT) / sizeof(uint32_t));
}

void vesa_update()
{
	if(wipebuffer == (void*)0x000A0000)
	{
		I_FinishUpdate();
		if(!dev_mode)
			_hack_update();
		return;
	}

	swap_buffers();
}

