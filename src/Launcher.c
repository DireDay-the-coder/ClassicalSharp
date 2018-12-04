#include "Launcher.h"
#include "LScreens.h"
#include "Resources.h"
#include "Drawer2D.h"
#include "Game.h"
#include "Deflate.h"
#include "Stream.h"
#include "Utils.h"
#include "Input.h"
#include "Window.h"
#include "GameStructs.h"
#include "Event.h"

struct LScreen* Launcher_Screen;
bool Launcher_Dirty;
Rect2D Launcher_DirtyArea;
Bitmap Launcher_Framebuffer;
bool Launcher_ClassicBackground;
FontDesc Launcher_TitleFont, Launcher_TextFont;

bool Launcher_ShouldExit, Launcher_ShouldUpdate;
TimeMS Launcher_PatchTime;
struct ServerListEntry* Launcher_PublicServers;
int Launcher_NumServers;

void Launcher_ShowError(ReturnCode res, const char* place) {
	String msg; char msgBuffer[STRING_SIZE * 2];
	String_InitArray_NT(msg, msgBuffer);

	String_Format2(&msg, "Error %x when %c", &res, place);
	msg.buffer[msg.length] = '\0';
	Window_ShowDialog("Error", msg.buffer);
}

/* TODO: FIX THESE STUBS!!! */
void Launcher_SecureSetOpt(const char* opt, const String* data, const String* key) { }

internal UpdateCheckTask checkTask;
static bool fullRedraw, pendingRedraw;
static FontDesc logoFont;

static void Launcher_RedrawAll(void) {
	Launcher_ResetPixels();
	if (Launcher_Screen) Launcher_Screen->DrawAll(Launcher_Screen);
	fullRedraw = true;
}

static void Launcher_ReqeustRedraw(void) {
	/* We may get multiple Redraw events in short timespan */
	/* So we just request a redraw at next launcher tick */
	pendingRedraw  = true;
	Launcher_Dirty = true;
}

/* updates window state on resize and redraws contents. */
static void Launcher_OnResize(void) {
	Game_UpdateClientSize();
	Launcher_Framebuffer.Width  = Game_Width;
	Launcher_Framebuffer.Height = Game_Height;

	Window_InitRaw(&Launcher_Framebuffer);
	Launcher_RedrawAll();
}

void Launcher_SetScreen(struct LScreen* screen) {
	if (Launcher_Screen) Launcher_Screen->Free(Launcher_Screen);
	Launcher_ResetPixels();
	Launcher_Screen = screen;

	screen->Init(screen);
	/* for hovering over active button etc */
	screen->MouseMove(screen, 0, 0);
}

static void Launcher_Init(void) {
	BitmapCol col = BITMAPCOL_CONST(125, 125, 125, 255);

	Event_RegisterVoid(&WindowEvents_Resized,      NULL, Launcher_OnResize);
	Event_RegisterVoid(&WindowEvents_StateChanged, NULL, Launcher_OnResize);
	Window.FocusedChanged += RedrawAll;
	Window.Redraw += RedrawPending;
	Keyboard.KeyDown += KeyDown;

	Options_Load();
	Options_Get(OPT_FONT_NAME, &Game_FontName, Font_DefaultName);
	/* TODO: Handle Arial font not working */
	Font_Make(&logoFont,           &Game_FontName, 32, FONT_STYLE_NORMAL);
	Font_Make(&Launcher_TitleFont, &Game_FontName, 16, FONT_STYLE_BOLD);
	Font_Make(&Launcher_TextFont,  &Game_FontName, 14, FONT_STYLE_NORMAL);

	Drawer2D_Cols['g'] = col;
	Utils_EnsureDirectory("texpacks");
	Utils_EnsureDirectory("audio");
}

void Dispose() {
	Event_UnregisterVoid(&WindowEvents_Resized,      NULL, Launcher_OnResize);
	Event_UnregisterVoid(&WindowEvents_StateChanged, NULL, Launcher_OnResize);

	Window.FocusedChanged -= RedrawAll;
	Window.Redraw -= RedrawPending;
	Keyboard.KeyDown -= KeyDown;

	List<FastBitmap> bitmaps = FetchFlagsTask.Bitmaps;
	for (int i = 0; i < bitmaps.Count; i++) {
		bitmaps[i].Dispose();
		bitmaps[i].Bitmap.Dispose();
	}

	Font_Free(&logoFont);
	Font_Free(&Launcher_TitleFont);
	Font_Free(&Launcher_TextFont);
}

