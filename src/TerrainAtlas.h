#ifndef CC_TERRAINATLAS_H
#define CC_TERRAINATLAS_H
#include "Bitmap.h"
/* Represents the 2D texture atlas of terrain.png, and converted into an array of 1D textures.
   Copyright 2014-2017 ClassicalSharp | Licensed under BSD-3
*/

#define ATLAS2D_TILES_PER_ROW 16
#define ATLAS2D_MASK 15
#define ATLAS2D_SHIFT 4
#ifdef EXTENDED_TEXTURES
#define ATLAS2D_MAX_ROWS_COUNT 32
#else
#define ATLAS2D_MAX_ROWS_COUNT 16
#endif
#define ATLAS1D_MAX_ATLASES (ATLAS2D_TILES_PER_ROW * ATLAS2D_MAX_ROWS_COUNT)

Bitmap Atlas2D_Bitmap;
int Atlas2D_TileSize, Atlas2D_RowsCount;
int Atlas1D_Count, Atlas1D_TilesPerAtlas;
int Atlas1D_Mask, Atlas1D_Shift;
float Atlas1D_InvTileSize;
GfxResourceID Atlas1D_TexIds[ATLAS1D_MAX_ATLASES];

#define Atlas2D_TileX(texLoc) ((texLoc) &  ATLAS2D_MASK)  /* texLoc % ATLAS2D_TILES_PER_ROW */
#define Atlas2D_TileY(texLoc) ((texLoc) >> ATLAS2D_SHIFT) /* texLoc / ATLAS2D_TILES_PER_ROW */
/* Returns the index of the given tile id within a 1D atlas */
#define Atlas1D_RowId(texLoc) ((texLoc)  & Atlas1D_Mask)  /* texLoc % Atlas1D_TilesPerAtlas */
/* Returns the index of the 1D atlas within the array of 1D atlases that contains the given tile id */
#define Atlas1D_Index(texLoc) ((texLoc) >> Atlas1D_Shift) /* texLoc / Atlas1D_TilesPerAtlas */

void Atlas2D_UpdateState(Bitmap* bmp);
GfxResourceID Atlas2D_LoadTile(TextureLoc texLoc);
void Atlas2D_Free(void);
TextureRec Atlas1D_TexRec(TextureLoc texLoc, int uCount, int* index);
void Atlas1D_UpdateState(void);
int Atlas1D_UsedAtlasesCount(void);
void Atlas1D_Free(void);
#endif
