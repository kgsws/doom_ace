// kgsws' Doom ACE
// VESA graphic modes.
#include "engine.h"
#include "utils.h"
#include "vesa.h"

typedef struct
{
	uint32_t signature;		// must be "VESA" to indicate valid VBE support
	uint16_t version;			// VBE version; high byte is major version, low byte is minor version
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
	uint8_t window_a;			// deprecated
	uint8_t window_b;			// deprecated
	uint16_t granularity;		// deprecated; used while calculating bank numbers
	uint16_t window_size;
	uint16_t segment_a;
	uint16_t segment_b;
	uint32_t win_func_ptr;		// deprecated; used to switch banks from protected mode without returning to real mode
	uint16_t pitch;			// number of bytes per horizontal line
	uint16_t width;			// width in pixels
	uint16_t height;			// height in pixels
	uint8_t w_char;			// unused...
	uint8_t y_char;			// ...
	uint8_t planes; // 1
	uint8_t bpp; // 8		// bits per pixel in this mode
	uint8_t banks;			// deprecated; total number of banks in this mode
	uint8_t memory_model; // 4?
	uint8_t bank_size;		// deprecated; size of a bank, almost always 64 KB but may be 16 KB...
	uint8_t image_pages; // double-buffer (0 = single screen)
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
	uint32_t framebuffer;		// physical address of the linear frame buffer; write here to draw to the screen
	uint32_t off_screen_mem_off;
	uint16_t off_screen_mem_size;	// size of memory in the framebuffer but not being displayed on the screen
	uint8_t reserved1[206];
} __attribute__ ((packed)) vesa_mode_info_t;

static void *screens;

static hook_t hook_list[] =
{
	{0x00074FC4, DATA_HOOK | HOOK_READ32, (uint32_t)&screens},
	// terminator
	{0}
};

void vesa_test()
{
	dpmi_regs_t dpmiregs;
	vesa_info_t *info;
	vesa_mode_info_t *mode;
	uint16_t *modes;

	utils_install_hooks(hook_list);

	info = screens;
	mode = screens;

	memset(&dpmiregs, 0, sizeof(dpmi_regs_t));

	// get VESA info
	memset(info, 0, sizeof(vesa_info_t));
	info->signature = 0x32454256;

	dpmiregs.es = (uint32_t)screens >> 4;
	dpmiregs.ds = dpmiregs.es;
	dpmiregs.ss = dpmiregs.es;
	dpmiregs.sp = 0xFF00;

	dpmiregs.eax.x = 0x4F00; // get VESA info

	dpmi_irq(0x10, &dpmiregs);

	if(dpmiregs.eax.x == 0x004F)
	{
		doom_printf("VESA version %d; mode table at 0x%08X\n", info->version, info->video_modes);
		if(info->version >= 0x0200)
		{
			modes = (uint16_t*)((info->video_modes >> 12) + (info->video_modes & 0xFFFF));
			while(1)
			{
				if(*modes == 0xFFFF)
					break;

				dpmiregs.es = (uint32_t)screens >> 4;
				dpmiregs.ds = dpmiregs.es;
				dpmiregs.ss = dpmiregs.es;
				dpmiregs.sp = 0xFF00;
				dpmiregs.eax.x = 0x4F01; // get mode info
				dpmiregs.ecx.x = *modes;
				dpmi_irq(0x10, &dpmiregs);

				doom_printf("mode 0x%04X: %ux%ux%u 0x%04X 0x%08X\n", *modes, mode->width, mode->height, mode->bpp, mode->attributes, mode->framebuffer);
				modes++;
			}
		}
	} else
		doom_printf("VESA not supported\n");
}

