#include "WorldGenManager.h"
#include "WorldGenSettings.h"
#include "Services/HeightfieldService.h"
#include "Services/ClimateSystem.h"
#include "Services/BiomeService.h"
#include "Services/PCGWorldService.h"
#include "Services/TileStreamingService.h"
#include "Services/POIService.h"
#include "VHMTerrainRendering/VHMTerrainRenderer.h"
#include "VHMTerrainRendering/VHMDebugSystem.h"
#include "Data/WorldGenTypes.h"
#include "Data/WorldGenAssets.h"
#include "UObject/SoftObjectPath.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/DateTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorldGenManager, Log, All);

AWorldGenManager::AWorldGenManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f; // Tick 10 times per second for streaming updates

	// Initialize default values
	StreamingUpdateInterval = 1.0f; // Update streaming every second
	LastStreamingUpdateTime = 0.0f;
	LastPlayerPosition = FVector::ZeroVector;
	TotalTileGenerationTime = 0.0f;
	TotalPCGGenerationTime = 0.0f;
	TotalTilesGenerated = 0;

	// Initialize service pointers
	WorldGenSettings = nullptr;
	HeightfieldService = nullptr;
	ClimateSystem = nullptr;
	BiomeService = nullptr;
	PCGWorldService = nullptr;
	TileStreamingService = nullptr;
	POIService = nullptr;
	VHMTerrainRenderer = nullptr;
	VHMDebugSystem = nullptr;
}

void AWorldGenManager::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogWorldGenManager, Log, TEXT("WorldGenManager BeginPlay - Initializing world generation systems"));

	// Initialize world generation systems
	if (!InitializeWorldGenSystems())
	{
		UE_LOG(LogWorldGenManager, Error, TEXT("Failed to initialize world generation systems"));
		HandleWorldGenerationError(TEXT("System initialization failed"));
		return;
	}

	// Perform initial streaming update
	UpdateWorldStreaming();

	UE_LOG(LogWorldGenManager, Log, TEXT("WorldGenManager initialization complete"));
}

void AWorldGenManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update streaming at specified intervals
	LastStreamingUpdateTime += DeltaTime;
	if (LastStreamingUpdateTime >= StreamingUpdateInterval)
	{
		UpdateWorldStreaming();
		LastStreamingUpdateTime = 0.0f;
	}
}

