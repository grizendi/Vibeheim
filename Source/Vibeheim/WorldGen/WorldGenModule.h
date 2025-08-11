#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

// LogWorldGen is declared in VoxelPluginAdapter.h

class FWorldGenModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};