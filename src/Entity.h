#ifndef CC_ENTITY_H
#define CC_ENTITY_H
#include "EntityComponents.h"
#include "Physics.h"
#include "Constants.h"
#include "Input.h"
#include "PackedCol.h"
/* Represents an in-game entity.
   Copyright 2014-2017 ClassicalSharp | Licensed under BSD-3
*/
struct Model;
struct IGameComponent;
struct ScheduledTask;
extern struct IGameComponent TabList_Component;
extern struct IGameComponent LocalPlayer_Component;

/* Offset used to avoid floating point roundoff errors. */
#define ENTITY_ADJUSTMENT 0.001f
/* Maxmimum number of characters in a model name. */
#define ENTITY_MAX_MODEL_LENGTH 11

#define ENTITIES_MAX_COUNT 256
#define ENTITIES_SELF_ID 255

typedef enum NameMode_ {
	NAME_MODE_NONE, NAME_MODE_HOVERED, NAME_MODE_ALL, NAME_MODE_ALL_HOVERED, NAME_MODE_ALL_UNSCALED, NAME_MODE_COUNT
} NameMode;
NameMode Entities_NameMode;
extern const char* NameMode_Names[NAME_MODE_COUNT];

typedef enum ShadowMode_ {
	SHADOW_MODE_NONE, SHADOW_MODE_SNAP_TO_BLOCK, SHADOW_MODE_CIRCLE, SHADOW_MODE_CIRCLE_ALL, SHADOW_MODE_COUNT
} ShadowMode;
ShadowMode Entities_ShadowMode;
extern const char* ShadowMode_Names[SHADOW_MODE_COUNT];

#define ENTITY_TYPE_NONE 0
#define ENTITY_TYPE_PLAYER 1

#define LOCATIONUPDATE_FLAG_POS   0x01
#define LOCATIONUPDATE_FLAG_HEADX 0x02
#define LOCATIONUPDATE_FLAG_HEADY 0x04
#define LOCATIONUPDATE_FLAG_ROTX  0x08
#define LOCATIONUPDATE_FLAG_ROTZ  0x10
/* Represents a location update for an entity. Can be a relative position, full position, and/or an orientation update. */
struct LocationUpdate {
	Vector3 Pos;
	float HeadX, HeadY, RotX, RotZ;
	uint8_t Flags;
	bool RelativePos;
};

/* Clamps the given angle so it lies between [0, 360). */
float LocationUpdate_Clamp(float degrees);
void LocationUpdate_MakeOri(struct LocationUpdate* update, float rotY, float headX);
void LocationUpdate_MakePos(struct LocationUpdate* update, Vector3 pos, bool rel);
void LocationUpdate_MakePosAndOri(struct LocationUpdate* update, Vector3 pos, float rotY, float headX, bool rel);

struct Entity;
struct EntityVTABLE {
	void (*Tick)(struct Entity* e, double delta);
	void (*Despawn)(struct Entity* e);
	void (*SetLocation)(struct Entity* e, struct LocationUpdate* update, bool interpolate);
	PackedCol (*GetCol)(struct Entity* e);
	void (*RenderModel)(struct Entity* e, double deltaTime, float t);
	void (*RenderName)(struct Entity* e);
	void (*ContextLost)(struct Entity* e);
	void (*ContextRecreated)(struct Entity* e);
};

/* Contains a model, along with position, velocity, and rotation. May also contain other fields and properties. */
struct Entity {
	struct EntityVTABLE* VTABLE;
	Vector3 Position;
	float HeadX, HeadY, RotX, RotY, RotZ;
	Vector3 Velocity;

	struct Model* Model;
	BlockID ModelBlock; /* BlockID, if model name was originally a vaid block id. */
	struct AABB ModelAABB;
	Vector3 ModelScale, Size;
	float StepSize;
	
	uint8_t SkinType, EntityType;
	bool NoShade, OnGround;
	GfxResourceID TextureId, MobTextureId;
	float uScale, vScale;
	struct Matrix Transform;