bool AWorldGenManager::InitializeWorldGenSystems()
{
    // Load world generation settings
    WorldGenSettings = UWorldGenSettings::GetWorldGenSettings();
    if (!WorldGenSettings)
	{
		UE_LOG(LogWorldGenManager, Error, TEXT("Failed to load WorldGen settings"));
		return false;
	}

    // Apply fixed streaming radii for test world (Generate=9, Load=5, Active=3)
    WorldGenSettings->Settings.GenerateRadius = 9;
    WorldGenSettings->Settings.LoadRadius = 5;
    WorldGenSettings->Settings.ActiveRadius = 3;
    UE_LOG(LogWorldGenManager, Log, TEXT("Applied fixed streaming radii: Generate=9, Load=5, Active=3"));

    // Resolve data assets (settings + biome definitions)
    ResolveWorldGenAssets();

	// Configure VHM settings for seam prevention
	if (!WorldGenSettings->VHMSettings.IsSet())
	{
		WorldGenSettings->VHMSettings = FVHMSettings();
	}
	FVHMSettings& VHMConfig = WorldGenSettings->VHMSettings.GetValue();
	VHMConfig.bEnableBoundaryStitching = true;
	VHMConfig.BoundaryBlendFactor = 0.5f;
	VHMConfig.BoundaryUpdateRadius = 1;
	UE_LOG(LogWorldGenManager, Log, TEXT("Configured VHM settings for seam prevention"));

	// Initialize Heightfield Service
	HeightfieldService = NewObject<UHeightfieldService>(this);
	if (!HeightfieldService || !HeightfieldService->Initialize(WorldGenSettings->Settings))
	{
		UE_LOG(LogWorldGenManager, Error, TEXT("Failed to initialize Heightfield Service"));
		return false;
	}

	// Initialize Climate System
	ClimateSystem = NewObject<UClimateSystem>(this);
	if (!ClimateSystem)
	{
		UE_LOG(LogWorldGenManager, Error, TEXT("Failed to create Climate System"));
		return false;
	}

	// Initialize climate system with default settings
	FClimateSettings ClimateSettings;
	ClimateSystem->Initialize(ClimateSettings, WorldGenSettings->Settings.Seed);

	// Initialize Biome Service
	BiomeService = NewObject<UBiomeService>(this);
	if (!BiomeService)
	{
		UE_LOG(LogWorldGenManager, Error, TEXT("Failed to initialize Biome Service"));
		return false;
	}

	// Initialize PCG World Service
	PCGWorldService = NewObject<UPCGWorldService>(this);
	if (!PCGWorldService || !PCGWorldService->Initialize(WorldGenSettings->Settings))
	{
		UE_LOG(LogWorldGenManager, Error, TEXT("Failed to initialize PCG World Service"));
		return false;
	}

	// Initialize POI Service
	POIService = NewObject<UPOIService>(this);
	if (!POIService || !POIService->Initialize(WorldGenSettings->Settings))
	{
		UE_LOG(LogWorldGenManager, Error, TEXT("Failed to initialize POI Service"));
		return false;
	}

	// Initialize Tile Streaming Service
	TileStreamingService = NewObject<UTileStreamingService>(this);
	if (!TileStreamingService || !TileStreamingService->Initialize(WorldGenSettings->Settings, HeightfieldService, BiomeService, PCGWorldService))
	{
		UE_LOG(LogWorldGenManager, Error, TEXT("Failed to initialize Tile Streaming Service"));
		return false;
	}

	// Initialize VHM Terrain Renderer with biome service for material support
	VHMTerrainRenderer = NewObject<UVHMTerrainRenderer>(this);
	if (!VHMTerrainRenderer || !VHMTerrainRenderer->InitializeWithBiomeService(WorldGenSettings, HeightfieldService, TileStreamingService, BiomeService))
	{
		UE_LOG(LogWorldGenManager, Error, TEXT("Failed to initialize VHM Terrain Renderer"));
		// Fallback: try basic initialization without biome service
		if (!VHMTerrainRenderer->Initialize(WorldGenSettings, HeightfieldService, TileStreamingService))
		{
			UE_LOG(LogWorldGenManager, Error, TEXT("VHM001: VHM Terrain Renderer initialization failed - using flat meadow fallback"));
			HandleWorldGenerationError(TEXT("VHM terrain rendering unavailable"));
		}
		else
		{
			UE_LOG(LogWorldGenManager, Warning, TEXT("VHM Terrain Renderer initialized without biome service"));
		}
	}

	// Initialize VHM Debug System
	VHMDebugSystem = NewObject<UVHMDebugSystem>(this);
	if (!VHMDebugSystem || !VHMDebugSystem->Initialize(VHMTerrainRenderer, GetWorld()))
	{
		UE_LOG(LogWorldGenManager, Error, TEXT("Failed to initialize VHM Debug System"));
		return false;
	}

	// Connect services together
	HeightfieldService->SetClimateSystem(ClimateSystem);
	POIService->SetBiomeService(BiomeService);
	POIService->SetHeightfieldService(HeightfieldService);
	
	// Connect VHM renderer to tile streaming events
	if (VHMTerrainRenderer && TileStreamingService)
	{
		TileStreamingService->SetVHMTerrainRenderer(VHMTerrainRenderer);
		UE_LOG(LogWorldGenManager, Log, TEXT("Connected VHM Terrain Renderer to tile streaming events"));
	}

	UE_LOG(LogWorldGenManager, Log, TEXT("All world generation systems initialized successfully"));
	return true;
}

void AWorldGenManager::UpdateWorldStreaming()
{
	if (!WorldGenSettings || !TileStreamingService)
	{
		return;
	}

	// Get current player position and update streaming
	FTileCoord CurrentPlayerTile = GetPlayerTileCoordinate();
	TileStreamingService->UpdateStreaming(CurrentPlayerTile);

	// Update VHM terrain renderer with current viewer position for LOD management
	if (VHMTerrainRenderer)
	{
		FVector ViewerPosition = FVector::ZeroVector;
		if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
		{
			if (APawn* PlayerPawn = PlayerController->GetPawn())
			{
				ViewerPosition = PlayerPawn->GetActorLocation();
			}
		}
		VHMTerrainRenderer->UpdateLODLevels(ViewerPosition);
		
		// Limit the number of active VHM components to prevent crashes
		TArray<FTileCoord> ActiveTiles = VHMTerrainRenderer->GetActiveMeshTiles();
		const int32 MaxActiveTiles = 25; // Reasonable limit for testing
		if (ActiveTiles.Num() > MaxActiveTiles)
		{
			UE_LOG(LogWorldGenManager, Log, TEXT("Too many active VHM tiles (%d), cleaning up oldest tiles"), ActiveTiles.Num());
			// Remove excess tiles (keep the first MaxActiveTiles)
			for (int32 i = MaxActiveTiles; i < ActiveTiles.Num(); i++)
			{
				VHMTerrainRenderer->RemoveTerrainMesh(ActiveTiles[i]);
			}
		}
	}

	// Update performance tracking from streaming service
	FTileStreamingMetrics StreamingMetrics = TileStreamingService->GetPerformanceMetrics();
	
	// Log performance info periodically
	static int32 UpdateCounter = 0;
	UpdateCounter++;
	if (UpdateCounter % 100 == 0) // Every 100 updates (roughly every 10 seconds)
	{
		UE_LOG(LogWorldGenManager, Log, TEXT("Streaming Stats: Active=%d, Loaded=%d, Generated=%d, AvgGenTime=%.2fms, CacheEff=%.2f%%"),
			StreamingMetrics.ActiveTiles, StreamingMetrics.LoadedTiles, StreamingMetrics.GeneratedTiles,
			StreamingMetrics.AverageGenerationTimeMs, StreamingMetrics.CacheEfficiency * 100.0f);
	}
}

