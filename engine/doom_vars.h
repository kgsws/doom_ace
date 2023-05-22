// kgsws' ACE Engine
////

// tables
extern fixed_t finetangent[];
extern fixed_t finesine[];
extern fixed_t finecosine[];
extern angle_t tantoangle[];

// arrays
extern mapthing_t playerstarts[];
extern mapthing_t deathmatchstarts[];
extern mapthing_t *deathmatch_p;

// stuff
extern uint32_t doom_stdin[];
extern uint32_t doom_stdout[];
extern heap_base_t heap_base;
extern memzone_t *mainzone;
extern uint8_t *screen_buffer;
extern uint8_t *am_fb;
extern uint32_t message_is_important;
extern uint32_t french_version;
extern int32_t myargc;
extern uint8_t **myargv;
extern int_regs_t x86regs;
extern dpmi_regs_t dpmiregs;

// netgame
extern doomcom_t *doomcom;
extern doomdata_t *netbuffer;

// events
extern event_t events[];
extern uint32_t eventtail;
extern uint32_t eventhead;

// game state
extern uint32_t wipegamestate;
extern uint32_t gametic;
extern uint32_t gamemode;
extern uint32_t gamemode_sw;
extern uint32_t gamemode_reg;
extern uint32_t gamestate;
extern uint32_t gameaction;
extern uint32_t paused;
extern uint32_t menuactive;
extern uint32_t demosequence;
extern uint32_t advancedemo;
extern uint32_t netgame;
extern uint32_t usergame;
extern uint32_t viewactive;
extern uint32_t d_skill;
extern uint32_t startskill;
extern uint32_t startepisode;
extern uint32_t startmap;
extern uint32_t autostart;

// automap
extern uint32_t automapactive;
extern uint32_t am_lastlevel;
extern uint32_t am_cheating;

// menu
extern uint32_t messageToPrint;
extern uint8_t *messageString;
extern uint16_t menu_item_now; // was itemOn
extern menu_t *currentMenu;
extern uint32_t showMessages;
extern uint32_t mouseSensitivity;
extern menu_t NewDef;
extern menu_t EpiDef;
extern menu_t LoadDef;
extern menuitem_t LoadMenu[];

// HUD
extern patch_t *hu_font[];
extern uint32_t hu_char_head;
extern uint32_t hu_char_tail;
extern cheat_buf_t cheat_buf[]; // was w_inputbuffer, 468 bytes

// save / load
extern uint8_t savename[];
extern uint8_t savedesc[];
extern int32_t saveslot;

// game play
extern uint32_t nomonsters;
extern uint32_t fastparm;
extern uint32_t respawnparm;
extern uint32_t deathmatch;
extern uint32_t gamemap;
extern uint32_t gameepisode;
extern uint32_t gameskill;
extern uint32_t leveltime;
extern uint32_t totalkills;
extern uint32_t totalitems;
extern uint32_t totalsecret;
extern uint32_t secretexit;
extern uint32_t respawnmonsters;

// game level
extern uint32_t numvertexes;
extern uint32_t numsides;
extern uint32_t numlines;
extern uint32_t numsectors;
extern uint32_t numsubsectors;
extern uint32_t numsegs;
extern uint32_t numnodes;
extern uint8_t *rejectmatrix;
extern line_t *lines;
extern vertex_t *vertexes;
extern side_t *sides;
extern sector_t *sectors;
extern subsector_t *subsectors;
extern seg_t *segs;
extern node_t *nodes;
extern fixed_t bmaporgx;
extern fixed_t bmaporgy;
extern int32_t bmapwidth;
extern int32_t bmapheight;
extern uint16_t *blockmaplump;
extern uint16_t *blockmap;
extern mobj_t **blocklinks;
extern uint32_t bodyqueslot;

