// default actor names as named in ZDoom
static uint8_t doom_actor_names[] =
{
	"\x0A""DoomPlayer" \
	"\x09""ZombieMan" \
	"\x0A""ShotgunGuy" \
	"\x08""Archvile" \
	"\x0C""ArchvileFire" \
	"\x08""Revenant" \
	"\x0E""RevenantTracer" \
	"\x13""RevenantTracerSmoke" \
	"\x05""Fatso" \
	"\x07""FatShot" \
	"\x0B""ChaingunGuy" \
	"\x07""DoomImp" \
	"\x05""Demon" \
	"\x07""Spectre" \
	"\x09""Cacodemon" \
	"\x0B""BaronOfHell" \
	"\x09""BaronBall" \
	"\x0A""HellKnight" \
	"\x08""LostSoul" \
	"\x10""SpiderMastermind" \
	"\x0B""Arachnotron" \
	"\x0A""Cyberdemon" \
	"\x0D""PainElemental" \
	"\x0D""WolfensteinSS" \
	"\x0D""CommanderKeen" \
	"\x09""BossBrain" \
	"\x07""BossEye" \
	"\x0A""BossTarget" \
	"\x09""SpawnShot" \
	"\x09""SpawnFire" \
	"\x0F""ExplosiveBarrel" \
	"\x0B""DoomImpBall" \
	"\x0D""CacodemonBall" \
	"\x06""Rocket" \
	"\x0A""PlasmaBall" \
	"\x07""BFGBall" \
	"\x11""ArachnotronPlasma" \
	"\x0A""BulletPuff" \
	"\x05""Blood" \
	"\x0B""TeleportFog" \
	"\x07""ItemFog" \
	"\x0C""TeleportDest" \
	"\x08""BFGExtra" \
	"\x0A""GreenArmor" \
	"\x09""BlueArmor" \
	"\x0B""HealthBonus" \
	"\x0A""ArmorBonus" \
	"\x08""BlueCard" \
	"\x07""RedCard" \
	"\x0A""YellowCard" \
	"\x0B""YellowSkull" \
	"\x08""RedSkull" \
	"\x09""BlueSkull" \
	"\x08""Stimpack" \
	"\x07""Medikit" \
	"\x0A""Soulsphere" \
	"\x15""InvulnerabilitySphere" \
	"\x07""Berserk" \
	"\x0A""BlurSphere" \
	"\x07""RadSuit" \
	"\x06""Allmap" \
	"\x08""Infrared" \
	"\x0A""Megasphere" \
	"\x04""Clip" \
	"\x0C""BoxOfBullets" \
	"\x0A""RocketAmmo" \
	"\x09""RocketBox" \
	"\x04""Cell" \
	"\x08""CellPack" \
	"\x05""Shell" \
	"\x08""ShellBox" \
	"\x08""Backpack" \
	"\x07""BFG9000" \
	"\x08""Chaingun" \
	"\x08""Chainsaw" \
	"\x0E""RocketLauncher" \
	"\x0B""PlasmaRifle" \
	"\x07""Shotgun" \
	"\x0C""SuperShotgun" \
	"\x08""TechLamp" \
	"\x09""TechLamp2" \
	"\x06""Column" \
	"\x0F""TallGreenColumn" \
	"\x10""ShortGreenColumn" \
	"\x0D""TallRedColumn" \
	"\x0E""ShortRedColumn" \
	"\x0B""SkullColumn" \
	"\x0B""HeartColumn" \
	"\x07""EvilEye" \
	"\x0D""FloatingSkull" \
	"\x09""TorchTree" \
	"\x09""BlueTorch" \
	"\x0A""GreenTorch" \
	"\x08""RedTorch" \
	"\x0E""ShortBlueTorch" \
	"\x0F""ShortGreenTorch" \
	"\x0D""ShortRedTorch" \
	"\x0A""Stalagtite" \
	"\x0A""TechPillar" \
	"\x0B""Candlestick" \
	"\x0A""Candelabra" \
	"\x0C""BloodyTwitch" \
	"\x05""Meat2" \
	"\x05""Meat3" \
	"\x05""Meat4" \
	"\x05""Meat5" \
	"\x0D""NonsolidMeat2" \
	"\x0D""NonsolidMeat4" \
	"\x0D""NonsolidMeat3" \
	"\x0D""NonsolidMeat5" \
	"\x0E""NonsolidTwitch" \
	"\x0D""DeadCacodemon" \
	"\x0A""DeadMarine" \
	"\x0D""DeadZombieMan" \
	"\x09""DeadDemon" \
	"\x0C""DeadLostSoul" \
	"\x0B""DeadDoomImp" \
	"\x0E""DeadShotgunGuy" \
	"\x0C""GibbedMarine" \
	"\x11""GibbedMarineExtra" \
	"\x0D""HeadsOnAStick" \
	"\x04""Gibs" \
	"\x0C""HeadOnAStick" \
	"\x0B""HeadCandles" \
	"\x09""DeadStick" \
	"\x09""LiveStick" \
	"\x07""BigTree" \
	"\x0D""BurningBarrel" \
	"\x0A""HangNoGuts" \
	"\x0C""HangBNoBrain" \
	"\x10""HangTLookingDown" \
	"\x0A""HangTSkull" \
	"\x0E""HangTLookingUp" \
	"\x0C""HangTNoBrain" \
	"\x09""ColonGibs" \
	"\x0E""SmallBloodPool" \
	"\x09""BrainStem"
};

static uint8_t doom_spawn_id[] =
{
	0,
	4,
	1,
	111,
	98,
	20,
	53,
	0,
	112,
	153,
	2,
	5,
	8,
	9,
	19,
	3,
	154,
	113,
	110,
	7,
	6,
	114,
	115,
	116,
	0,
	0,
	0,
	0,
	0,
	0,
	125,
	10,
	126,
	127,
	51,
	128,
	129,
	131,
	130,
	0,
	0,
	0,
	0,
	68,
	69,
	152,
	22,
	85,
	86,
	87,
	88,
	89,
	90,
	23,
	24,
	25,
	133,
	134,
	135,
	136,
	137,
	138,
	132,
	11,
	139,
	140,
	141,
	75,
	142,
	12,
	143,
	144,
	31,
	28,
	32,
	29,
	30,
	27,
	33,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	145,
	0,
	0,
	146,
	0,
	0,
	0,
	0,
	0,
	149,
	0,
	0,
	0,
	0,
	0,
	0,
	147,
	148,
	150
};