void AWorldGenManager::ResolveWorldGenAssets()
{
    static const TCHAR* DefaultSettingsPath = TEXT("/Game/Data/WorldGen/DA_WorldGenSettings_Default.DA_WorldGenSettings_Default");
    static const TCHAR* DefaultBiomesPath   = TEXT("/Game/Data/WorldGen/DA_BiomeDefinitions_Default.DA_BiomeDefinitions_Default");

    // Resolve settings asset soft reference
    if (!WorldGenSettingsAsset.IsValid())
    {
        const FSoftObjectPath Existing = WorldGenSettingsAsset.ToSoftObjectPath();
        if (!Existing.IsValid())
        {
            WorldGenSettingsAsset = TSoftObjectPtr<UWorldGenSettingsAsset>(FSoftObjectPath(DefaultSettingsPath));
        }
    }

    UWorldGenSettingsAsset* SettingsAsset = WorldGenSettingsAsset.IsNull() ? nullptr : WorldGenSettingsAsset.LoadSynchronous();
    if (!SettingsAsset)
    {
        UE_LOG(LogWorldGenManager, Warning, TEXT("WorldGenSettingsAsset not found (path: %s)"), *WorldGenSettingsAsset.ToString());
    }
    else
    {
        TArray<FString> Errors;
        if (!SettingsAsset->ValidateAsset(Errors))
        {
            for (const FString& E : Errors)
            {
                UE_LOG(LogWorldGenManager, Warning, TEXT("SettingsAsset validation: %s"), *E);
            }
        }

        // Allow settings asset to drive biome definitions reference if unset
        if (!BiomeDefinitionsAsset.IsValid() && SettingsAsset->BiomeDefinitions.ToSoftObjectPath().IsValid())
        {
            BiomeDefinitionsAsset = SettingsAsset->BiomeDefinitions;
        }
    }

    // Resolve biome definitions soft reference
    if (!BiomeDefinitionsAsset.IsValid())
    {
        const FSoftObjectPath Existing = BiomeDefinitionsAsset.ToSoftObjectPath();
        if (!Existing.IsValid())
        {
            BiomeDefinitionsAsset = TSoftObjectPtr<UBiomeDefinitionsAsset>(FSoftObjectPath(DefaultBiomesPath));
        }
    }

    UBiomeDefinitionsAsset* BiomesAsset = BiomeDefinitionsAsset.IsNull() ? nullptr : BiomeDefinitionsAsset.LoadSynchronous();
    if (!BiomesAsset)
    {
        UE_LOG(LogWorldGenManager, Warning, TEXT("BiomeDefinitionsAsset not found (path: %s)"), *BiomeDefinitionsAsset.ToString());
    }
    else
    {
        TArray<FString> Errors;
        if (!BiomesAsset->ValidateAsset(Errors))
        {
            for (const FString& E : Errors)
            {
                UE_LOG(LogWorldGenManager, Warning, TEXT("BiomeDefinitions validation: %s"), *E);
            }
        }
    }
}