// map & maputl
extern mobj_t *tmthing;
extern uint32_t tmflags;
extern fixed_t tmdropoffz;
extern fixed_t tmfloorz;
extern fixed_t tmceilingz;
extern fixed_t lowfloor;
extern fixed_t openrange;
extern fixed_t opentop;
extern fixed_t openbottom;
extern uint32_t nofit;
extern uint32_t crushchange;
extern mobj_t *linetarget;
extern line_t *spechit[];
extern uint32_t numspechit;
extern uint32_t validcount;
extern intercept_t intercepts[];
extern intercept_t *intercept_p;
extern divline_t trace;
extern fixed_t bestslidefrac;
extern line_t *bestslideline;
extern mobj_t *slidemo;
extern fixed_t shootz;
extern fixed_t aimslope;
extern uint32_t la_damage;
extern fixed_t bulletslope;
extern fixed_t botslope; // was bottomslope
extern fixed_t topslope;
extern mobj_t *shootthing;
extern fixed_t attackrange;
extern fixed_t tmbbox[4];
extern mobj_t *bombspot;
extern mobj_t *bombsource;
extern uint32_t bombdamage;
extern mobj_t *corpsehit;
extern mobj_t *vileobj;
extern fixed_t viletryx;
extern fixed_t viletryy;
extern fixed_t tmymove;
extern fixed_t tmxmove;

// player
extern uint32_t playeringame[];
extern player_t players[];
extern uint32_t consoleplayer;
extern uint32_t displayplayer;
extern uint32_t onground;

// sprite
extern fixed_t *spritewidth;
extern fixed_t *spriteoffset;
extern fixed_t *spritetopoffset;
extern int32_t spr_maxframe;
extern spriteframe_t sprtemp[];
extern spritedef_t *sprites;
extern uint32_t *spr_names[];
extern uint32_t numsprites;

// texture
extern uint32_t numtextures;
extern texture_t **textures;
extern uint16_t **texturecolumnlump;
extern uint16_t **texturecolumnofs;
extern uint8_t **texturecomposite;
extern uint32_t *texturecompositesize;
extern uint32_t *texturewidthmask;
extern fixed_t *textureheight;
extern uint32_t *texturetranslation;

// flat
extern uint32_t numflats;
extern uint16_t *flattranslation; // this was uint32_t

// things
extern thinker_t thinkercap;

// render
extern uint32_t r_setblocks;
extern uint32_t r_setdetail;
extern uint8_t *r_rdptr;
extern uint8_t *r_fbptr;

// render, textures
extern uint32_t skytexture;
extern uint32_t skyflatnum;

// render, view
extern player_t *viewplayer;
extern fixed_t viewx;
extern fixed_t viewy;
extern fixed_t viewz;
extern fixed_t viewsin;
extern fixed_t viewcos;
extern angle_t viewangle;
extern int32_t extralight;
extern uint8_t *fixedcolormap;

// render, screen
extern fixed_t centerxfrac;
extern fixed_t centeryfrac;
extern int32_t centerx;
extern int32_t centery;
extern fixed_t projection;
extern int32_t viewwindowx;
extern int32_t viewwindowy;
extern fixed_t yslope[];
extern int32_t viewheight;
extern int32_t viewwidth;
extern uint32_t screenblocks;
extern uint32_t usegamma;
extern uint32_t detaillevel;

// render, status
extern visplane_t *lastvisplane;
extern sector_t *frontsector;
extern sector_t *backsector;
extern seg_t *curline;
extern drawseg_t *ds_p;
extern visplane_t *ceilingplane;
extern visplane_t *floorplane;
extern line_t *linedef;
extern side_t *sidedef;
extern int16_t *maskedtexturecol;
extern uint32_t markceiling;
extern uint32_t markfloor;
extern uint32_t segtextured;
extern uint32_t maskedtexture;
extern uint32_t midtexture;
extern uint32_t bottomtexture;
extern uint32_t toptexture;
extern angle_t rw_normalangle;
extern angle_t rw_angle1;
extern fixed_t rw_distance;
extern fixed_t rw_x;
extern fixed_t rw_stopx;
extern fixed_t bottomstep;
extern fixed_t topstep;
extern fixed_t bottomfrac;
extern fixed_t topfrac;
extern fixed_t rw_scalestep;
extern fixed_t rw_scale;
extern fixed_t rw_centerangle;
extern fixed_t rw_offset;
extern fixed_t rw_toptexturemid;
extern fixed_t rw_midtexturemid;
extern fixed_t rw_bottomtexturemid;
extern fixed_t pixlow;
extern fixed_t pixlowstep;
extern fixed_t pixhigh;
extern fixed_t pixhighstep;

