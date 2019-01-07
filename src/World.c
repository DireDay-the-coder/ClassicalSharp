#include "World.h"
#include "Logger.h"
#include "String.h"
#include "Platform.h"
#include "Event.h"
#include "Block.h"
#include "Entity.h"
#include "ExtMath.h"
#include "Physics.h"
#include "Game.h"

BlockRaw* World_Blocks;
#ifdef EXTENDED_BLOCKS
BlockRaw* World_Blocks2;
#endif
int World_BlocksSize;

int World_Width, World_Height, World_Length;
int World_MaxX, World_MaxY, World_MaxZ;
int World_OneY;
uint8_t World_Uuid[16];

/*########################################################################################################################*
*----------------------------------------------------------World----------------------------------------------------------*
*#########################################################################################################################*/
static void World_NewUuid(void) {
	RNGState rnd;
	int i;
	Random_InitFromCurrentTime(&rnd);

	/* seed a bit more randomness for uuid */
	for (i = 0; i < Game_Username.length; i++) {
		Random_Next(&rnd, Game_Username.buffer[i] + 3);
	}

	for (i = 0; i < 16; i++) {
		World_Uuid[i] = Random_Next(&rnd, 256);
	}

	/* Set version and variant bits */
	World_Uuid[6] &= 0x0F;
	World_Uuid[6] |= 0x40; /* version 4*/
	World_Uuid[8] &= 0x3F;
	World_Uuid[8] |= 0x80; /* variant 2*/
}

void World_Reset(void) {
#ifdef EXTENDED_BLOCKS
	if (World_Blocks != World_Blocks2) Mem_Free(World_Blocks2);
#endif
	Mem_Free(World_Blocks);
	World_Width = 0; World_Height = 0; World_Length = 0;
	World_MaxX = 0;  World_MaxY = 0;   World_MaxZ = 0;

#ifdef EXTENDED_BLOCKS
	World_Blocks2 = NULL;
#endif
	World_Blocks = NULL; World_BlocksSize = 0;
	Env_Reset();
	World_NewUuid();
}

void World_SetNewMap(BlockRaw* blocks, int blocksSize, int width, int height, int length) {
	World_Width = width; World_Height = height; World_Length = length;
	World_Blocks = blocks; World_BlocksSize = blocksSize;
	if (!World_BlocksSize) World_Blocks = NULL;

	if (blocksSize != (width * height * length)) {
		Logger_Abort("Blocks array size does not match volume of map");
	}
#ifdef EXTENDED_BLOCKS
	World_Blocks2 = World_Blocks;
#endif

	World_OneY = width * length;
	World_MaxX = width  - 1;
	World_MaxY = height - 1;
	World_MaxZ = length - 1;

	if (Env_EdgeHeight == -1) {
		Env_EdgeHeight = height / 2;
	}
	if (Env_CloudsHeight == -1) {
		Env_CloudsHeight = height + 2;
	}
}


#ifdef EXTENDED_BLOCKS
void World_SetBlock(int x, int y, int z, BlockID block) {
	int i = World_Pack(x, y, z);
	World_Blocks[i] = (BlockRaw)block;

	/* defer allocation of second map array if possible */
	if (World_Blocks == World_Blocks2) {
		if (block < 256) return;
		World_Blocks2 = Mem_AllocCleared(World_BlocksSize, 1, "blocks array upper");
		Block_SetUsedCount(768);
	}
	World_Blocks2[i] = (BlockRaw)(block >> 8);
}
#else
void World_SetBlock(int x, int y, int z, BlockID block) {
	World_Blocks[World_Pack(x, y, z)] = block; 
}
#endif

BlockID World_GetPhysicsBlock(int x, int y, int z) {
	if (x < 0 || x >= World_Width || z < 0 || z >= World_Length || y < 0) return BLOCK_BEDROCK;
	if (y >= World_Height) return BLOCK_AIR;

	return World_GetBlock(x, y, z);
}

BlockID World_SafeGetBlock_3I(Vector3I p) {
	return World_IsValidPos(p.X, p.Y, p.Z) ? World_GetBlock(p.X, p.Y, p.Z) : BLOCK_AIR;
}

bool World_IsValidPos(int x, int y, int z) {
	return x >= 0 && y >= 0 && z >= 0 && 
		x < World_Width && y < World_Height && z < World_Length;
}