void AWorldGenManager::ReloadWorldGenAssets()
{
    ResolveWorldGenAssets();
    UE_LOG(LogWorldGenManager, Log, TEXT("Reloaded WorldGen data assets (settings/biomes)"));

    UWorldGenSettingsAsset* SettingsAsset = WorldGenSettingsAsset.IsNull() ? nullptr : WorldGenSettingsAsset.LoadSynchronous();
    UBiomeDefinitionsAsset* BiomesAsset   = BiomeDefinitionsAsset.IsNull() ? nullptr : BiomeDefinitionsAsset.LoadSynchronous();

    if (WorldGenSettings)
    {
        WorldGenSettings->SelectedSettingsAsset = WorldGenSettingsAsset;
        WorldGenSettings->SelectedBiomeDefinitionsAsset = BiomeDefinitionsAsset;

        TArray<FString> Warnings;
        if (WorldGenSettings->ApplyFromAssets(SettingsAsset, BiomesAsset, Warnings))
        {
            for (const FString& W : Warnings)
            {
                UE_LOG(LogWorldGenManager, Verbose, TEXT("Asset apply: %s"), *W);
            }
            UE_LOG(LogWorldGenManager, Log, TEXT("Applied Data Assets to world generation settings"));
        }
    }

    if (BiomeService && BiomesAsset)
    {
        BiomeService->SetBiomeDefinitions(BiomesAsset->Biomes);
        UE_LOG(LogWorldGenManager, Log, TEXT("Applied BiomeDefinitionsAsset to BiomeService (%d biomes)"), BiomesAsset->Biomes.Num());
    }
}

FTileCoord AWorldGenManager::GetPlayerTileCoordinate() const
{
	// Get the first player controller
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PlayerController || !PlayerController->GetPawn())
	{
		return FTileCoord(0, 0); // Default to origin if no player found
	}

    FVector PlayerLocation = PlayerController->GetPawn()->GetActorLocation();
    // Convert Unreal centimeters to meters before mapping to tile coords
    FVector PlayerLocationMeters = PlayerLocation / 100.0f;
    return FTileCoord::FromWorldPosition(PlayerLocationMeters, WorldGenSettings->Settings.TileSizeMeters);
}

TArray<FTileCoord> AWorldGenManager::CalculateTilesToGenerate(const FTileCoord& PlayerTileCoord)
{
	TArray<FTileCoord> TilesToGenerate;
	
	if (!WorldGenSettings)
	{
		return TilesToGenerate;
	}

	int32 GenerateRadius = WorldGenSettings->Settings.GenerateRadius;

	// Generate tiles in a square pattern around the player
	for (int32 X = PlayerTileCoord.X - GenerateRadius; X <= PlayerTileCoord.X + GenerateRadius; X++)
	{
		for (int32 Y = PlayerTileCoord.Y - GenerateRadius; Y <= PlayerTileCoord.Y + GenerateRadius; Y++)
		{
			FTileCoord TileCoord(X, Y);
			
			// Check if tile is already cached/generated
			FHeightfieldData ExistingHeightfield;
			if (!HeightfieldService->GetCachedHeightfield(TileCoord, ExistingHeightfield))
			{
				TilesToGenerate.Add(TileCoord);
			}
		}
	}

	return TilesToGenerate;
}

void AWorldGenManager::GenerateSurroundingTiles(const TArray<FTileCoord>& TilesToGenerate)
{
	if (!WorldGenSettings || !HeightfieldService || !BiomeService || !PCGWorldService)
	{
		return;
	}

	double StartTime = FPlatformTime::Seconds();
	
	for (const FTileCoord& TileCoord : TilesToGenerate)
	{
		double TileStartTime = FPlatformTime::Seconds();

		try
		{
			// Generate heightfield data
			FHeightfieldData HeightfieldData = HeightfieldService->GenerateHeightfield(
				WorldGenSettings->Settings.Seed, TileCoord);

			// Cache the generated heightfield
			HeightfieldService->CacheHeightfield(HeightfieldData);

			// Generate biome data for the tile
			EBiomeType TileBiome = BiomeService->DetermineTileBiome(TileCoord, HeightfieldData.HeightData);

			// Generate PCG content for the tile
			FPCGGenerationData PCGData = PCGWorldService->GenerateBiomeContent(
				TileCoord, TileBiome, HeightfieldData.HeightData);

			double TileEndTime = FPlatformTime::Seconds();
			float TileGenTime = static_cast<float>((TileEndTime - TileStartTime) * 1000.0);

			// Update performance metrics
			UpdatePerformanceMetrics(TileGenTime, PCGData.GenerationTimeMs);

			UE_LOG(LogWorldGenManager, Verbose, TEXT("Generated tile (%d, %d) in %.2fms - Biome: %d, Instances: %d"), 
				TileCoord.X, TileCoord.Y, TileGenTime, static_cast<int32>(TileBiome), PCGData.TotalInstanceCount);

			// Check performance targets
			if (TileGenTime > WorldGenSettings->Settings.TileGenTargetMs)
			{
				UE_LOG(LogWorldGenManager, Warning, TEXT("Tile generation exceeded target time: %.2fms > %.2fms"), 
					TileGenTime, WorldGenSettings->Settings.TileGenTargetMs);
			}
		}
		catch (...)
		{
			UE_LOG(LogWorldGenManager, Error, TEXT("Exception occurred while generating tile (%d, %d)"), 
				TileCoord.X, TileCoord.Y);
			HandleWorldGenerationError(FString::Printf(TEXT("Tile generation failed for (%d, %d)"), 
				TileCoord.X, TileCoord.Y));
		}
	}

	double EndTime = FPlatformTime::Seconds();
	float TotalGenTime = static_cast<float>((EndTime - StartTime) * 1000.0);

	UE_LOG(LogWorldGenManager, Log, TEXT("Generated %d tiles in %.2fms (avg %.2fms per tile)"), 
		TilesToGenerate.Num(), TotalGenTime, 
		TilesToGenerate.Num() > 0 ? TotalGenTime / TilesToGenerate.Num() : 0.0f);
}

