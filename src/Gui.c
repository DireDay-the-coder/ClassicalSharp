#include "Gui.h"
#include "Window.h"
#include "Game.h"
#include "Graphics.h"
#include "Event.h"
#include "Drawer2D.h"
#include "ExtMath.h"
#include "Screens.h"
#include "Camera.h"
#include "InputHandler.h"
#include "ErrorHandler.h"
#include "Platform.h"
#include "Bitmap.h"

GfxResourceID Gui_GuiTex, Gui_GuiClassicTex, Gui_IconsTex;
struct Screen* Gui_Status;
struct Screen* Gui_HUD;
struct Screen* Gui_Active;
struct Screen* Gui_Overlays[GUI_MAX_OVERLAYS];
int Gui_OverlaysCount;

void Gui_DefaultRecreate(void* elem) {
	struct GuiElem* e = elem;
	Elem_Free(e); Elem_Init(e);
}

void Screen_CommonInit(void* screen) { 
	struct Screen* s = screen;
	Event_RegisterVoid(&GfxEvents_ContextLost,      s, s->VTABLE->ContextLost);
	Event_RegisterVoid(&GfxEvents_ContextRecreated, s, s->VTABLE->ContextRecreated);

	if (Gfx_LostContext) return;
	s->VTABLE->ContextRecreated(s);
}

void Screen_CommonFree(void* screen) { struct Screen* s = screen;
	Event_UnregisterVoid(&GfxEvents_ContextLost,      s, s->VTABLE->ContextLost);
	Event_UnregisterVoid(&GfxEvents_ContextRecreated, s, s->VTABLE->ContextRecreated);
	s->VTABLE->ContextLost(s);
}

void Widget_SetLocation(void* widget, uint8_t horAnchor, uint8_t verAnchor, int xOffset, int yOffset) {
	struct Widget* w = widget;
	w->HorAnchor = horAnchor; w->VerAnchor = verAnchor;
	w->XOffset   = xOffset;   w->YOffset = yOffset;
	Widget_Reposition(w);
}

void Widget_CalcPosition(void* widget) {
	struct Widget* w = widget;
	w->X = Gui_CalcPos(w->HorAnchor, w->XOffset, w->Width , Game_Width );
	w->Y = Gui_CalcPos(w->VerAnchor, w->YOffset, w->Height, Game_Height);
}

void Widget_Reset(void* widget) {
	struct Widget* w = widget;
	w->Active   = false;
	w->Disabled = false;
	w->X = 0; w->Y = 0;
	w->Width = 0; w->Height = 0;
	w->HorAnchor = ANCHOR_MIN;
	w->VerAnchor = ANCHOR_MIN;
	w->XOffset = 0; w->YOffset = 0;
	w->MenuClick = NULL;
}

bool Widget_Contains(void* widget, int x, int y) {
	struct Widget* w = widget;
	return Gui_Contains(w->X, w->Y, w->Width, w->Height, x, y);
}


/*########################################################################################################################*
*----------------------------------------------------------Gui------------------------------------------------------------*
*#########################################################################################################################*/
int Gui_CalcPos(uint8_t anchor, int offset, int size, int axisLen) {
	if (anchor == ANCHOR_MIN) return offset;
	if (anchor == ANCHOR_MAX) return axisLen - size - offset;
	return (axisLen - size) / 2 + offset;
}

bool Gui_Contains(int recX, int recY, int width, int height, int x, int y) {
	return x >= recX && y >= recY && x < (recX + width) && y < (recY + height);
}

static void Gui_FileChanged(void* obj, struct Stream* stream, const String* name) {
	if (String_CaselessEqualsConst(name, "gui.png")) {
		Game_UpdateTexture(&Gui_GuiTex, stream, name, NULL);
	} else if (String_CaselessEqualsConst(name, "gui_classic.png")) {
		Game_UpdateTexture(&Gui_GuiClassicTex, stream, name, NULL);
	} else if (String_CaselessEqualsConst(name, "icons.png")) {
		Game_UpdateTexture(&Gui_IconsTex, stream, name, NULL);
	}
}

