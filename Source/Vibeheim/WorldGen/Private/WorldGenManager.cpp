#include "WorldGenManager.h"
#include "WorldGenSettings.h"
#include "Services/HeightfieldService.h"
#include "Services/ClimateSystem.h"
#include "Services/BiomeService.h"
#include "Services/PCGWorldService.h"
#include "Data/WorldGenTypes.h"
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

	// Connect services together
	HeightfieldService->SetClimateSystem(ClimateSystem);
	// HeightfieldService->SetNoiseSystem(NoiseSystem); // TODO: Add when NoiseSystem is available

	UE_LOG(LogWorldGenManager, Log, TEXT("All world generation systems initialized successfully"));
	return true;
}

void AWorldGenManager::UpdateWorldStreaming()
{
	if (!WorldGenSettings || !HeightfieldService)
	{
		return;
	}

	// Get current player position
	FTileCoord CurrentPlayerTile = GetPlayerTileCoordinate();
	
	// Check if player has moved to a different tile
	FVector CurrentPlayerPos = FVector(
		CurrentPlayerTile.X * WorldGenSettings->Settings.TileSizeMeters,
		CurrentPlayerTile.Y * WorldGenSettings->Settings.TileSizeMeters,
		0.0f
	);

	float DistanceMoved = FVector::Dist(LastPlayerPosition, CurrentPlayerPos);
	
	// Only update streaming if player has moved significantly or this is the first update
	if (DistanceMoved > WorldGenSettings->Settings.TileSizeMeters * 0.5f || LastPlayerPosition.IsZero())
	{
		LastPlayerPosition = CurrentPlayerPos;

		// Calculate tiles that need to be generated
		TArray<FTileCoord> TilesToGenerate = CalculateTilesToGenerate(CurrentPlayerTile);

		if (TilesToGenerate.Num() > 0)
		{
			UE_LOG(LogWorldGenManager, Log, TEXT("Player moved to tile (%d, %d), generating %d tiles"), 
				CurrentPlayerTile.X, CurrentPlayerTile.Y, TilesToGenerate.Num());

			// Generate tiles around the player
			GenerateSurroundingTiles(TilesToGenerate);
		}
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
	return FTileCoord::FromWorldPosition(PlayerLocation, WorldGenSettings->Settings.TileSizeMeters);
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
	OutTileGenerationTimeMs = TotalTilesGenerated > 0 ? TotalTileGenerationTime / TotalTilesGenerated : 0.0f;
	OutPCGGenerationTimeMs = TotalTilesGenerated > 0 ? TotalPCGGenerationTime / TotalTilesGenerated : 0.0f;
	
	// Get loaded tiles from heightfield service
	float AvgGenTime;
	int32 CachedTiles;
	if (HeightfieldService)
	{
		HeightfieldService->GetPerformanceStats(AvgGenTime, CachedTiles);
		OutLoadedTiles = CachedTiles;
	}
	else
	{
		OutLoadedTiles = 0;
	}
	
	OutPendingLoads = 0; // TODO: Implement pending loads tracking when streaming system is added
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
