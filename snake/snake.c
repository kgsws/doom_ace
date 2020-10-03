// kgsws' Doom ACE
// Snake! game example.
#include "engine.h"
#include "utils.h"
#include "defs.h"
#include "snake.h"

#define SNAKE_MAX_LENGTH	(2*1024)
#define SNAKE_LENGTH	64
#define SNAKE_SPACING	6
#define SNAKE_PILL_GROW	32
#define SNAKE_TURN	0x03C00000
#define EDGE_DISTANCE	4
#define TAIL_DISTANCE	3
#define PILL_DISTANCE	5
#define TAIL_SKIP	(SNAKE_SPACING*4)

typedef struct
{
	int x, y;
	const char *text;
} msgline_t;

typedef struct
{
	int16_t x, y;
} snake_pos_t;

hook_t hook_restore[] =
{
	// restore menu drawing
	{0x0001d4a9, CODE_HOOK | HOOK_RELADDR_DOOM, 0x00023D40},
	// restore menu input
	{0x0001d14a, CODE_HOOK | HOOK_RELADDR_DOOM, 0x00023050},
	// enable 'wipe' again
	{0x0001d4b4, CODE_HOOK | HOOK_UINT16, 0x0775}, // original instruction
	// terminator
	{0}
};

msgline_t snake_info[] =
{
	{0, 0, "Arbitrary code execution"},
	{0, 1, "in The Ultimate Doom"},
	{0, 2, "by kgsws"},
	{0, 4, "Use LEFT/RIGHT to turn"},
	{0, 5, "Press ENTER to start ..."},
	// terminator
	{0}
};

uint8_t snake_direction = 0xFF;
fixed_t snake_x, snake_y;
int pill_x, pill_y;
angle_t snake_a;
uint32_t snake_gametic;
int snake_grow_count;
int snake_length;
snake_pos_t *snake_position;
// gfx lumps
int snake_parts[3];
int snake_head;
int snake_tail;
int snake_pill;
int snake_dead;
int snake_finished;

static void snake_random_pill();

void snake_init()
{
	// get font height
	int font_height = M_StringHeight("Snake!") + 2;

	// prepare info text
	msgline_t *ln = snake_info;
	while(ln->text)
	{
		const char *text = (char*)relocate_addr_ace(ln->text);
		ln->x = 160 - M_StringWidth(text) / 2;
		ln->y = 64 + ln->y * font_height;
		ln->text = text;
		ln++;
	}

	// get some graphics
	snake_parts[0] = W_GetNumForName("STKEYS5"); // red
	snake_parts[1] = W_GetNumForName("STKEYS3"); // blue
	snake_parts[2] = W_GetNumForName("STKEYS4"); // yellow
	snake_dead = W_GetNumForName("END0"); // THE END
	snake_finished = W_GetNumForName("WIF"); // FINISHED

	// get memory for history
	snake_position = Z_Malloc(sizeof(snake_pos_t) * SNAKE_MAX_LENGTH, PU_STATIC, NULL);
}

