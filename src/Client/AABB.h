#ifndef CS_AABB_H
#define CS_AABB_H
#include "Typedefs.h"
#include "Vectors.h"
/* Describes an axis aligned bounding box, and contains various methods related to them.
   Copyright 2014-2017 ClassicalSharp | Licensed under BSD-3
*/

/* Descibes an axis aligned bounding box. */
typedef struct AABB {
	/* Minimum coordinate of this AABB. */
	Vector3 Min;
	/* Maximum coordinate of this AABB. */
	Vector3 Max;
} AABB;


/* Returns the width of the given bounding box. */
#define AABB_Width(bb)  (bb->Max.X - bb.Min.X)
/* Returns the height of the given bounding box. */
#define AABB_Height(bb) (bb->Max.Y - bb.Min.Y)
/* Returns the length of the given bounding box. */
#define AABB_Length(bb) (bb->Max.Z - bb.Min.Z)


void AABB_FromCoords6(Real32 x1, Real32 y1, Real32 z1, Real32 x2, Real32 y2, Real32 z2);

void AABB_FromCoords(Vector3* min, Vector3* max, AABB* result);

void AABB_Make(Vector3* pos, Vector3* size, AABB* result);


/* Returns a new AABB, with the min and max coordinates of the original AABB translated by the given vector. */
void AABB_Offset(AABB* bb, Vector3* amount, AABB* result);

/* Returns a new AABB, with the min and max coordinates of the original AABB expanded away from AABB centre by the given vector. */
void AABB_Expand(AABB* bb, Vector3* amount, AABB* result);

/* Returns a new AABB, with the min and max coordinates of the original AABB scaled by the given value. */
void AABB_Scale(AABB* bb, Real32 scale, AABB* result);

/* Determines whether this AABB intersects the given AABB on any axes. */
bool AABB_Intersects(AABB* bb, AABB* other);

/* Determines whether this AABB entirely contains the AABB on all axes. */
bool AAAB_Contains(AABB* parent, AABB* child);

/* Determines whether this AABB entirely contains the coordinates on all axes. */
bool AABB_ContainsPoint(AABB* parent, Vector3* P);
#endif