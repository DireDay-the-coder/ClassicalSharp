#ifndef CC_ENVRENDERER_H
#define CC_ENVRENDERER_H
#include "String.h"
/* Renders environment of the map. (clouds, sky, fog, map sides/edges, skybox, rain/snow)
   Copyright 2014-2017 ClassicalSharp | Licensed under BSD-3
*/
struct IGameComponent;
extern struct IGameComponent EnvRenderer_Component;

#define ENV_MINIMAL 1
#define ENV_LEGACY  2

/* Renders coloured sky plane. */
void EnvRenderer_RenderSky(double deltaTime);
/* Renders textured cloud plane. */
void EnvRenderer_RenderClouds(double deltaTime);
/* Updates current fog colour and mode. */
void EnvRenderer_UpdateFog(void);

/* Renders borders around map and under horizon. */
void EnvRenderer_RenderMapSides(double delta);
/* Renders flat horizon surrounding map. */
void EnvRenderer_RenderMapEdges(double delta);
/* Renders a skybox around the player. */
void EnvRenderer_RenderSkybox(double deltaTime);
/* Whether a skybox should be rendered. */
bool EnvRenderer_ShouldRenderSkybox(void);

extern int16_t* Weather_Heightmap;
void EnvRenderer_OnBlockChanged(int x, int y, int z, BlockID oldBlock, BlockID newBlock);
/* Renders rainfall/snowfall weather. */
void EnvRenderer_RenderWeather(double deltaTime);

/* Whether large quads are broken down into smaller quads. */
/* This makes them have less rendering issues when using vertex fog. */
extern bool EnvRenderer_Legacy;
/* Whether minimal environmental effects are rendered. */
/* Minimal mode disables skybox, clouds and fog. */
extern bool EnvRenderer_Minimal;
/* Sets whether Legacy and Minimal modes are used based on given flags. */
void EnvRenderer_SetMode(int flags);
/* Calculates mode flags for the given mode. */
/* mode can be: normal, normalfast, legacy, legacyfast */
CC_NOINLINE int EnvRenderer_CalcFlags(const String* mode);
#endif
