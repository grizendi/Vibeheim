// Temporarily disabled to fix module loading issues
#if 0
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

// TEMPORARY: Disable console commands to prevent startup crashes
// TODO: Re-enable with proper deferred registration
#if 0
#include "Misc/App.h"
#include "HAL/PlatformTime.h"
#include "WorldGenSettings.h"
#include "Services/NoiseSystem.h"
#include "Services/ClimateSystem.h"
#include "Services/HeightfieldService.h"
#include "Services/BiomeService.h"
#include "Services/PCGWorldService.h"
#include "Services/TileStreamingService.h"
#include "Services/POIService.h"
#include "Tests/WorldGenIntegrationTest.h"
#include "VHMTerrainRendering/VHMTerrainRenderer.h"
#include "VHMTerrainRendering/VHMDebugSystem.h"
#include "VHMTerrainRendering/TerrainMaterialSystem.h"
#include "VHMTerrainRendering/TerrainLODManager.h"
#include "VHMTerrainRendering/HeightfieldTextureManager.h"
#include "VHMTerrainRendering/TileBoundaryManager.h"
#include "GameFramework/Actor.h"
#include "VirtualHeightfieldMeshComponent.h"
#include "WorldGenManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorldGenConsole, Log, All);

// Safety macro for console commands to prevent crashes during engine startup
#define WORLDGEN_CONSOLE_SAFETY_CHECK() \
	if (!GEngine || !GEngine->GetWorld() || !IsInGameThread()) \
	{ \
		UE_LOG(LogWorldGenConsole, Warning, TEXT("WorldGen console command ignored - engine not ready")); \
		return; \
	}

// Deferred console command registration to prevent startup crashes
class FWorldGenConsoleCommands
{
public:
	static void RegisterCommands();
	static void UnregisterCommands();
	
private:
	static TArray<IConsoleObject*> RegisteredCommands;
	static bool bCommandsRegistered;
};

// Settings management commands
static FAutoConsoleCommand WorldGenLoadSettingsCommand(
	TEXT("wg.LoadSettings"),
	TEXT("Load world generation settings from JSON file. Usage: wg.LoadSettings [ConfigPath]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		WORLDGEN_CONSOLE_SAFETY_CHECK();
		
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings instance"));
			return;
		}

		FString ConfigPath = TEXT("Config/WorldGenSettings.json");
		if (Args.Num() > 0)
		{
			ConfigPath = Args[0];
		}

		bool bSuccess = Settings->LoadFromJSON(ConfigPath);
		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("Successfully loaded WorldGen settings from %s"), *ConfigPath);
			UE_LOG(LogTemp, Log, TEXT("Seed: %llu, GenerateRadius: %d, LoadRadius: %d, ActiveRadius: %d"), 
				Settings->Settings.Seed, Settings->Settings.GenerateRadius, 
				Settings->Settings.LoadRadius, Settings->Settings.ActiveRadius);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to load WorldGen settings from %s"), *ConfigPath);
		}
	})
);

static FAutoConsoleCommand WorldGenShowSettingsCommand(
	TEXT("wg.ShowSettings"),
	TEXT("Display current world generation settings"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		WORLDGEN_CONSOLE_SAFETY_CHECK();
		
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings instance"));
			return;
		}

		const FWorldGenConfig& S = Settings->Settings;
		UE_LOG(LogTemp, Log, TEXT("=== World Generation Settings ==="));
		UE_LOG(LogTemp, Log, TEXT("Seed: %llu"), S.Seed);
		UE_LOG(LogTemp, Log, TEXT("TileSizeMeters: %.1f"), S.TileSizeMeters);
		UE_LOG(LogTemp, Log, TEXT("GenerateRadius: %d"), S.GenerateRadius);
		UE_LOG(LogTemp, Log, TEXT("LoadRadius: %d"), S.LoadRadius);
		UE_LOG(LogTemp, Log, TEXT("ActiveRadius: %d"), S.ActiveRadius);
	})
);

