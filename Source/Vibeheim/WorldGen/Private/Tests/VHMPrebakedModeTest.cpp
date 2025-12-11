#if WITH_AUTOMATION_TESTS

#include "VHMPrebakedModeTest.h"
#include "Misc/AutomationTest.h"
#include "Data/WorldGenTerrainResource.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "VHMTerrainRendering/VHMTerrainRenderer.h"
#include "WorldGenSettings.h"

/**
 * Validates that prebaked mode initializes and renders a tile without invoking runtime height generation.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVHMPrebakedModeBypassRuntimeGeneration,
	"Vibeheim.WorldGen.VHM.PreBaked.BypassRuntimeGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVHMPrebakedModeBypassRuntimeGeneration::RunTest(const FString& Parameters)
{
	// Create an isolated world for spawning components
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("VHMPrebakedTestWorld")));
	if (!TestWorld)
	{
		AddError(TEXT("Failed to create test world."));
		return false;
	}

	// Prepare config with prebaked heightfield enabled
	UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
	if (!Settings)
	{
		AddError(TEXT("Failed to acquire WorldGenSettings."));
		TestWorld->DestroyWorld(false);
		return false;
	}

	const TOptional<FVHMSettings> PreviousVHMSettings = Settings->VHMSettings;
	FVHMSettings PrebakedSettings = Settings->VHMSettings.Get(FVHMSettings());
	PrebakedSettings.bUsePrebakedHeightfield = true;
	Settings->VHMSettings = PrebakedSettings;

	// Build a simple prebaked terrain resource
	UWorldGenTerrainResource* TerrainResource = NewObject<UWorldGenTerrainResource>();
	TerrainResource->WorldOrigin = FVector2D::ZeroVector;
	TerrainResource->TileSizeMeters = Settings->Settings.TileSizeMeters;
	TerrainResource->SampleSpacingMeters = Settings->Settings.SampleSpacingMeters;

	constexpr int32 TextureSize = 4;
	UTexture2D* HeightTexture = NewObject<UTexture2D>();
	HeightTexture->Source.Init(TextureSize, TextureSize, 1, 1, TSF_R32F);

	TArray<float> HeightValues;
	HeightValues.SetNumZeroed(TextureSize * TextureSize);
	for (int32 Index = 0; Index < HeightValues.Num(); ++Index)
	{
		HeightValues[Index] = 100.0f + Index; // deterministic unique values
	}

	void* MipData = HeightTexture->Source.LockMip(0);
	FMemory::Memcpy(MipData, HeightValues.GetData(), HeightValues.Num() * sizeof(float));
	HeightTexture->Source.UnlockMip(0);
	HeightTexture->CompressionSettings = TC_HDR;
	HeightTexture->SRGB = 0;
	HeightTexture->UpdateResource();

	TerrainResource->HeightTextures.Add(FIntPoint::ZeroValue, HeightTexture);

	// Initialize renderer with prebaked resource and stub services
	UVHMTestHeightfieldService* CountingHeightService = NewObject<UVHMTestHeightfieldService>();
	CountingHeightService->Initialize(Settings->Settings);

	UVHMTerrainRenderer* Renderer = NewObject<UVHMTerrainRenderer>(TestWorld);
	Renderer->SetPrebakedTerrainResource(TerrainResource);

	const bool bInitialized =
		Renderer->Initialize(Settings, CountingHeightService, /*TileStreamingService*/ nullptr);
	TestTrue(TEXT("Renderer initializes in prebaked mode without streaming services"), bInitialized);

	// Attempt to create a mesh from prebaked data; runtime generation should not be hit
	const bool bCreated = Renderer->CreateTerrainMeshForTile(FTileCoord(0, 0));
	TestTrue(TEXT("Renderer creates terrain mesh from prebaked data"), bCreated);
	TestEqual(TEXT("Runtime GenerateHeightfield was not called"), CountingHeightService->GenerateCalls, 0);
	TestTrue(TEXT("Runtime GetCachedHeightfield should not be used for prebaked path"),
	         CountingHeightService->CachedCalls <= 1);

	// Cleanup and restore settings
	Renderer->Cleanup();
	if (PreviousVHMSettings.IsSet())
	{
		Settings->VHMSettings = PreviousVHMSettings.GetValue();
	}
	TestWorld->DestroyWorld(false);
	return true;
}

#endif // WITH_AUTOMATION_TESTS