static void Gui_Init(void) {
	Event_RegisterEntry(&TextureEvents_FileChanged, NULL, Gui_FileChanged);
	Gui_Status = StatusScreen_MakeInstance();
	Gui_HUD    = HUDScreen_MakeInstance();
}

static void Gui_Ready(void) {
	Elem_Init(Gui_Status);
	Elem_Init(Gui_HUD);
}

static void Gui_Reset(void) {
	int i;
	for (i = 0; i < Gui_OverlaysCount; i++) {
		Elem_TryFree(Gui_Overlays[i]);
	}
	Gui_OverlaysCount = 0;
}

static void Gui_Free(void) {
	Event_UnregisterEntry(&TextureEvents_FileChanged, NULL, Gui_FileChanged);
	Gui_CloseActive();
	Elem_TryFree(Gui_Status);
	Elem_TryFree(Gui_HUD);

	if (Gui_Active) { Elem_TryFree(Gui_Active); }
	Gfx_DeleteTexture(&Gui_GuiTex);
	Gfx_DeleteTexture(&Gui_GuiClassicTex);
	Gfx_DeleteTexture(&Gui_IconsTex);
	Gui_Reset();
}

struct IGameComponent Gui_Component = {
	Gui_Init,  /* Init  */
	Gui_Free,  /* Free  */
	Gui_Reset, /* Reset */
	NULL, /* OnNewMap */
	NULL, /* OnNewMapLoaded */
	Gui_Ready
};

struct Screen* Gui_GetActiveScreen(void) {
	return Gui_OverlaysCount ? Gui_Overlays[0] : Gui_GetUnderlyingScreen();
}

struct Screen* Gui_GetUnderlyingScreen(void) {
	return Gui_Active ? Gui_Active : Gui_HUD;
}

void Gui_FreeActive(void) {
	if (Gui_Active) { Elem_TryFree(Gui_Active); }
}
void Gui_CloseActive(void) {
	Gui_FreeActive(); 
	Gui_SetActive(NULL);
}

void Gui_SetActive(struct Screen* screen) {
	InputHandler_ScreenChanged(Gui_Active, screen);
	if (screen) { 
		Elem_Init(screen);
		/* for selecting active button etc */
		Elem_HandlesMouseMove(screen, Mouse_X, Mouse_Y);
	}

	Gui_Active = screen;
	Gui_CalcCursorVisible();
}
void Gui_RefreshHud(void) { Elem_Recreate(Gui_HUD); }

void Gui_ShowOverlay(struct Screen* overlay, bool atFront) {
	int i;
	if (Gui_OverlaysCount == GUI_MAX_OVERLAYS) {
		ErrorHandler_Fail("Gui_ShowOverlay - hit max count");
	}

	if (atFront) {		
		/* Insert overlay at start of list */
		for (i = Gui_OverlaysCount - 1; i > 0; i--) {
			Gui_Overlays[i] = Gui_Overlays[i - 1];
		}
		Gui_Overlays[0] = overlay;
	} else {
		Gui_Overlays[Gui_OverlaysCount] = overlay;
	}
	Gui_OverlaysCount++;

	Elem_Init(overlay);
	Gui_CalcCursorVisible();
}

int Gui_IndexOverlay(const void* overlay) {
	const struct Screen* s = overlay;
	int i;

	for (i = 0; i < Gui_OverlaysCount; i++) {
		if (Gui_Overlays[i] == s) return i;
	}
	return -1;
}

void Gui_FreeOverlay(void* overlay) {
	struct Screen* s = overlay;
	int i;

	Elem_Free(s);
	i = Gui_IndexOverlay(overlay);
	if (i == -1) return;

	for (; i < Gui_OverlaysCount - 1; i++) {
		Gui_Overlays[i] = Gui_Overlays[i + 1];
	}

	Gui_OverlaysCount--;
	Gui_Overlays[Gui_OverlaysCount] = NULL;
	Gui_CalcCursorVisible();
}

