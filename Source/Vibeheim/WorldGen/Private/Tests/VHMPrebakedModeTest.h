#pragma once

#include "CoreMinimal.h"
#include "Services/HeightfieldService.h"
#include "VHMPrebakedModeTest.generated.h"

/**
 * Stub heightfield service that records calls without generating data.
 */
UCLASS()
class UVHMTestHeightfieldService : public UHeightfieldService
{
	GENERATED_BODY()
public:
	int32 GenerateCalls = 0;
	int32 CachedCalls = 0;

	virtual FHeightfieldData GenerateHeightfield(int32 Seed, FTileCoord TileCoord) override
	{
		++GenerateCalls;
		return FHeightfieldData();
	}

	virtual bool GetCachedHeightfield(FTileCoord TileCoord, FHeightfieldData& OutHeightfieldData) override
	{
		++CachedCalls;
		return false;
	}
};
