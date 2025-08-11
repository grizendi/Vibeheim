#include "WorldGenModule.h"
#include "VoxelPluginAdapter.h"
#include "Modules/ModuleManager.h"

// LogWorldGen is defined in VoxelPluginAdapter.cpp

void FWorldGenModule::StartupModule()
{
    UE_LOG(LogWorldGen, Log, TEXT("WorldGen module started"));
}

void FWorldGenModule::ShutdownModule()
{
    UE_LOG(LogWorldGen, Log, TEXT("WorldGen module shutdown"));
}

// WorldGen is a subsystem within Vibeheim module, not a separate module