bool World_IsValidPos_3I(Vector3I p) {
	return p.X >= 0 && p.Y >= 0 && p.Z >= 0 &&
		p.X < World_Width && p.Y < World_Height && p.Z < World_Length;
}


/*########################################################################################################################*
*-------------------------------------------------------Environment-------------------------------------------------------*
*#########################################################################################################################*/
#define Env_Set(src, dst, var) \
if (src != dst) { dst = src; Event_RaiseInt(&WorldEvents.EnvVarChanged, var); }

#define Env_SetCol(src, dst, var)\
if (!PackedCol_Equals(src, dst)) { dst = src; Event_RaiseInt(&WorldEvents.EnvVarChanged, var); }

const char* Weather_Names[3] = { "Sunny", "Rainy", "Snowy" };

PackedCol Env_DefaultSkyCol    = PACKEDCOL_CONST(0x99, 0xCC, 0xFF, 0xFF);
PackedCol Env_DefaultFogCol    = PACKEDCOL_CONST(0xFF, 0xFF, 0xFF, 0xFF);
PackedCol Env_DefaultCloudsCol = PACKEDCOL_CONST(0xFF, 0xFF, 0xFF, 0xFF);
PackedCol Env_DefaultSunCol    = PACKEDCOL_CONST(0xFF, 0xFF, 0xFF, 0xFF);
PackedCol Env_DefaultShadowCol = PACKEDCOL_CONST(0x9B, 0x9B, 0x9B, 0xFF);

BlockID Env_EdgeBlock, Env_SidesBlock;
int Env_EdgeHeight, Env_SidesOffset, Env_CloudsHeight; 
float Env_CloudsSpeed, Env_WeatherSpeed, Env_WeatherFade;
int Env_Weather; bool Env_ExpFog;
float Env_SkyboxHorSpeed, Env_SkyboxVerSpeed;

PackedCol Env_SkyCol, Env_FogCol, Env_CloudsCol;
PackedCol Env_SunCol,    Env_SunXSide,    Env_SunZSide,    Env_SunYMin;
PackedCol Env_ShadowCol, Env_ShadowXSide, Env_ShadowZSide, Env_ShadowYMin;

static char World_TextureUrlBuffer[STRING_SIZE];
String World_TextureUrl = String_FromArray(World_TextureUrlBuffer);

void Env_Reset(void) {
	Env_EdgeHeight   = -1;
	Env_SidesOffset  = -2;
	Env_CloudsHeight = -1;

	Env_EdgeBlock  = BLOCK_STILL_WATER;
	Env_SidesBlock = BLOCK_BEDROCK;

	Env_CloudsSpeed    = 1.0f;
	Env_WeatherSpeed   = 1.0f;
	Env_WeatherFade    = 1.0f;
	Env_SkyboxHorSpeed = 0.0f;
	Env_SkyboxVerSpeed = 0.0f;

	Env_ShadowCol = Env_DefaultShadowCol;
	PackedCol_GetShaded(Env_ShadowCol, &Env_ShadowXSide,
		&Env_ShadowZSide, &Env_ShadowYMin);

	Env_SunCol = Env_DefaultSunCol;
	PackedCol_GetShaded(Env_SunCol, &Env_SunXSide,
		&Env_SunZSide, &Env_SunYMin);

	Env_SkyCol    = Env_DefaultSkyCol;
	Env_FogCol    = Env_DefaultFogCol;
	Env_CloudsCol = Env_DefaultCloudsCol;
	Env_Weather = WEATHER_SUNNY;
	Env_ExpFog = false;
}


void Env_SetEdgeBlock(BlockID block) {
	/* some server software wrongly uses this value */
	if (block == 255 && !Block_IsCustomDefined(255)) block = BLOCK_STILL_WATER; 
	Env_Set(block, Env_EdgeBlock, ENV_VAR_EDGE_BLOCK);
}
void Env_SetSidesBlock(BlockID block) {
	/* some server software wrongly uses this value */
	if (block == 255 && !Block_IsCustomDefined(255)) block = BLOCK_BEDROCK;
	Env_Set(block, Env_SidesBlock, ENV_VAR_SIDES_BLOCK);
}

void Env_SetEdgeHeight(int height) {
	Env_Set(height, Env_EdgeHeight, ENV_VAR_EDGE_HEIGHT);
}
void Env_SetSidesOffset(int offset) {
	Env_Set(offset, Env_SidesOffset, ENV_VAR_SIDES_OFFSET);
}
void Env_SetCloudsHeight(int height) {
	Env_Set(height, Env_CloudsHeight, ENV_VAR_CLOUDS_HEIGHT);
}
void Env_SetCloudsSpeed(float speed) {
	Env_Set(speed, Env_CloudsSpeed, ENV_VAR_CLOUDS_SPEED);
}