// render, map
extern cliprange_t solidsegs[];
extern vissprite_t *vissprite_p;
extern vissprite_t vsprsortedhead;

// render, tables
extern angle_t xtoviewangle[];
extern int16_t floorclip[];
extern int16_t ceilingclip[];
extern int16_t negonearray[];
extern int16_t screenheightarray[];

// render, light
extern uint8_t *colormaps;
extern fixed_t planeheight;
extern int32_t planezlight; // this was uint8_t**
extern uint32_t fuzzpos;

// render, draw
extern int32_t dc_x;
extern int32_t dc_yl;
extern int32_t dc_yh;
extern uint8_t *dc_source;
extern fixed_t dc_texturemid;
extern fixed_t dc_iscale;
extern uint8_t *dc_colormap;
extern uint8_t *dc_translation;
extern fixed_t ds_xfrac;
extern fixed_t ds_yfrac;
extern fixed_t ds_ystep;
extern fixed_t ds_xstep;
extern int32_t ds_x1;
extern int32_t ds_x2;
extern int32_t ds_y;
extern uint8_t *ds_colormap;
extern uint8_t *ds_source;
extern fixed_t sprtopscreen;
extern fixed_t spryscale;
extern fixed_t pspriteiscale;
extern fixed_t skytexturemid;
extern int16_t *mfloorclip;
extern int16_t *mceilingclip;
extern void (*colfunc)() __attribute((regparm(2),no_caller_saved_registers));
extern void (*spanfunc)() __attribute((regparm(2),no_caller_saved_registers));

// render, location
extern uint32_t columnofs[SCREENWIDTH];
extern uint8_t *ylookup[SCREENWIDTH];

// controls
extern int32_t mousey;

// special
extern plat_t *activeplats[];
extern ceiling_t *activeceilings[];

// demo
extern uint32_t netdemo;
extern uint32_t demoplayback;
extern uint32_t demorecording;
extern uint8_t *defdemoname;
extern uint32_t singledemo;

// sound
extern old_sfxinfo_t S_sfx[];
extern musicinfo_t S_music[];
extern musicinfo_t *music_now;
extern int32_t volume_val; // unknown name

// wad
extern uint8_t *wadfiles[MAXWADFILES];
extern lumpinfo_t *lumpinfo;
extern void **lumpcache;
extern uint32_t numlumps;

// gfx
extern uint32_t lu_palette;
extern uint32_t st_palette;
extern patch_t *tallnum[];
extern patch_t *tallpercent;
extern patch_t *shortnum[];
extern st_number_t w_ready;
extern st_number_t w_ammo[];
extern st_number_t w_maxammo[];
extern st_multicon_t w_arms[];
extern int32_t keyboxes[];

// controls
extern uint32_t gamekeydown[256];
extern uint32_t mousebuttons[3];
extern int32_t mouseb_fire;
extern int32_t mouseb_strafe;
extern int32_t mouseb_forward;
extern int32_t joyb_speed;
extern int32_t joyb_use;
extern int32_t joyb_strafe;
extern int32_t joyb_fire;
extern uint32_t key_up;
extern uint32_t key_down;
extern uint32_t key_strafeleft;
extern uint32_t key_straferight;
extern uint32_t key_left;
extern uint32_t key_right;
extern uint32_t key_fire;
extern uint32_t key_use;
extern uint32_t key_speed;
extern uint32_t key_strafe;