	struct AnimatedComp Anim;
	char SkinNameRaw[STRING_SIZE];
};
typedef bool (*Entity_TouchesCondition)(BlockID block);

void Entity_Init(struct Entity* e);
Vector3 Entity_GetEyePosition(struct Entity* e);
float Entity_GetEyeHeight(struct Entity* e);
void Entity_GetTransform(struct Entity* e, Vector3 pos, Vector3 scale, struct Matrix* m);
void Entity_GetPickingBounds(struct Entity* e, struct AABB* bb);
void Entity_GetBounds(struct Entity* e, struct AABB* bb);
void Entity_SetModel(struct Entity* e, const String* model);
void Entity_UpdateModelBounds(struct Entity* e);
bool Entity_TouchesAny(struct AABB* bb, Entity_TouchesCondition cond);
bool Entity_TouchesAnyRope(struct Entity* e);	
bool Entity_TouchesAnyLava(struct Entity* e);
bool Entity_TouchesAnyWater(struct Entity* e);

struct Entity* Entities_List[ENTITIES_MAX_COUNT];
void Entities_Tick(struct ScheduledTask* task);
void Entities_RenderModels(double delta, float t);
void Entities_RenderNames(double delta);
void Entities_RenderHoveredNames(double delta);
void Entities_Init(void);
void Entities_Free(void);
void Entities_Remove(EntityID id);
EntityID Entities_GetCloset(struct Entity* src);
void Entities_DrawShadows(void);

#define TABLIST_MAX_NAMES 256
StringsBuffer TabList_Buffer;
uint16_t TabList_PlayerNames[TABLIST_MAX_NAMES];
uint16_t TabList_ListNames[TABLIST_MAX_NAMES];
uint16_t TabList_GroupNames[TABLIST_MAX_NAMES];
uint8_t  TabList_GroupRanks[TABLIST_MAX_NAMES];
bool TabList_Valid(EntityID id);
bool TabList_Remove(EntityID id);
void TabList_Set(EntityID id, const String* player, const String* list, const String* group, uint8_t rank);

#define TabList_UNSAFE_GetPlayer(id) StringsBuffer_UNSAFE_Get(&TabList_Buffer, TabList_PlayerNames[id]);
#define TabList_UNSAFE_GetList(id)   StringsBuffer_UNSAFE_Get(&TabList_Buffer, TabList_ListNames[id]);
#define TabList_UNSAFE_GetGroup(id)  StringsBuffer_UNSAFE_Get(&TabList_Buffer, TabList_GroupNames[id]);
#define Player_Layout struct Entity Base; char DisplayNameRaw[STRING_SIZE]; bool FetchedSkin; struct Texture NameTex;

/* Represents a player entity. */
struct Player { Player_Layout };
void Player_SetName(struct Player* player, const String* name, const String* skin);
void Player_UpdateNameTex(struct Player* player);
void Player_ResetSkin(struct Player* player);

/* Represents another entity in multiplayer */
struct NetPlayer {
	Player_Layout
	struct NetInterpComp Interp;
	bool ShouldRender;
};
void NetPlayer_Init(struct NetPlayer* player, const String* displayName, const String* skinName);
struct NetPlayer NetPlayers_List[ENTITIES_SELF_ID];

/* Represents the user/player's own entity. */
struct LocalPlayer {
	Player_Layout
	Vector3 Spawn, OldVelocity;
	float SpawnRotY, SpawnHeadX, ReachDistance;
	struct HacksComp Hacks;
	struct TiltComp Tilt;
	struct InterpComp Interp;
	struct CollisionsComp Collisions;
	struct PhysicsComp Physics;
	bool _WarnedRespawn, _WarnedFly, _WarnedNoclip;
};

struct LocalPlayer LocalPlayer_Instance;
void LocalPlayer_Init(void);
float LocalPlayer_JumpHeight(void);
void LocalPlayer_CheckHacksConsistency(void);
void LocalPlayer_SetInterpPosition(float t);
bool LocalPlayer_HandlesKey(Key key);
#endif
