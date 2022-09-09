// kgsws' ACE Engine
////

#define MAX_CHEAT_BUFFER	47
#define CHAT_CHEAT_MARKER	0xC7

typedef struct
{
	uint8_t text[MAX_CHEAT_BUFFER];
	int8_t len;
} cheat_buf_t;

//

extern cheat_buf_t *cheat_buf;

//

uint32_t cheat_check(uint32_t pidx);
void cheat_player_flags(player_t *pl);

