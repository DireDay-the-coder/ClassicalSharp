#ifndef CC_EVENT_H
#define CC_EVENT_H
#include "String.h"
#include "Vectors.h"
/* Helper method for managing events, and contains all events.
   Copyright 2014-2017 ClassicalSharp | Licensed under BSD-3
*/

/* Maximum number of event handlers that can be registered. */
#define EVENT_MAX_CALLBACKS 32
struct Stream;

typedef void (*Event_Void_Callback)(void* obj);
struct Event_Void {
	Event_Void_Callback Handlers[EVENT_MAX_CALLBACKS]; 
	void* Objs[EVENT_MAX_CALLBACKS]; int Count;
};

typedef void (*Event_Int_Callback)(void* obj, int argument);
struct Event_Int {
	Event_Int_Callback Handlers[EVENT_MAX_CALLBACKS];
	void* Objs[EVENT_MAX_CALLBACKS]; int Count;
};

typedef void (*Event_Float_Callback)(void* obj, float argument);
struct Event_Float {
	Event_Float_Callback Handlers[EVENT_MAX_CALLBACKS];
	void* Objs[EVENT_MAX_CALLBACKS]; int Count;
};

typedef void (*Event_Entry_Callback)(void* obj, struct Stream* stream, const String* name);
struct Event_Entry {
	Event_Entry_Callback Handlers[EVENT_MAX_CALLBACKS];
	void* Objs[EVENT_MAX_CALLBACKS]; int Count;
};

typedef void (*Event_Block_Callback)(void* obj, Vector3I coords, BlockID oldBlock, BlockID block);
struct Event_Block {
	Event_Block_Callback Handlers[EVENT_MAX_CALLBACKS];
	void* Objs[EVENT_MAX_CALLBACKS]; int Count;
};

typedef void (*Event_MouseMove_Callback)(void* obj, int xDelta, int yDelta);
struct Event_MouseMove {
	Event_MouseMove_Callback Handlers[EVENT_MAX_CALLBACKS];
	void* Objs[EVENT_MAX_CALLBACKS]; int Count;
};

typedef void (*Event_Chat_Callback)(void* obj, const String* msg, int msgType);
struct Event_Chat {
	Event_Chat_Callback Handlers[EVENT_MAX_CALLBACKS];
	void* Objs[EVENT_MAX_CALLBACKS]; int Count;
};

void Event_RaiseVoid(struct Event_Void* handlers);
void Event_RegisterVoid(struct Event_Void* handlers,   void* obj, Event_Void_Callback handler);
void Event_UnregisterVoid(struct Event_Void* handlers, void* obj, Event_Void_Callback handler);

void Event_RaiseInt(struct Event_Int* handlers, int arg);
void Event_RegisterInt(struct Event_Int* handlers,   void* obj, Event_Int_Callback handler);
void Event_UnregisterInt(struct Event_Int* handlers, void* obj, Event_Int_Callback handler);

void Event_RaiseFloat(struct Event_Float* handlers, float arg);
void Event_RegisterFloat(struct Event_Float* handlers,   void* obj, Event_Float_Callback handler);
void Event_UnregisterFloat(struct Event_Float* handlers, void* obj, Event_Float_Callback handler);

void Event_RaiseEntry(struct Event_Entry* handlers, struct Stream* stream, const String* name);
void Event_RegisterEntry(struct Event_Entry* handlers,   void* obj, Event_Entry_Callback handler);
void Event_UnregisterEntry(struct Event_Entry* handlers, void* obj, Event_Entry_Callback handler);

void Event_RaiseBlock(struct Event_Block* handlers, Vector3I coords, BlockID oldBlock, BlockID block);
void Event_RegisterBlock(struct Event_Block* handlers,   void* obj, Event_Block_Callback handler);
void Event_UnregisterBlock(struct Event_Block* handlers, void* obj, Event_Block_Callback handler);

void Event_RaiseMouseMove(struct Event_MouseMove* handlers, int xDelta, int yDelta);
void Event_RegisterMouseMove(struct Event_MouseMove* handlers,   void* obj, Event_MouseMove_Callback handler);
void Event_UnregisterMouseMove(struct Event_MouseMove* handlers, void* obj, Event_MouseMove_Callback handler);

