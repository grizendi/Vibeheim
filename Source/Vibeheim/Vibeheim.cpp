// Copyright Epic Games, Inc. All Rights Reserved.

#include "Vibeheim.h"
#include "Modules/ModuleManager.h"

static_assert(ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 6,
              "Vibeheim requires Unreal Engine 5.6.x");

IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, Vibeheim, "Vibeheim" );