// VHM Debugging and Visualization Commands
static FAutoConsoleCommand WorldGenVHMShowMeshesCommand(
	TEXT("wg.VHM.ShowMeshes"),
	TEXT("Display information about active VHM components"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		WORLDGEN_CONSOLE_SAFETY_CHECK();
		
		// Find WorldGenManager in the world
		if (UWorld* World = GEngine->GetWorldFromContextObject(GEngine, EGetWorldErrorMode::LogAndReturnNull))
		{
			for (TActorIterator<AWorldGenManager> ActorItr(World); ActorItr; ++ActorItr)
			{
				AWorldGenManager* WorldGenManager = *ActorItr;
				if (WorldGenManager && WorldGenManager->GetVHMTerrainRenderer())
				{
					UVHMTerrainRenderer* VHMRenderer = WorldGenManager->GetVHMTerrainRenderer();
					TArray<FTileCoord> ActiveTiles = VHMRenderer->GetActiveMeshTiles();
					
					UE_LOG(LogTemp, Log, TEXT("=== VHM Mesh Information ==="));
					UE_LOG(LogTemp, Log, TEXT("Active VHM Components: %d"), ActiveTiles.Num());
					
					for (const FTileCoord& TileCoord : ActiveTiles)
					{
						FTerrainMeshData MeshData;
						if (VHMRenderer->GetTerrainMeshData(TileCoord, MeshData))
						{
							UE_LOG(LogTemp, Log, TEXT("Tile (%d, %d): LOD %d, Visible: %s, Bounds: %s"), 
								TileCoord.X, TileCoord.Y, MeshData.CurrentLODLevel,
								MeshData.bIsVisible ? TEXT("Yes") : TEXT("No"),
								*MeshData.WorldBounds.ToString());
						}
					}
					return;
				}
			}
		}
		
		UE_LOG(LogTemp, Warning, TEXT("VHM mesh display requires active WorldGenManager instance"));
	})
);

static FAutoConsoleCommand WorldGenVHMCreateMeshCommand(
	TEXT("wg.VHM.CreateMesh"),
	TEXT("Manually create VHM mesh for a specific tile. Usage: wg.VHM.CreateMesh <TileX> <TileY>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.VHM.CreateMesh <TileX> <TileY>"));
			return;
		}

		int32 TileX = FCString::Atoi(*Args[0]);
		int32 TileY = FCString::Atoi(*Args[1]);
		FTileCoord TileCoord(TileX, TileY);

		// Find WorldGenManager in the world
		if (UWorld* World = GEngine->GetWorldFromContextObject(GEngine, EGetWorldErrorMode::LogAndReturnNull))
		{
			for (TActorIterator<AWorldGenManager> ActorItr(World); ActorItr; ++ActorItr)
			{
				AWorldGenManager* WorldGenManager = *ActorItr;
				if (WorldGenManager && WorldGenManager->GetVHMTerrainRenderer())
				{
					UVHMTerrainRenderer* VHMRenderer = WorldGenManager->GetVHMTerrainRenderer();
					
					UE_LOG(LogTemp, Log, TEXT("Creating VHM mesh for tile (%d, %d)..."), TileX, TileY);
					bool bSuccess = VHMRenderer->CreateTerrainMeshForTile(TileCoord);
					
					if (bSuccess)
					{
						UE_LOG(LogTemp, Log, TEXT("Successfully created VHM mesh for tile (%d, %d)"), TileX, TileY);
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("Failed to create VHM mesh for tile (%d, %d)"), TileX, TileY);
					}
					return;
				}
			}
		}
		
		UE_LOG(LogTemp, Warning, TEXT("VHM mesh creation requires active WorldGenManager instance"));
	})
);

