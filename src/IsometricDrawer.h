#ifndef CC_ISOMETRICDRAWER_H
#define CC_ISOMETRICDRAWER_H
#include "VertexStructs.h"
/* Draws 2D isometric blocks for the hotbar and inventory UIs.
   Copyright 2014-2017 ClassicalSharp | Licensed under BSD-3
*/

/* Maximum number of vertices used to draw a block in isometric way. */
#define ISOMETRICDRAWER_MAXVERTICES 16
/* Sets up state to begin drawing blocks isometrically. */
void IsometricDrawer_BeginBatch(VertexP3fT2fC4b* vertices, GfxResourceID vb);
/* Buffers the vertices needed to draw the given block at the given position. */
void IsometricDrawer_DrawBatch(BlockID block, float size, float x, float y);
/* Flushes buffered vertices to the GPU, then restores state. */
void IsometricDrawer_EndBatch(void);
#endif