void AWorldGenManager::GetWorldGenPerformanceStats(float& OutTileGenerationTimeMs, float& OutPCGGenerationTimeMs, int32& OutLoadedTiles, int32& OutPendingLoads)
{
	if (TileStreamingService)
	{
		// Get metrics from tile streaming service
		FTileStreamingMetrics StreamingMetrics = TileStreamingService->GetPerformanceMetrics();
		OutTileGenerationTimeMs = StreamingMetrics.AverageGenerationTimeMs;
		OutPCGGenerationTimeMs = StreamingMetrics.AverageGenerationTimeMs; // PCG time is included in generation time
		OutLoadedTiles = StreamingMetrics.LoadedTiles + StreamingMetrics.ActiveTiles;
		OutPendingLoads = StreamingMetrics.PendingGenerations;
	}
	else
	{
		// Fallback to old system if streaming service unavailable
		OutTileGenerationTimeMs = TotalTilesGenerated > 0 ? TotalTileGenerationTime / TotalTilesGenerated : 0.0f;
		OutPCGGenerationTimeMs = TotalTilesGenerated > 0 ? TotalPCGGenerationTime / TotalTilesGenerated : 0.0f;
		OutLoadedTiles = 0;
		OutPendingLoads = 0;
	}
}

void AWorldGenManager::HandleWorldGenerationError(const FString& ErrorMessage)
{
	UE_LOG(LogWorldGenManager, Error, TEXT("World Generation Error: %s"), *ErrorMessage);

	// Implement fallback systems
	if (WorldGenSettings)
	{
		// Try to reset to default settings if current settings are problematic
		TArray<FString> ValidationErrors;
		if (!WorldGenSettings->ValidateSettings(ValidationErrors))
		{
			UE_LOG(LogWorldGenManager, Warning, TEXT("Attempting to reset to default settings due to validation errors"));
			WorldGenSettings->ResetToDefaults();
		}
	}

	// Clear heightfield cache to force regeneration if needed
	if (HeightfieldService)
	{
		HeightfieldService->ClearHeightfieldCache();
		UE_LOG(LogWorldGenManager, Log, TEXT("Cleared heightfield cache for clean restart"));
	}

	// TODO: Implement additional fallback mechanisms:
	// - Fallback to simpler generation algorithms
	// - Load pre-generated fallback terrain
	// - Notify game systems of degraded world generation
}

void AWorldGenManager::UpdatePerformanceMetrics(float TileGenTime, float PCGGenTime)
{
	TotalTileGenerationTime += TileGenTime;
	TotalPCGGenerationTime += PCGGenTime;
	TotalTilesGenerated++;

	// Log performance warnings if targets are consistently exceeded
	if (TotalTilesGenerated > 0 && TotalTilesGenerated % 10 == 0) // Every 10 tiles
	{
		float AvgTileGenTime = TotalTileGenerationTime / TotalTilesGenerated;
		float AvgPCGGenTime = TotalPCGGenerationTime / TotalTilesGenerated;

		if (WorldGenSettings)
		{
			if (AvgTileGenTime > WorldGenSettings->Settings.TileGenTargetMs)
			{
				UE_LOG(LogWorldGenManager, Warning, TEXT("Average tile generation time (%.2fms) exceeds target (%.2fms)"), 
					AvgTileGenTime, WorldGenSettings->Settings.TileGenTargetMs);
			}

			if (AvgPCGGenTime > WorldGenSettings->Settings.PCGTargetMsPerTile)
			{
				UE_LOG(LogWorldGenManager, Warning, TEXT("Average PCG generation time (%.2fms) exceeds target (%.2fms)"), 
					AvgPCGGenTime, WorldGenSettings->Settings.PCGTargetMsPerTile);
			}
		}
	}
}
