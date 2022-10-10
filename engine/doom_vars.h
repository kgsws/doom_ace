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
extern uint8_t *screen_buffer;

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

// automap
extern uint32_t automapactive;
extern uint32_t am_lastlevel;

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
extern uint32_t numsides;
extern uint32_t numlines;
extern uint32_t numsectors;
extern line_t *lines;
extern vertex_t *vertexes;
extern side_t *sides;
extern sector_t *sectors;
extern subsector_t *subsectors;
extern seg_t *segs;
extern fixed_t bmaporgx;
extern fixed_t bmaporgy;

// map & maputl
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

// player
extern uint32_t playeringame[];
extern player_t players[];
extern uint32_t consoleplayer;
extern uint32_t displayplayer;

// things
extern thinker_t thinkercap;

// render, textures
extern uint32_t skytexture;
extern uint32_t skyflatnum;

// render, view
extern fixed_t viewx;
extern fixed_t viewy;
extern fixed_t viewz;
extern angle_t viewangle;
extern int32_t extralight;
extern uint8_t *fixedcolormap;

// render, screen
extern fixed_t centeryfrac;
extern int32_t centerx;
extern int32_t centery;
extern int32_t viewwindowx;
extern int32_t viewwindowy;
extern fixed_t yslope[];
extern uint32_t viewheight;
extern uint32_t viewwidth;
extern uint32_t screenblocks;
extern uint32_t usegamma;

// render, status
extern sector_t *frontsector;
extern sector_t *backsector;
extern seg_t *curline;
extern drawseg_t *ds_p;
extern visplane_t *ceilingplane;
extern visplane_t *floorplane;
extern uint32_t markceiling;
extern uint32_t markfloor;
extern angle_t rw_normalangle;
extern angle_t rw_angle1;
extern fixed_t rw_distance;

// render, map
extern cliprange_t solidsegs[];
extern vissprite_t *vissprite_p;
extern vissprite_t vsprsortedhead;

// render, tables
extern angle_t xtoviewangle[];
extern int16_t floorclip[];
extern int16_t ceilingclip[];

// render, light
extern fixed_t planeheight;
extern uint8_t **planezlight;
extern uint8_t *scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
extern uint8_t *zlight[LIGHTLEVELS][MAXLIGHTZ];

// render, draw
extern uint32_t dc_x;
extern uint32_t dc_yl;
extern uint32_t dc_yh;
extern uint8_t *dc_source;
extern fixed_t dc_texturemid;
extern fixed_t dc_iscale;
extern uint8_t *ds_source;
extern uint8_t *dc_colormap;
extern fixed_t sprtopscreen;
extern fixed_t spryscale;
extern int16_t *mfloorclip;
extern int16_t *mceilingclip;
extern void (*colfunc)();

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

// random
extern uint32_t prndindex;

// fi, wi
extern wbstartstruct_t wminfo;
extern int32_t finaleflat; // type is changed!
extern uint8_t *finaletext;
extern uint32_t finalecount;
extern uint32_t finalestage;

// original map info
extern uint8_t *mapnames[];
extern uint8_t *mapnames2[];
extern uint32_t pars[];
extern uint32_t cpars[];