void Run() {
	Window = Factory.CreateWindow(640, 400, Program.AppName,
		GraphicsMode.Default, DisplayDevice.Default);
	Window_SetVisible(true);
	Drawer2D_Component.Init();
	Game_UpdateClientSize();

	Launcher_Init();
	Launcher_TryLoadTexturePack();

	Launcher_Framebuffer.Width  = Game_Width;
	Launcher_Framebuffer.Height = Game_Height;
	Window_InitRaw(&Launcher_Framebuffer);

	Downloader = new AsyncDownloader(Drawer);
	Downloader.Init("");
	Downloader.Cookies = new CookieContainer();
	Downloader.KeepAlive = true;

	Resources_CheckExistence();
	checkTask = new UpdateCheckTask();
	checkTask.RunAsync(this);

	if (Resources_Count) {
		SetScreen(new ResourcesScreen(this));
	} else {
		SetScreen(new MainScreen(this));
	}

	while (true) {
		Window_ProcessEvents();
		if (!Window_Exists) break;
		if (Launcher_ShouldExit) break;

		checkTask.Tick();
		Launcher_Screen->Tick(Launcher_Screen);
		if (Launcher_Dirty) Launcher_Display();
		Thread_Sleep(10);
	}

	if (Options.Load()) {
		LauncherSkin.SaveToOptions();
		Options.Save();
	}

	if (Launcher_Screen) {
		Launcher_Screen->Free(Launcher_Screen);
		Launcher_Screen = NULL;
	}

	if (Launcher_ShouldUpdate)
		Updater.Applier.ApplyUpdate();
	if (Window_Exists)
		Window_Close();
}

void Display() {
	if (pendingRedraw) {
		Launcher_RedrawAll();
		pendingRedraw = false;
	}

	Launcher_Screen->OnDisplay(Launcher_Screen);
	Launcher_Dirty = false;

	Rectangle rec = new Rectangle(0, 0, Framebuffer.Width, Framebuffer.Height);
	if (!fullRedraw && DirtyArea.Width > 0) {
		rec = DirtyArea;
	}

	Window_DrawRaw(rec);
	DirtyArea = Rectangle.Empty;
	fullRedraw = false;
}

void KeyDown(Key key) {
	if (IsShutdown(key)) Launcher_ShouldExit = true;
}

static bool Launcher_IsShutdown(Key key) {
	if (key == KEY_F4 && Key_IsAltPressed()) return true;

	/* On OSX, Cmd+Q should also terminate the process */
#ifdef CC_BUILD_OSX
	return key == Key.Q && Key_IsWinPressed();
#else
	return false;
#endif
}

/*########################################################################################################################*
*---------------------------------------------------------Colours/Skin----------------------------------------------------*
*#########################################################################################################################*/
BitmapCol Launcher_BackgroundCol       = BITMAPCOL_CONST(153, 127, 172, 255);
BitmapCol Launcher_ButtonBorderCol     = BITMAPCOL_CONST( 97,  81, 110, 255);
BitmapCol Launcher_ButtonForeActiveCol = BITMAPCOL_CONST(189, 168, 206, 255);
BitmapCol Launcher_ButtonForeCol       = BITMAPCOL_CONST(141, 114, 165, 255);
BitmapCol Launcher_ButtonHighlightCol  = BITMAPCOL_CONST(162, 131, 186, 255);

void Launcher_ResetSkin(void) {
	/* Have to duplicate it here, sigh */
	BitmapCol defaultBackgroundCol       = BITMAPCOL_CONST(153, 127, 172, 255);
	BitmapCol defaultButtonBorderCol     = BITMAPCOL_CONST( 97,  81, 110, 255);
	BitmapCol defaultButtonForeActiveCol = BITMAPCOL_CONST(189, 168, 206, 255);
	BitmapCol defaultButtonForeCol       = BITMAPCOL_CONST(141, 114, 165, 255);
	BitmapCol defaultButtonHighlightCol  = BITMAPCOL_CONST(162, 131, 186, 255);

	Launcher_BackgroundCol       = defaultBackgroundCol;
	Launcher_ButtonBorderCol     = defaultButtonBorderCol;
	Launcher_ButtonForeActiveCol = defaultButtonForeActiveCol;
	Launcher_ButtonForeCol       = defaultButtonForeCol;
	Launcher_ButtonHighlightCol  = defaultButtonHighlightCol;
}

CC_NOINLINE static void Launcher_GetCol(const char* key, BitmapCol* col) {
	PackedCol tmp;
	String value;
	if (!Options_UNSAFE_Get(key, &value))     return;
	if (!PackedCol_TryParseHex(&value, &tmp)) return;

	col->R = tmp.R; col->G = tmp.G; col->B = tmp.B;
}