void Gui_RenderGui(double delta) {
	bool showHUD, hudBefore;
	Gfx_Mode2D(Game_Width, Game_Height);

	showHUD   = !Gui_Active || !Gui_Active->HidesHUD;
	hudBefore = !Gui_Active || !Gui_Active->RenderHUDOver;
	if (showHUD) { Elem_Render(Gui_Status, delta); }

	if (showHUD && hudBefore)  { Elem_Render(Gui_HUD, delta); }
	if (Gui_Active)            { Elem_Render(Gui_Active, delta); }
	if (showHUD && !hudBefore) { Elem_Render(Gui_HUD, delta); }

	if (Gui_OverlaysCount) { Elem_Render(Gui_Overlays[0], delta); }
	Gfx_Mode3D();
}

void Gui_OnResize(void) {
	int i;
	if (Gui_Active) { Screen_OnResize(Gui_Active); }
	Screen_OnResize(Gui_HUD);

	for (i = 0; i < Gui_OverlaysCount; i++) {
		Screen_OnResize(Gui_Overlays[i]);
	}
}

static bool gui_cursorVisible = true;
void Gui_CalcCursorVisible(void) {
	bool vis = Gui_GetActiveScreen()->HandlesAllInput;
	if (vis == gui_cursorVisible) return;
	gui_cursorVisible = vis;

	Window_SetCursorVisible(vis);
	if (Window_Focused)
		Camera_Active->RegrabMouse();
}


/*########################################################################################################################*
*-------------------------------------------------------TextAtlas---------------------------------------------------------*
*#########################################################################################################################*/
void TextAtlas_Make(struct TextAtlas* atlas, const String* chars, const FontDesc* font, const String* prefix) {
	struct DrawTextArgs args; 
	Size2D size;
	Bitmap bmp;
	int i;

	DrawTextArgs_Make(&args, prefix, font, true);
	size = Drawer2D_MeasureText(&args);

	atlas->Offset   = size.Width;
	atlas->FontSize = font->Size;
	size.Width += 16 * chars->length;

	Mem_Set(atlas->Widths, 0, sizeof(atlas->Widths));
	Bitmap_AllocateClearedPow2(&bmp, size.Width, size.Height);
	{
		Drawer2D_DrawText(&bmp, &args, 0, 0);		
		for (i = 0; i < chars->length; i++) {
			args.Text = String_UNSAFE_Substring(chars, i, 1);
			atlas->Widths[i] = Drawer2D_MeasureText(&args).Width;

			Drawer2D_DrawText(&bmp, &args, atlas->Offset + font->Size * i, 0);
		}
		Drawer2D_Make2DTexture(&atlas->Tex, &bmp, size, 0, 0);
	}	
	Mem_Free(bmp.Scan0);

	Drawer2D_ReducePadding_Tex(&atlas->Tex, Math_Floor(font->Size), 4);
	atlas->uScale = 1.0f / (float)bmp.Width;
	atlas->Tex.uv.U2 = atlas->Offset * atlas->uScale;
	atlas->Tex.Width = atlas->Offset;	
}

void TextAtlas_Free(struct TextAtlas* atlas) { Gfx_DeleteTexture(&atlas->Tex.ID); }

void TextAtlas_Add(struct TextAtlas* atlas, int charI, VertexP3fT2fC4b** vertices) {
	struct Texture part = atlas->Tex;
	int width       = atlas->Widths[charI];
	PackedCol white = PACKEDCOL_WHITE;

	part.X  = atlas->CurX; part.Width = width;
	part.uv.U1 = (atlas->Offset + charI * atlas->FontSize) * atlas->uScale;
	part.uv.U2 = part.uv.U1 + width                        * atlas->uScale;

	atlas->CurX += width;	
	Gfx_Make2DQuad(&part, white, vertices);
}

void TextAtlas_AddInt(struct TextAtlas* atlas, int value, VertexP3fT2fC4b** vertices) {
	char digits[STRING_INT_CHARS];
	int i, count;

	if (value < 0) {
		TextAtlas_Add(atlas, 10, vertices); value = -value; /* - sign */
	}
	count = String_MakeUInt32((uint32_t)value, digits);

	for (i = count - 1; i >= 0; i--) {
		TextAtlas_Add(atlas, digits[i] - '0' , vertices);
	}
}