// config
extern uint32_t usemouse;
extern uint32_t usejoystick;
extern uint32_t numChannels;
extern uint32_t snd_sfxdevice;
extern uint32_t snd_musicdevice;
extern uint32_t snd_sbport;
extern uint32_t snd_sbdma;
extern uint32_t snd_mport;
extern uint32_t snd_sbirq;
extern uint32_t snd_MusicVolume;
extern uint32_t snd_SfxVolume;

// random
extern uint8_t rndtable[256];
extern uint32_t prndindex;
extern uint8_t rng_table[128*16*4]; // was zlight

// fi, wi
extern wbstartstruct_t wminfo;
extern int32_t finaleflat; // type is changed!
extern uint8_t *finaletext;
extern int32_t finalecount;
extern int32_t finalestage;

// original map info
extern uint8_t *mapnames[];
extern uint8_t *mapnames2[];
extern uint32_t pars[];
extern uint32_t cpars[];

// original info
extern uint32_t maxammo[];
extern uint32_t clipammo[];
extern deh_mobjinfo_t deh_mobjinfo[];
extern deh_state_t deh_states[];
extern weaponinfo_t deh_weaponinfo[];
extern drawseg_t d_drawsegs[];
extern vissprite_t d_vissprites[];
extern visplane_t d_visplanes[];

// strings
extern uint8_t dtxt_pnames[];
extern uint8_t dtxt_texture1[];
extern uint8_t dtxt_texture2[];
extern uint8_t dtxt_colormap[];
extern uint8_t dtxt_stcfn[];
extern uint64_t dtxt_skull_name[];
extern uint8_t dtxt_m_option[];
extern uint8_t dtxt_m_lscntr[];
extern uint8_t dtxt_m_lsleft[];
extern uint8_t dtxt_m_lsrght[];
extern uint8_t dtxt_m_episod[];
extern uint8_t dtxt_PD_BLUEO[];
extern uint8_t dtxt_PD_REDO[];
extern uint8_t dtxt_PD_YELLOWO[];
extern uint8_t dtxt_PD_BLUEK[];
extern uint8_t dtxt_PD_YELLOWK[];
extern uint8_t dtxt_PD_REDK[];
extern uint8_t dtxt_mus_pfx[];
extern uint8_t dtxt_help2[];
extern uint8_t dtxt_victory2[];
extern uint8_t dtxt_wif[];
extern uint8_t dtxt_wienter[];
extern uint8_t dtxt_dm2int[];
extern uint8_t dtxt_read_m[];
extern uint8_t dtxt_inter[];
extern uint8_t dtxt_victor[];
extern uint8_t dtxt_m_epi1[];
extern uint8_t dtxt_m_epi2[];
extern uint8_t dtxt_m_epi3[];
extern uint8_t dtxt_interpic[];
extern uint8_t dtxt_E1TEXT[];
extern uint8_t dtxt_E2TEXT[];
extern uint8_t dtxt_E3TEXT[];
extern uint8_t dtxt_C1TEXT[];
extern uint8_t dtxt_C2TEXT[];
extern uint8_t dtxt_C3TEXT[];
extern uint8_t dtxt_C4TEXT[];
extern uint8_t dtxt_C5TEXT[];
extern uint8_t dtxt_C6TEXT[];
extern uint8_t dtxt_slime16[];
extern uint8_t dtxt_rrock14[];
extern uint8_t dtxt_rrock07[];
extern uint8_t dtxt_rrock17[];
extern uint8_t dtxt_rrock13[];
extern uint8_t dtxt_rrock19[];
extern uint8_t dtxt_floor4_8[];
extern uint8_t dtxt_sflr6_1[];
extern uint8_t dtxt_mflr8_4[];
extern uint8_t dtxt_STSTR_DQDON[];
extern uint8_t dtxt_STSTR_DQDOFF[];
extern uint8_t dtxt_STSTR_FAADDED[];
extern uint8_t dtxt_STSTR_KFAADDED[];
extern uint8_t dtxt_STSTR_NCON[];
extern uint8_t dtxt_STSTR_NCOFF[];
extern uint8_t dtxt_m_loadg[];
extern uint8_t dtxt_m_saveg[];