void Event_RaiseChat(struct Event_Chat* handlers, const String* msg, int msgType);
void Event_RegisterChat(struct Event_Chat* handlers,   void* obj, Event_Chat_Callback handler);
void Event_UnregisterChat(struct Event_Chat* handlers, void* obj, Event_Chat_Callback handler);

extern struct Event_Int EntityEvents_Added;    /* Entity is spawned in the current world */
extern struct Event_Int EntityEvents_Removed;  /* Entity is despawned from the current world */
extern struct Event_Int TabListEvents_Added;   /* Tab list entry is created */
extern struct Event_Int TabListEvents_Changed; /* Tab list entry is modified */
extern struct Event_Int TabListEvents_Removed; /* Tab list entry is removed */

extern struct Event_Void TextureEvents_AtlasChanged; /* Terrain atlas (terrain.png) is changed */
extern struct Event_Void TextureEvents_PackChanged;  /* Texture pack is changed */
extern struct Event_Entry TextureEvents_FileChanged; /* File in a texture pack is changed (terrain.png, rain.png) */

extern struct Event_Void GfxEvents_ViewDistanceChanged; /* View/fog distance is changed */
extern struct Event_Void GfxEvents_LowVRAMDetected;     /* Insufficient VRAM detected, need to free some GPU resources */
extern struct Event_Void GfxEvents_ProjectionChanged;   /* Projection matrix has changed */
extern struct Event_Void GfxEvents_ContextLost;         /* Context is destroyed after having been previously created */
extern struct Event_Void GfxEvents_ContextRecreated;    /* Context is recreated after having been previously lost */

extern struct Event_Block UserEvents_BlockChanged;          /* User changes a block */
extern struct Event_Void UserEvents_HackPermissionsChanged; /* Hack permissions of the player changes */
extern struct Event_Void UserEvents_HeldBlockChanged;       /* Held block in hotbar changes */

extern struct Event_Void BlockEvents_PermissionsChanged; /* Block permissions (can place/delete) for a block changes */
extern struct Event_Void BlockEvents_BlockDefChanged;    /* Block definition is changed or removed */

extern struct Event_Void WorldEvents_NewMap;       /* Player begins loading a new world */
extern struct Event_Float WorldEvents_Loading;      /* Portion of world is decompressed/generated (Arg is progress from 0-1) */
extern struct Event_Void WorldEvents_MapLoaded;    /* New world has finished loading, player can now interact with it */
extern struct Event_Int WorldEvents_EnvVarChanged; /* World environment variable changed by player/CPE/WoM config */

extern struct Event_Void ChatEvents_FontChanged;   /* User changes whether system chat font used, and when the bitmapped font texture changes */
extern struct Event_Chat ChatEvents_ChatReceived;  /* Raised when message is being added to chat */
extern struct Event_Chat ChatEvents_ChatSending;   /* Raised when user sends a message */
extern struct Event_Int ChatEvents_ColCodeChanged; /* Raised when a colour code changes */

extern struct Event_Void WindowEvents_Redraw;             /* Window contents invalidated, should be redrawn */
extern struct Event_Void WindowEvents_Moved;              /* Window is moved */
extern struct Event_Void WindowEvents_Resized;            /* Window is resized */
extern struct Event_Void WindowEvents_Closing;            /* Window is about to close */
extern struct Event_Void WindowEvents_Closed;             /* Window has closed */
extern struct Event_Void WindowEvents_VisibilityChanged;  /* Visibility of the window changed */
extern struct Event_Void WindowEvents_FocusChanged;       /* Focus of the window changed */
extern struct Event_Void WindowEvents_StateChanged;       /* WindowState of the window changed */

extern struct Event_Int KeyEvents_Press; /* Raised when a character is typed. Arg is a character */
extern struct Event_Int KeyEvents_Down;  /* Raised when a key is pressed. Arg is a member of Key enumeration */
extern struct Event_Int KeyEvents_Up;    /* Raised when a key is released. Arg is a member of Key enumeration */

extern struct Event_MouseMove MouseEvents_Moved; /* Mouse position is changed (Arg is delta from last position) */
extern struct Event_Int MouseEvents_Down;        /* Mouse button is pressed (Arg is MouseButton member) */
extern struct Event_Int MouseEvents_Up;          /* Mouse button is released (Arg is MouseButton member) */
extern struct Event_Float MouseEvents_Wheel;      /* Mouse wheel is moved/scrolled (Arg is wheel delta) */
#endif