static FAutoConsoleCommand WorldGenVHMUpdateMeshCommand(
	TEXT("wg.VHM.UpdateMesh"),
	TEXT("Test real-time VHM mesh updates for a specific tile. Usage: wg.VHM.UpdateMesh <TileX> <TileY>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.VHM.UpdateMesh <TileX> <TileY>"));
			return;
		}

		int32 TileX = FCString::Atoi(*Args[0]);
		int32 TileY = FCString::Atoi(*Args[1]);
		FTileCoord TileCoord(TileX, TileY);

		// Find WorldGenManager in the world
		if (UWorld* World = GEngine->GetWorldFromContextObject(GEngine, EGetWorldErrorMode::LogAndReturnNull))
		{
			for (TActorIterator<AWorldGenManager> ActorItr(World); ActorItr; ++ActorItr)
			{
				AWorldGenManager* WorldGenManager = *ActorItr;
				if (WorldGenManager && WorldGenManager->GetVHMTerrainRenderer())
				{
					UVHMTerrainRenderer* VHMRenderer = WorldGenManager->GetVHMTerrainRenderer();
					
					// Create a test modification
					TArray<FHeightfieldModification> TestModifications;
					FHeightfieldModification TestMod;
					TestMod.Center = FVector2D(32.0f, 32.0f); // Center of 64m tile
					TestMod.Radius = 10.0f;
					TestMod.Strength = 5.0f;
					TestMod.Operation = EHeightfieldOperation::Add;
					TestModifications.Add(TestMod);
					
					UE_LOG(LogTemp, Log, TEXT("Updating VHM mesh for tile (%d, %d) with test modification..."), TileX, TileY);
					bool bSuccess = VHMRenderer->UpdateTerrainMesh(TileCoord, TestModifications);
					
					if (bSuccess)
					{
						UE_LOG(LogTemp, Log, TEXT("Successfully updated VHM mesh for tile (%d, %d)"), TileX, TileY);
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("Failed to update VHM mesh for tile (%d, %d)"), TileX, TileY);
					}
					return;
				}
			}
		}
		
		UE_LOG(LogTemp, Warning, TEXT("VHM mesh update requires active WorldGenManager instance"));
	})
);

static FAutoConsoleCommand WorldGenVHMStatsCommand(
	TEXT("wg.VHM.Stats"),
	TEXT("Display VHM performance statistics"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		// Find WorldGenManager in the world
		if (UWorld* World = GEngine->GetWorldFromContextObject(GEngine, EGetWorldErrorMode::LogAndReturnNull))
		{
			for (TActorIterator<AWorldGenManager> ActorItr(World); ActorItr; ++ActorItr)
			{
				AWorldGenManager* WorldGenManager = *ActorItr;
				if (WorldGenManager && WorldGenManager->GetVHMTerrainRenderer())
				{
					UVHMTerrainRenderer* VHMRenderer = WorldGenManager->GetVHMTerrainRenderer();
					FVHMPerformanceStats Stats = VHMRenderer->GetPerformanceStats();
					
					UE_LOG(LogTemp, Log, TEXT("=== VHM Performance Statistics ==="));
					UE_LOG(LogTemp, Log, TEXT("Active VHM Components: %d"), Stats.ActiveVHMComponents);
					UE_LOG(LogTemp, Log, TEXT("Texture Memory Usage: %.2f MB"), Stats.TextureMemoryUsageMB);
					UE_LOG(LogTemp, Log, TEXT("Average Mesh Generation: %.2f ms"), Stats.AverageMeshGenerationMs);
					UE_LOG(LogTemp, Log, TEXT("Last Mesh Generation: %.2f ms"), Stats.LastMeshGenerationMs);
					UE_LOG(LogTemp, Log, TEXT("LOD Transitions This Frame: %d"), Stats.LODTransitionsThisFrame);
					UE_LOG(LogTemp, Log, TEXT("Current FPS: %.1f"), Stats.CurrentFPS);
					UE_LOG(LogTemp, Log, TEXT("Total Meshes Created: %d"), Stats.TotalMeshesCreated);
					UE_LOG(LogTemp, Log, TEXT("Peak Generation Time: %.2f ms"), Stats.PeakGenerationTimeMs);
					UE_LOG(LogTemp, Log, TEXT("Boundary Stitching Operations: %d"), Stats.BoundaryStitchingOperations);
					return;
				}
			}
		}
		
		UE_LOG(LogTemp, Warning, TEXT("VHM statistics require active WorldGenManager instance"));
	})
);

// VHM Debug Visualization Commands
static FAutoConsoleCommand WorldGenVHMDebugBoundsCommand(
	TEXT("wg.VHM.DebugBounds"),
	TEXT("Toggle VHM component bounds visualization. Usage: wg.VHM.DebugBounds [0|1]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		bool bEnable = true;
		if (Args.Num() > 0)
		{
			bEnable = FCString::Atoi(*Args[0]) != 0;
		}

		// Find WorldGenManager and VHM debug system
		if (UWorld* World = GEngine->GetWorldFromContextObject(GEngine, EGetWorldErrorMode::LogAndReturnNull))
		{
			for (TActorIterator<AWorldGenManager> ActorItr(World); ActorItr; ++ActorItr)
			{
				AWorldGenManager* WorldGenManager = *ActorItr;
				if (WorldGenManager && WorldGenManager->GetVHMDebugSystem())
				{
					UVHMDebugSystem* DebugSystem = WorldGenManager->GetVHMDebugSystem();
					DebugSystem->SetDebugVisualizationEnabled(bEnable);
					DebugSystem->SetDebugMode(EVHMDebugMode::ComponentBounds);
					
					UE_LOG(LogTemp, Log, TEXT("VHM component bounds visualization %s"), 
						bEnable ? TEXT("enabled") : TEXT("disabled"));
					return;
				}
			}
		}
		
		UE_LOG(LogTemp, Warning, TEXT("VHM debug bounds requires active WorldGenManager with debug system"));
	})
);

