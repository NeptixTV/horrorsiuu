#pragma once

#include "CoreMinimal.h"

HORRORHEIST_API DECLARE_LOG_CATEGORY_EXTERN(LogHorrorHeist, Log, All);

/** Physical surface ids (see DefaultEngine.ini [/Script/Engine.PhysicsSettings]). */
#define HH_SURFACE_CONCRETE SurfaceType1
#define HH_SURFACE_WOOD     SurfaceType2
#define HH_SURFACE_METAL    SurfaceType3
#define HH_SURFACE_FABRIC   SurfaceType4
#define HH_SURFACE_WATER    SurfaceType5
#define HH_SURFACE_GRAVEL   SurfaceType6