void Env_SetWeatherSpeed(float speed) {
	Env_Set(speed, Env_WeatherSpeed, ENV_VAR_WEATHER_SPEED);
}
void Env_SetWeatherFade(float rate) {
	Env_Set(rate, Env_WeatherFade, ENV_VAR_WEATHER_FADE);
}
void Env_SetWeather(int weather) {
	Env_Set(weather, Env_Weather, ENV_VAR_WEATHER);
}
void Env_SetExpFog(bool expFog) {
	Env_Set(expFog, Env_ExpFog, ENV_VAR_EXP_FOG);
}
void Env_SetSkyboxHorSpeed(float speed) {
	Env_Set(speed, Env_SkyboxHorSpeed, ENV_VAR_SKYBOX_HOR_SPEED);
}
void Env_SetSkyboxVerSpeed(float speed) {
	Env_Set(speed, Env_SkyboxVerSpeed, ENV_VAR_SKYBOX_VER_SPEED);
}

void Env_SetSkyCol(PackedCol col) {
	Env_SetCol(col, Env_SkyCol, ENV_VAR_SKY_COL);
}
void Env_SetFogCol(PackedCol col) {
	Env_SetCol(col, Env_FogCol, ENV_VAR_FOG_COL);
}
void Env_SetCloudsCol(PackedCol col) {
	Env_SetCol(col, Env_CloudsCol, ENV_VAR_CLOUDS_COL);
}

void Env_SetSunCol(PackedCol col) {
	PackedCol_GetShaded(col, &Env_SunXSide, &Env_SunZSide, &Env_SunYMin);
	Env_SetCol(col, Env_SunCol, ENV_VAR_SUN_COL);
}
void Env_SetShadowCol(PackedCol col) {
	PackedCol_GetShaded(col, &Env_ShadowXSide, &Env_ShadowZSide, &Env_ShadowYMin);
	Env_SetCol(col, Env_ShadowCol, ENV_VAR_SHADOW_COL);
}


/*########################################################################################################################*
*-------------------------------------------------------Respawning--------------------------------------------------------*
*#########################################################################################################################*/
float Respawn_HighestSolidY(struct AABB* bb) {
	int minX = Math_Floor(bb->Min.X), maxX = Math_Floor(bb->Max.X);
	int minY = Math_Floor(bb->Min.Y), maxY = Math_Floor(bb->Max.Y);
	int minZ = Math_Floor(bb->Min.Z), maxZ = Math_Floor(bb->Max.Z);
	float highestY = RESPAWN_NOT_FOUND;

	BlockID block;
	struct AABB blockBB;
	Vector3 v;
	int x, y, z;	

	for (y = minY; y <= maxY; y++) { v.Y = (float)y;
		for (z = minZ; z <= maxZ; z++) { v.Z = (float)z;
			for (x = minX; x <= maxX; x++) { v.X = (float)x;

				block = World_GetPhysicsBlock(x, y, z);
				Vector3_Add(&blockBB.Min, &v, &Blocks.MinBB[block]);
				Vector3_Add(&blockBB.Max, &v, &Blocks.MaxBB[block]);

				if (Blocks.Collide[block] != COLLIDE_SOLID) continue;
				if (!AABB_Intersects(bb, &blockBB)) continue;
				if (blockBB.Max.Y > highestY) highestY = blockBB.Max.Y;
			}
		}
	}
	return highestY;
}

Vector3 Respawn_FindSpawnPosition(float x, float z, Vector3 modelSize) {
	Vector3 spawn = Vector3_Create3(x, 0.0f, z);
	struct AABB bb;
	float highestY;
	int y;

	spawn.Y = World_Height + ENTITY_ADJUSTMENT;
	AABB_Make(&bb, &spawn, &modelSize);
	spawn.Y = 0.0f;
	
	for (y = World_Height; y >= 0; y--) {
		highestY = Respawn_HighestSolidY(&bb);
		if (highestY != RESPAWN_NOT_FOUND) {
			spawn.Y = highestY; break;
		}
		bb.Min.Y -= 1.0f; bb.Max.Y -= 1.0f;
	}
	return spawn;
}