void snake_drawer()
{
	// this function replaced M_Drawer, which is called every frame

	if(snake_direction == 0xFF)
	{
		// intro screen
		msgline_t *ln = snake_info;
		while(ln->text)
		{
			M_WriteText(ln->x, ln->y, ln->text);
			ln++;
		}
	} else
	{
		void *gfx_head = W_CacheLumpNum(snake_head, PU_CACHE);
		void *gfx_tail = W_CacheLumpNum(snake_tail, PU_CACHE);
		void *gfx_pill = W_CacheLumpNum(snake_pill, PU_CACHE);

		if(*gametic != snake_gametic && !(snake_direction & 0x80))
		{
			int x, y;
			// angle change
			if(snake_direction & 1)
				snake_a += SNAKE_TURN;
			if(snake_direction & 2)
				snake_a -= SNAKE_TURN;
			// movement
			snake_x += finesine[snake_a >> ANGLETOFINESHIFT];
			snake_y += finecosine[snake_a >> ANGLETOFINESHIFT];
			//
			x = snake_x >> FRACBITS;
			y = snake_y >> FRACBITS;
			// new position
			snake_position[snake_length].x = x;
			snake_position[snake_length].y = y;
			// edge check
			if(x < EDGE_DISTANCE || x >= (SCREENWIDTH - EDGE_DISTANCE) || y < EDGE_DISTANCE || y >= (SCREENHEIGHT - EDGE_DISTANCE))
			{
				// dead
				snake_direction = 0x80;
				snake_grow_count = 0;
				S_StartSound(NULL, sfx_pdiehi);
				// original position
				snake_x -= finesine[snake_a >> ANGLETOFINESHIFT];
				snake_y -= finecosine[snake_a >> ANGLETOFINESHIFT];
			} else
			// tail logic; move tail positions and check new position at once
			for(int i = 0; i < snake_length; i++)
			{
				// collision check
				if(i < snake_length - TAIL_SKIP && abs(snake_position[i].x - x) <= TAIL_DISTANCE && abs(snake_position[i].y - y) <= TAIL_DISTANCE)
				{
					// dead
					snake_direction = 0x80;
					snake_grow_count = 0;
					S_StartSound(NULL, sfx_noway);
				}
				// move
				snake_position[i] = snake_position[i+1];
			}
			// pill logic
			if(abs(pill_x - x) <= PILL_DISTANCE && abs(pill_y - y) <= PILL_DISTANCE)
			{
				snake_grow_count += SNAKE_PILL_GROW;
				if(snake_length + snake_grow_count < SNAKE_MAX_LENGTH)
				{
					S_StartSound(NULL, sfx_getpow);
					snake_random_pill();
				} else
				{
					pill_x = -1;
					S_StartSound(NULL, sfx_itmbk);
				}
			}
			// grow logic
			if(snake_grow_count)
			{
				snake_length++;
				if(snake_length == SNAKE_MAX_LENGTH)
					// win
					snake_direction = 0x88;
				snake_grow_count--;
			}
			// done
			snake_gametic = *gametic;
		}
		// draw tail
		for(int i = 0; i < snake_length; i += SNAKE_SPACING)
		{
			V_DrawPatchDirect(snake_position[i].x - 3, snake_position[i].y - 3, 0, gfx_tail);
		}
		// draw pill
		if(pill_x >= 0)
			V_DrawPatchDirect(pill_x - 3, pill_y - 3, 0, gfx_pill);
		// draw head
		V_DrawPatchDirect((snake_x >> FRACBITS) - 3, (snake_y >> FRACBITS) - 3, 0, gfx_head);
		// final snake
		if(snake_direction & 0x80)
		{
			if(snake_direction & 8)
				V_DrawPatchDirect(129, 94, 0, W_CacheLumpNum(snake_finished, PU_CACHE));
			else
				V_DrawPatchDirect(110, 70, 0, W_CacheLumpNum(snake_dead, PU_CACHE));
		}
	}
}

static void snake_reset()
{
	// restart SNAKE! game
	if(snake_direction & 0x80)
		S_StartSound(NULL, sfx_getpow);
	snake_direction = 0;
	snake_x = 160 << FRACBITS;
	snake_y = 100 << FRACBITS;
	snake_a = *gametic << 24;
	snake_grow_count = SNAKE_LENGTH;
	snake_length = 0;
	snake_random_pill();
	switch(*gametic % 6)
	{
		case 0:
			snake_head = snake_parts[0];
			snake_tail = snake_parts[1];
			snake_pill = snake_parts[2];
		break;
		case 1:
			snake_head = snake_parts[1];
			snake_tail = snake_parts[2];
			snake_pill = snake_parts[0];
		break;
		case 2:
			snake_head = snake_parts[2];
			snake_tail = snake_parts[0];
			snake_pill = snake_parts[1];
		break;
		case 3:
			snake_head = snake_parts[0];
			snake_tail = snake_parts[2];
			snake_pill = snake_parts[1];
		break;
		case 4:
			snake_head = snake_parts[1];
			snake_tail = snake_parts[0];
			snake_pill = snake_parts[2];
		break;
		case 5:
			snake_head = snake_parts[2];
			snake_tail = snake_parts[1];
			snake_pill = snake_parts[0];
		break;
	}
}

static void snake_random_pill()
{
	pill_x = (SCREENWIDTH / 2 - 128) + M_Random();
	pill_y = 12 + M_Random() % (SCREENHEIGHT - 24);
}

int __attribute((regparm(1))) snake_input(event_t *ev)
{
	// this function replaced M_Responder, which is called for every input event
	if(ev->type == ev_keydown)
	{
		switch(ev->data1)
		{
			case KEY_ESCAPE:
				// exit the SNAKE!
				S_StartSound(NULL, sfx_sgcock);
				M_StartControlPanel();
				// uninstall SNAKE! hooks
				utils_install_hooks(hook_restore);
				// free SNAKE! memory
				Z_Free(snake_position);
				Z_Free((uint8_t*)ace_segment + 0x1000);
				// done
				return 1;
			case KEY_ENTER:
				snake_reset();
			break;
			case KEY_LEFTARROW:
				snake_direction |= 1;
			break;
			case KEY_RIGHTARROW:
				snake_direction |= 2;
			break;
			case KEY_CTRL:
				snake_grow_count = 16;
			break;
		}
	} else
	if(ev->type == ev_keyup)
	{
		switch(ev->data1)
		{
			case KEY_LEFTARROW:
				snake_direction &= ~1;
			break;
			case KEY_RIGHTARROW:
				snake_direction &= ~2;
			break;
		}
	}

	// always 'eat' the key
	return 1;
}

