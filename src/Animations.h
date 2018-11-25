#ifndef CC_ANIMS_H
#define CC_ANIMS_H
#include "Core.h"
/* Texture animations, and water and lava liquid animations.
   Copyright 2014 - 2017 ClassicalSharp | Licensed under BSD-3
   Based off the incredible work from https://dl.dropboxusercontent.com/u/12694594/lava.txt
   mirrored at https://github.com/UnknownShadow200/ClassicalSharp/wiki/Minecraft-Classic-lava-animation-algorithm
   Water animation originally written by cybertoon, big thanks!
*/

struct IGameComponent;
struct ScheduledTask;
extern struct IGameComponent Animations_Component;

void Animations_Tick(struct ScheduledTask* task);
#endif