static FAutoConsoleCommand WorldGenVHMWireframeCommand(
	TEXT("wg.VHM.Wireframe"),
	TEXT("Toggle VHM wireframe mode. Usage: wg.VHM.Wireframe [0|1]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		bool bEnable = true;
		if (Args.Num() > 0)
		{
			bEnable = FCString::Atoi(*Args[0]) != 0;
		}

		// Find WorldGenManager and VHM debug system
		if (UWorld* World = GEngine->GetWorldFromContextObject(GEngine, EGetWorldErrorMode::LogAndReturnNull))
		{
			for (TActorIterator<AWorldGenManager> ActorItr(World); ActorItr; ++ActorItr)
			{
				AWorldGenManager* WorldGenManager = *ActorItr;
				if (WorldGenManager && WorldGenManager->GetVHMDebugSystem())
				{
					UVHMDebugSystem* DebugSystem = WorldGenManager->GetVHMDebugSystem();
					DebugSystem->SetWireframeMode(bEnable);
					
					UE_LOG(LogTemp, Log, TEXT("VHM wireframe mode %s"), 
						bEnable ? TEXT("enabled") : TEXT("disabled"));
					return;
				}
			}
		}
		
		UE_LOG(LogTemp, Warning, TEXT("VHM wireframe mode requires active WorldGenManager with debug system"));
	})
);

static FAutoConsoleCommand WorldGenVHMExportTexturesCommand(
	TEXT("wg.VHM.ExportTextures"),
	TEXT("Export height and material textures for a tile. Usage: wg.VHM.ExportTextures <TileX> <TileY> [OutputDir]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.VHM.ExportTextures <TileX> <TileY> [OutputDir]"));
			return;
		}

		int32 TileX = FCString::Atoi(*Args[0]);
		int32 TileY = FCString::Atoi(*Args[1]);
		FTileCoord TileCoord(TileX, TileY);
		FString OutputDir = Args.Num() > 2 ? Args[2] : TEXT("Debug/VHM/");

		// Find WorldGenManager and VHM debug system
		if (UWorld* World = GEngine->GetWorldFromContextObject(GEngine, EGetWorldErrorMode::LogAndReturnNull))
		{
			for (TActorIterator<AWorldGenManager> ActorItr(World); ActorItr; ++ActorItr)
			{
				AWorldGenManager* WorldGenManager = *ActorItr;
				if (WorldGenManager && WorldGenManager->GetVHMDebugSystem())
				{
					UVHMDebugSystem* DebugSystem = WorldGenManager->GetVHMDebugSystem();
					
					UE_LOG(LogTemp, Log, TEXT("Exporting textures for tile (%d, %d) to: %s"), TileX, TileY, *OutputDir);
					bool bSuccess = DebugSystem->ExportAllTexturesForTile(TileCoord, OutputDir);
					
					if (bSuccess)
					{
						UE_LOG(LogTemp, Log, TEXT("Successfully exported textures for tile (%d, %d)"), TileX, TileY);
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("Failed to export textures for tile (%d, %d)"), TileX, TileY);
					}
					return;
				}
			}
		}
		
		UE_LOG(LogTemp, Warning, TEXT("VHM texture export requires active WorldGenManager with debug system"));
	})
);

// VHM Debug Console Variables
static TAutoConsoleVariable<bool> CVarVHMShowBounds(
	TEXT("wg.VHM.ShowBounds"),
	false,
	TEXT("Show VHM component bounds visualization"),
	ECVF_Default
);

static TAutoConsoleVariable<bool> CVarVHMWireframe(
	TEXT("wg.VHM.Wireframe"),
	false,
	TEXT("Enable VHM wireframe rendering mode"),
	ECVF_Default
);
#endif
#endif // Temporarily disabled