void Launcher_LoadSkin(void) {
	Launcher_GetCol("launcher-back-col",                   &Launcher_BackgroundCol);
	Launcher_GetCol("launcher-btn-border-col",             &Launcher_ButtonBorderCol);
	Launcher_GetCol("launcher-btn-fore-active-col",        &Launcher_ButtonForeActiveCol);
	Launcher_GetCol("launcher-btn-fore-inactive-col",      &Launcher_ButtonForeCol);
	Launcher_GetCol("launcher-btn-highlight-inactive-col", &Launcher_ButtonHighlightCol);
}

CC_NOINLINE static void Launcher_SetCol(const char* key, BitmapCol col) {
	String value; char valueBuffer[8];
	PackedCol tmp;
	tmp.R = col.R; tmp.G = col.G; tmp.B = col.B; tmp.A = 0;
	
	String_InitArray(value, valueBuffer);
	PackedCol_ToHex(&value, tmp);
	Options_Set(key, &value);
}

void Launcher_SaveSkin(void) {
	Launcher_SetCol("launcher-back-col",                   Launcher_BackgroundCol);
	Launcher_SetCol("launcher-btn-border-col",             Launcher_ButtonBorderCol);
	Launcher_SetCol("launcher-btn-fore-active-col",        Launcher_ButtonForeActiveCol);
	Launcher_SetCol("launcher-btn-fore-inactive-col",      Launcher_ButtonForeCol);
	Launcher_SetCol("launcher-btn-highlight-inactive-col", Launcher_ButtonHighlightCol);
}


/*########################################################################################################################*
*----------------------------------------------------------Background-----------------------------------------------------*
*#########################################################################################################################*/
static FontDesc logoFont;
static bool useBitmappedFont;
static Bitmap terrainBmp, fontBmp;
#define TILESIZE 48

static bool Launcher_SelectZipEntry(const String* path) {
	return
		String_CaselessEqualsConst(path, "default.png") ||
		String_CaselessEqualsConst(path, "terrain.png");
}

static void Launcher_LoadTextures(Bitmap* bmp) {
	int tileSize = bmp->Width / 16;
	Bitmap_Allocate(&terrainBmp, TILESIZE * 2, TILESIZE);

	/* Precompute the scaled background */
	Drawer2D_BmpScaled(&terrainBmp, TILESIZE, 0, TILESIZE, TILESIZE,
						bmp, 2 * tileSize, 0, tileSize, tileSize,
						TILESIZE, TILESIZE, 128, 64);
	Drawer2D_BmpScaled(&terrainBmp, 0, 0, TILESIZE, TILESIZE,
						bmp, 1 * tileSize, 0, tileSize, tileSize,
						TILESIZE, TILESIZE, 96, 96);					
}

static void Launcher_ProcessZipEntry(const String* path, struct Stream* data, struct ZipEntry* entry) {
	Bitmap bmp;
	ReturnCode res;

	if (String_CaselessEqualsConst(path, "default.png")) {
		if (fontBmp.Scan0) return;
		res = Png_Decode(&fontBmp, data);

		if (res) {
			Launcher_ShowError(res, "decoding default.png"); return;
		} else {
			Drawer2D_SetFontBitmap(&fontBmp);
			useBitmappedFont = !Options_GetBool(OPT_USE_CHAT_FONT, false);
		}
	} else if (String_CaselessEqualsConst(path, "terrain.png")) {
		if (terrainBmp.Scan0) return;
		res = Png_Decode(&bmp, data);

		if (res) {
			Launcher_ShowError(res, "decoding default.png"); return;
		} else {
			Drawer2D_SetFontBitmap(&bmp);
			Launcher_LoadTextures(&bmp);
		}
	}
}

static void Launcher_ExtractTexturePack(const String* path) {
	struct ZipState state;
	struct Stream stream;
	ReturnCode res;

	res = Stream_OpenFile(&stream, path);
	if (res) {
		Launcher_ShowError(res, "opening texture pack"); return;
	}

	Zip_Init(&state, &stream);
	state.SelectEntry  = Launcher_SelectZipEntry;
	state.ProcessEntry = Launcher_ProcessZipEntry;
	res = Zip_Extract(&state);

	if (res) {
		Launcher_ShowError(res, "extracting texture pack");
	}
	stream.Close(&stream);
}

void Launcher_TryLoadTexturePack(void) {
	static String defZipPath = String_FromConst("texpacks/default.zip");
	String path; char pathBuffer[FILENAME_SIZE];
	String texPack;

	if (Options_UNSAFE_Get("nostalgia-classicbg", &texPack)) {
		Launcher_ClassicBackground = Options_GetBool("nostalgia-classicbg", false);
	} else {
		Launcher_ClassicBackground = Options_GetBool(OPT_CLASSIC_MODE,      false);
	}

	Options_UNSAFE_Get(OPT_DEFAULT_TEX_PACK, &texPack);
	String_InitArray(path, pathBuffer);
	String_Format1(&path, "texpacks/", &texPack);

	if (!texPack.length || !File_Exists(&path)) path = defZipPath;
	if (!File_Exists(&path)) return;

	Launcher_ExtractTexturePack(&path);
	/* user selected texture pack is missing some required .png files */
	if (!fontBmp.Scan0 || !terrainBmp.Scan0) {
		Launcher_ExtractTexturePack(&defZipPath);
	}
}

static void Launcher_ClearTile(int x, int y, int width, int height, int srcX) {
	Drawer2D_BmpTiled(&Launcher_Framebuffer, x, y, width, height,
		&terrainBmp, srcX, 0, TILESIZE, TILESIZE);
}

void Launcher_ResetArea(int x, int y, int width, int height) {
	if (Launcher_ClassicBackground && terrainBmp.Scan0) {
		Launcher_ClearTile(x, y, width, height, 0);
	} else {
		Gradient_Noise(&Launcher_Framebuffer, Launcher_BackgroundCol, 6, x, y, width, height);
	}
}

void Launcher_ResetPixels(void) {
	static String title_fore = String_FromConst("&eClassi&fCube");
	static String title_back = String_FromConst("&0Classi&0Cube");
	struct DrawTextArgs args;
	int x;

	if (Launcher_ClassicBackground && terrainBmp.Scan0) {
		Launcher_ClearTile(0,        0, Game_Width,               TILESIZE, TILESIZE);
		Launcher_ClearTile(0, TILESIZE, Game_Width, Game_Height - TILESIZE, 0);
	} else {
		Launcher_ResetArea(0, 0, Game_Width, Game_Height);
	}

	Drawer2D_BitmappedText = (useBitmappedFont || Launcher_ClassicBackground) && fontBmp.Scan0;
	DrawTextArgs_Make(&args, &title_fore, &logoFont, false);
	x = Game_Width / 2 - Drawer2D_MeasureText(&args).Width / 2;

	args.Text = title_back;
	Drawer2D_DrawText(&Launcher_Framebuffer, &args, x + 4, 4);
	args.Text = title_fore;
	Drawer2D_DrawText(&Launcher_Framebuffer, &args, x, 0);

	Drawer2D_BitmappedText = false;
	Launcher_Dirty = true;
}

static TimeMS lastJoin;
bool Launcher_StartGame(const String* user, const String* mppass, const String* ip, const String* port, const String* server) {
#ifdef CC_BUILD_WINDOWS
	static String exe = String_FromConst("ClassiCube.exe");
#else
	static String exe = String_FromConst("ClassiCube");
#endif
	String args; char argsBuffer[512];
	TimeMS now;
	ReturnCode res;
	
	now = DateTime_CurrentUTC_MS();
	if (lastJoin + 1000 < now) return false;
	lastJoin = now;

	/* Make sure if the client has changed some settings in the meantime, we keep the changes */
	Options_Load();
	Launcher_ShouldExit = Options_GetBool(OPT_AUTO_CLOSE_LAUNCHER, false);

	/* Save resume info */
	if (server) {
		Options_Set("launcher-server",   server);
		Options_Set("launcher-username", user);
		Options_Set("launcher-ip",       ip);
		Options_Set("launcher-port",     port);
		Launcher_SecureSetOpt("launcher-mppass", mppass, user);
		Options_Save();
	}

	String_InitArray(args, argsBuffer);
	String_AppendString(&args, user);
	if (mppass->length) String_Format3(&args, "%s %s %s", mppass, ip, port);

	res = Platform_StartProcess(&exe, &args);
#ifdef CC_BUILD_WINDOWS
	/* TODO: Check this*/
	/* HRESULT when user clicks 'cancel' to 'are you sure you want to run ClassiCube.exe' */
	if (res == 0x80004005) return;
#endif

	if (res) {
		Launcher_ShowError(res, "starting game");
		Launcher_ShouldExit = false;
		return false;
	}
	return true;
}
