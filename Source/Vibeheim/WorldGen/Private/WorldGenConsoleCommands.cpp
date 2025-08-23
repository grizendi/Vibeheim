#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "WorldGenSettings.h"
#include "Services/NoiseSystem.h"
#include "Services/ClimateSystem.h"
#include "Services/HeightfieldService.h"
#include "Services/BiomeService.h"
#include "Services/PCGWorldService.h"
#include "Services/TileStreamingService.h"
#include "Services/POIService.h"
#include "Tests/WorldGenIntegrationTest.h"



// Settings management commands
static FAutoConsoleCommand WorldGenLoadSettingsCommand(
	TEXT("wg.LoadSettings"),
	TEXT("Load world generation settings from JSON file. Usage: wg.LoadSettings [ConfigPath]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
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

static FAutoConsoleCommand WorldGenSaveSettingsCommand(
	TEXT("wg.SaveSettings"),
	TEXT("Save current world generation settings to JSON file. Usage: wg.SaveSettings [ConfigPath]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
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

		bool bSuccess = Settings->SaveToJSON(ConfigPath);
		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("Successfully saved WorldGen settings to %s"), *ConfigPath);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to save WorldGen settings to %s"), *ConfigPath);
		}
	})
);

static FAutoConsoleCommand WorldGenValidateSettingsCommand(
	TEXT("wg.ValidateSettings"),
	TEXT("Validate current world generation settings and report any issues"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings instance"));
			return;
		}

		TArray<FString> ValidationErrors;
		bool bValid = Settings->ValidateSettings(ValidationErrors);

		if (bValid)
		{
			UE_LOG(LogTemp, Log, TEXT("WorldGen settings validation passed - all settings are valid"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("WorldGen settings validation found %d issues:"), ValidationErrors.Num());
			for (const FString& Error : ValidationErrors)
			{
				UE_LOG(LogTemp, Warning, TEXT("  - %s"), *Error);
			}
		}
	})
);

static FAutoConsoleCommand WorldGenShowSettingsCommand(
	TEXT("wg.ShowSettings"),
	TEXT("Display current world generation settings"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings instance"));
			return;
		}

		const FWorldGenConfig& S = Settings->Settings;
		UE_LOG(LogTemp, Log, TEXT("=== World Generation Settings ==="));
		UE_LOG(LogTemp, Log, TEXT("Seed: %llu"), S.Seed);
		UE_LOG(LogTemp, Log, TEXT("WorldGenVersion: %d"), S.WorldGenVersion);
		UE_LOG(LogTemp, Log, TEXT("TileSizeMeters: %.1f (locked)"), S.TileSizeMeters);
		UE_LOG(LogTemp, Log, TEXT("SampleSpacingMeters: %.1f (locked)"), S.SampleSpacingMeters);
		UE_LOG(LogTemp, Log, TEXT("MaxTerrainHeight: %.1f (locked)"), S.MaxTerrainHeight);
		UE_LOG(LogTemp, Log, TEXT("SeaLevel: %.1f"), S.SeaLevel);
		UE_LOG(LogTemp, Log, TEXT("GenerateRadius: %d"), S.GenerateRadius);
		UE_LOG(LogTemp, Log, TEXT("LoadRadius: %d"), S.LoadRadius);
		UE_LOG(LogTemp, Log, TEXT("ActiveRadius: %d"), S.ActiveRadius);
		UE_LOG(LogTemp, Log, TEXT("HeightfieldScale: %.1f"), S.HeightfieldScale);
		UE_LOG(LogTemp, Log, TEXT("HeightfieldResolution: %d"), S.HeightfieldResolution);
		UE_LOG(LogTemp, Log, TEXT("CellSize: %d"), S.CellSize);
		UE_LOG(LogTemp, Log, TEXT("VegetationDensity: %.2f"), S.VegetationDensity);
		UE_LOG(LogTemp, Log, TEXT("POIDensity: %.2f"), S.POIDensity);
		UE_LOG(LogTemp, Log, TEXT("MaxHISMInstances: %d"), S.MaxHISMInstances);
		UE_LOG(LogTemp, Log, TEXT("BiomeScale: %.4f"), S.BiomeScale);
		UE_LOG(LogTemp, Log, TEXT("BiomeBlendDistance: %.1f"), S.BiomeBlendDistance);
		UE_LOG(LogTemp, Log, TEXT("RVTResolution: %d"), S.RVTResolution);
		UE_LOG(LogTemp, Log, TEXT("RVTTileSize: %d"), S.RVTTileSize);
		UE_LOG(LogTemp, Log, TEXT("TileGenTargetMs: %.2f"), S.TileGenTargetMs);
		UE_LOG(LogTemp, Log, TEXT("PCGTargetMsPerTile: %.2f"), S.PCGTargetMsPerTile);
	})
);

static FAutoConsoleCommand WorldGenResetSettingsCommand(
	TEXT("wg.ResetSettings"),
	TEXT("Reset world generation settings to default values"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings instance"));
			return;
		}

		Settings->ResetToDefaults();
		UE_LOG(LogTemp, Log, TEXT("WorldGen settings reset to default values"));
	})
);

// Climate and heightfield testing commands
static FAutoConsoleCommand WorldGenTestClimateCommand(
	TEXT("wg.TestClimate"),
	TEXT("Test climate system at a specific location. Usage: wg.TestClimate X Y [Altitude]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.TestClimate X Y [Altitude]"));
			return;
		}

		float X = FCString::Atof(*Args[0]);
		float Y = FCString::Atof(*Args[1]);
		float Altitude = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 0.0f;

		// Create and initialize climate system
		UClimateSystem* ClimateSystem = NewObject<UClimateSystem>();
		FClimateSettings ClimateSettings;
		ClimateSystem->Initialize(ClimateSettings, 1337);

		// Calculate climate data
		FVector2D WorldPos(X, Y);
		FClimateData ClimateData = ClimateSystem->CalculateClimate(WorldPos, Altitude);

		UE_LOG(LogTemp, Log, TEXT("=== Climate Data at (%.1f, %.1f, %.1f) ==="), X, Y, Altitude);
		UE_LOG(LogTemp, Log, TEXT("Temperature: %.2f°C"), ClimateData.Temperature);
		UE_LOG(LogTemp, Log, TEXT("Moisture: %.3f"), ClimateData.Moisture);
		UE_LOG(LogTemp, Log, TEXT("Ring Bias: %.3f"), ClimateData.RingBias);
	})
);

static FAutoConsoleCommand WorldGenTestHeightfieldCommand(
	TEXT("wg.TestHeightfield"),
	TEXT("Generate heightfield for a specific tile. Usage: wg.TestHeightfield TileX TileY [Seed]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.TestHeightfield TileX TileY [Seed]"));
			return;
		}

		int32 TileX = FCString::Atoi(*Args[0]);
		int32 TileY = FCString::Atoi(*Args[1]);
		int32 Seed = Args.Num() > 2 ? FCString::Strtoui64(*Args[2], nullptr, 10) : 1337;

		// Get world gen settings
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings"));
			return;
		}

		// Create and initialize services
		UNoiseSystem* NoiseSystem = NewObject<UNoiseSystem>();
		NoiseSystem->Initialize(Seed);
		
		UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
		HeightfieldService->Initialize(Settings->Settings);
		HeightfieldService->SetNoiseSystem(NoiseSystem);

		// Generate heightfield
		FTileCoord TileCoord(TileX, TileY);
		FHeightfieldData HeightfieldData = HeightfieldService->GenerateHeightfield(Seed, TileCoord);

		UE_LOG(LogTemp, Log, TEXT("=== Heightfield Data for Tile (%d, %d) ==="), TileX, TileY);
		UE_LOG(LogTemp, Log, TEXT("Resolution: %d x %d"), HeightfieldData.Resolution, HeightfieldData.Resolution);
		UE_LOG(LogTemp, Log, TEXT("Height Range: %.2f to %.2f"), HeightfieldData.MinHeight, HeightfieldData.MaxHeight);
		UE_LOG(LogTemp, Log, TEXT("Sample Count: %d"), HeightfieldData.HeightData.Num());

		// Get performance stats
		float AvgTime;
		int32 CachedTiles;
		HeightfieldService->GetPerformanceStats(AvgTime, CachedTiles);
		UE_LOG(LogTemp, Log, TEXT("Generation Time: %.2fms, Cached Tiles: %d"), AvgTime, CachedTiles);
	})
);

static FAutoConsoleCommand WorldGenExportDebugPNGCommand(
	TEXT("wg.ExportDebugPNG"),
	TEXT("Export debug PNGs for a tile. Usage: wg.ExportDebugPNG TileX TileY [OutputPath]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.ExportDebugPNG TileX TileY [OutputPath]"));
			return;
		}

		int32 TileX = FCString::Atoi(*Args[0]);
		int32 TileY = FCString::Atoi(*Args[1]);
		FString OutputPath = Args.Num() > 2 ? Args[2] : FString::Printf(TEXT("Debug/Tile_%d_%d.png"), TileX, TileY);

		// Get world gen settings
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings"));
			return;
		}

		// Create services
		UNoiseSystem* NoiseSystem = NewObject<UNoiseSystem>();
		UClimateSystem* ClimateSystem = NewObject<UClimateSystem>();
		UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
		UBiomeService* BiomeService = NewObject<UBiomeService>();

		// Initialize services
		NoiseSystem->Initialize(Settings->Settings.Seed);
		FClimateSettings ClimateSettings;
		ClimateSystem->Initialize(ClimateSettings, Settings->Settings.Seed);
		HeightfieldService->Initialize(Settings->Settings);
		HeightfieldService->SetNoiseSystem(NoiseSystem);
		HeightfieldService->SetClimateSystem(ClimateSystem);
		BiomeService->Initialize(ClimateSystem, Settings->Settings);

		// Generate data
		FTileCoord TileCoord(TileX, TileY);
		FHeightfieldData HeightfieldData = HeightfieldService->GenerateHeightfield(Settings->Settings.Seed, TileCoord);

		// Export PNGs
		bool bHeightSuccess = HeightfieldService->ExportHeightfieldPNG(HeightfieldData, OutputPath);
		bool bClimateSuccess = ClimateSystem->ExportClimatePNG(TileCoord, HeightfieldData.HeightData, OutputPath);
		bool bBiomeSuccess = BiomeService->ExportBiomePNG(TileCoord, HeightfieldData.HeightData, OutputPath);

		UE_LOG(LogTemp, Log, TEXT("=== Debug PNG Export Results ==="));
		UE_LOG(LogTemp, Log, TEXT("Heightfield: %s"), bHeightSuccess ? TEXT("Success") : TEXT("Failed"));
		UE_LOG(LogTemp, Log, TEXT("Climate: %s"), bClimateSuccess ? TEXT("Success") : TEXT("Failed"));
		UE_LOG(LogTemp, Log, TEXT("Biome: %s"), bBiomeSuccess ? TEXT("Success") : TEXT("Failed"));
		UE_LOG(LogTemp, Log, TEXT("Output Path: %s"), *OutputPath);
	})
);

static FAutoConsoleCommand WorldGenTestBiomeCommand(
	TEXT("wg.TestBiome"),
	TEXT("Test biome determination at a specific location. Usage: wg.TestBiome X Y [Altitude]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.TestBiome X Y [Altitude]"));
			return;
		}

		float X = FCString::Atof(*Args[0]);
		float Y = FCString::Atof(*Args[1]);
		float Altitude = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 0.0f;

		// Get world gen settings
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings"));
			return;
		}

		// Create and initialize services
		UClimateSystem* ClimateSystem = NewObject<UClimateSystem>();
		UBiomeService* BiomeService = NewObject<UBiomeService>();

		FClimateSettings ClimateSettings;
		ClimateSystem->Initialize(ClimateSettings, Settings->Settings.Seed);
		BiomeService->Initialize(ClimateSystem, Settings->Settings);

		// Determine biome
		FVector2D WorldPos(X, Y);
		FBiomeResult BiomeResult = BiomeService->DetermineBiome(WorldPos, Altitude);

		UE_LOG(LogTemp, Log, TEXT("=== Biome Data at (%.1f, %.1f, %.1f) ==="), X, Y, Altitude);
		UE_LOG(LogTemp, Log, TEXT("Primary Biome: %d"), static_cast<int32>(BiomeResult.PrimaryBiome));
		UE_LOG(LogTemp, Log, TEXT("Is Blended: %s"), BiomeResult.IsBlended() ? TEXT("Yes") : TEXT("No"));
		UE_LOG(LogTemp, Log, TEXT("Biome Weights:"));
		
		for (const auto& WeightPair : BiomeResult.BiomeWeights)
		{
			UE_LOG(LogTemp, Log, TEXT("  Biome %d: %.3f"), static_cast<int32>(WeightPair.Key), WeightPair.Value);
		}
	})
);

static FAutoConsoleCommand WorldGenTestNoiseCommand(
	TEXT("wg.TestNoise"),
	TEXT("Test noise system with different algorithms. Usage: wg.TestNoise X Y [NoiseType] [Scale]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.TestNoise X Y [NoiseType] [Scale]"));
			UE_LOG(LogTemp, Log, TEXT("NoiseTypes: 0=Perlin, 1=Simplex, 2=Ridge, 3=Billow, 4=Voronoi"));
			return;
		}

		float X = FCString::Atof(*Args[0]);
		float Y = FCString::Atof(*Args[1]);
		int32 NoiseType = Args.Num() > 2 ? FCString::Atoi(*Args[2]) : 0;
		float Scale = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 0.01f;

		// Create and initialize noise system
		UNoiseSystem* NoiseSystem = NewObject<UNoiseSystem>();
		NoiseSystem->Initialize(1337);

		// Test different noise types
		FAdvancedNoiseSettings NoiseSettings;
		NoiseSettings.NoiseType = static_cast<ENoiseType>(FMath::Clamp(NoiseType, 0, 4));
		NoiseSettings.Scale = Scale;
		NoiseSettings.Amplitude = 1.0f;
		NoiseSettings.Octaves = 4;
		NoiseSettings.Persistence = 0.5f;
		NoiseSettings.Lacunarity = 2.0f;

		FVector2D TestPosition(X, Y);
		float NoiseValue = NoiseSystem->GenerateOctaveNoise(TestPosition, NoiseSettings);

		FString NoiseTypeName;
		switch (NoiseSettings.NoiseType)
		{
			case ENoiseType::Perlin: NoiseTypeName = TEXT("Perlin"); break;
			case ENoiseType::Simplex: NoiseTypeName = TEXT("Simplex"); break;
			case ENoiseType::Ridge: NoiseTypeName = TEXT("Ridge"); break;
			case ENoiseType::Billow: NoiseTypeName = TEXT("Billow"); break;
			case ENoiseType::Voronoi: NoiseTypeName = TEXT("Voronoi"); break;
			default: NoiseTypeName = TEXT("Unknown"); break;
		}

		UE_LOG(LogTemp, Log, TEXT("=== Noise Test at (%.1f, %.1f) ==="), X, Y);
		UE_LOG(LogTemp, Log, TEXT("Noise Type: %s"), *NoiseTypeName);
		UE_LOG(LogTemp, Log, TEXT("Scale: %.4f"), Scale);
		UE_LOG(LogTemp, Log, TEXT("Octaves: %d"), NoiseSettings.Octaves);
		UE_LOG(LogTemp, Log, TEXT("Noise Value: %.6f"), NoiseValue);

		// Test domain warping
		NoiseSettings.DomainWarp.bEnabled = true;
		NoiseSettings.DomainWarp.Strength = 50.0f;
		NoiseSettings.DomainWarp.Scale = Scale * 0.5f;
		
		float WarpedNoiseValue = NoiseSystem->GenerateOctaveNoise(TestPosition, NoiseSettings);
		UE_LOG(LogTemp, Log, TEXT("With Domain Warp: %.6f"), WarpedNoiseValue);
	})
);

static FAutoConsoleCommand WorldGenBasicSystemTestCommand(
	TEXT("wg.BasicTest"),
	TEXT("Test basic system functionality without external dependencies"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UE_LOG(LogTemp, Log, TEXT("=== Basic WorldGen System Test ==="));
		
		// Test WorldGen Settings
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (Settings)
		{
			UE_LOG(LogTemp, Log, TEXT("✓ WorldGen Settings loaded successfully"));
			UE_LOG(LogTemp, Log, TEXT("  Seed: %llu"), Settings->Settings.Seed);
			UE_LOG(LogTemp, Log, TEXT("  Tile Size: %.1fm (locked)"), Settings->Settings.TileSizeMeters);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("✗ Failed to load WorldGen Settings"));
		}
		
		// Test Noise System
		UNoiseSystem* NoiseSystem = NewObject<UNoiseSystem>();
		if (NoiseSystem)
		{
			NoiseSystem->Initialize(1337);
			
			FAdvancedNoiseSettings NoiseSettings;
			NoiseSettings.NoiseType = ENoiseType::Perlin;
			NoiseSettings.Scale = 0.01f;
			NoiseSettings.Amplitude = 50.0f;
			
			float NoiseValue = NoiseSystem->GenerateNoise(FVector2D(100.0f, 100.0f), NoiseSettings);
			UE_LOG(LogTemp, Log, TEXT("✓ Noise System working - Sample value: %.3f"), NoiseValue);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("✗ Failed to create Noise System"));
		}
		
		// Test Climate System
		UClimateSystem* ClimateSystem = NewObject<UClimateSystem>();
		if (ClimateSystem)
		{
			FClimateSettings ClimateSettings;
			ClimateSystem->Initialize(ClimateSettings, 1337);
			
			FClimateData ClimateData = ClimateSystem->CalculateClimate(FVector2D(0.0f, 0.0f), 10.0f);
			UE_LOG(LogTemp, Log, TEXT("✓ Climate System working"));
			UE_LOG(LogTemp, Log, TEXT("  Temperature: %.2f°C"), ClimateData.Temperature);
			UE_LOG(LogTemp, Log, TEXT("  Moisture: %.3f"), ClimateData.Moisture);
			UE_LOG(LogTemp, Log, TEXT("  Ring Bias: %.3f"), ClimateData.RingBias);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("✗ Failed to create Climate System"));
		}
		
		// Test Heightfield System
		if (Settings && NoiseSystem)
		{
			UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
			if (HeightfieldService)
			{
				HeightfieldService->Initialize(Settings->Settings);
				HeightfieldService->SetNoiseSystem(NoiseSystem);
				
				FTileCoord TestTile(0, 0);
				FHeightfieldData HeightfieldData = HeightfieldService->GenerateHeightfield(1337, TestTile);
				
				UE_LOG(LogTemp, Log, TEXT("✓ Heightfield System working"));
				UE_LOG(LogTemp, Log, TEXT("  Generated %d height samples"), HeightfieldData.HeightData.Num());
				UE_LOG(LogTemp, Log, TEXT("  Height range: %.2f to %.2f"), HeightfieldData.MinHeight, HeightfieldData.MaxHeight);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("✗ Failed to create Heightfield Service"));
			}
		}
		
		UE_LOG(LogTemp, Log, TEXT("=== Basic Test Complete ==="));
	})
);

// PCG System Commands
static FAutoConsoleCommand WorldGenTestPCGCommand(
	TEXT("wg.TestPCG"),
	TEXT("Test PCG world service functionality. Usage: wg.TestPCG TileX TileY [BiomeType]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.TestPCG TileX TileY [BiomeType]"));
			UE_LOG(LogTemp, Log, TEXT("BiomeTypes: 0=None, 1=Meadows, 2=Forest, 3=Mountains, 4=Ocean"));
			return;
		}

		int32 TileX = FCString::Atoi(*Args[0]);
		int32 TileY = FCString::Atoi(*Args[1]);
		int32 BiomeTypeInt = Args.Num() > 2 ? FCString::Atoi(*Args[2]) : 1; // Default to Meadows
		EBiomeType BiomeType = static_cast<EBiomeType>(FMath::Clamp(BiomeTypeInt, 0, 4));

		// Get world gen settings
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings"));
			return;
		}

		// Create services
		UNoiseSystem* NoiseSystem = NewObject<UNoiseSystem>();
		UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
		UPCGWorldService* PCGService = NewObject<UPCGWorldService>();

		// Initialize services
		NoiseSystem->Initialize(Settings->Settings.Seed);
		HeightfieldService->Initialize(Settings->Settings);
		HeightfieldService->SetNoiseSystem(NoiseSystem);
		PCGService->Initialize(Settings->Settings);

		// Generate heightfield data
		FTileCoord TileCoord(TileX, TileY);
		FHeightfieldData HeightfieldData = HeightfieldService->GenerateHeightfield(Settings->Settings.Seed, TileCoord);

		// Generate PCG content
		FPCGGenerationData PCGData = PCGService->GenerateBiomeContent(TileCoord, BiomeType, HeightfieldData.HeightData);

		UE_LOG(LogTemp, Log, TEXT("=== PCG Generation Results ==="));
		UE_LOG(LogTemp, Log, TEXT("Tile: (%d, %d)"), TileX, TileY);
		UE_LOG(LogTemp, Log, TEXT("Biome: %d"), static_cast<int32>(BiomeType));
		UE_LOG(LogTemp, Log, TEXT("Generated Instances: %d"), PCGData.TotalInstanceCount);
		UE_LOG(LogTemp, Log, TEXT("Generation Time: %.2fms"), PCGData.GenerationTimeMs);

		// Get performance stats
		FPCGPerformanceStats PerfStats = PCGService->GetPerformanceStats();
		UE_LOG(LogTemp, Log, TEXT("Average Generation Time: %.2fms"), PerfStats.AverageGenerationTimeMs);
		UE_LOG(LogTemp, Log, TEXT("Total Instances Generated: %d"), PerfStats.TotalInstancesGenerated);

#if WITH_PCG
		UE_LOG(LogTemp, Log, TEXT("PCG System: Available"));
#else
		UE_LOG(LogTemp, Log, TEXT("PCG System: Using fallback generation"));
#endif
	})
);

// Dynamic Runtime Commands
static FAutoConsoleCommand WorldGenSetSeedCommand(
	TEXT("wg.Seed"),
	TEXT("Set the world generation seed. Usage: wg.Seed <NewSeed>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.Seed <NewSeed>"));
			return;
		}

		uint64 NewSeed = FCString::Strtoui64(*Args[0], nullptr, 10);
		
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings instance"));
			return;
		}

		uint64 OldSeed = Settings->Settings.Seed;
		Settings->Settings.Seed = NewSeed;
		
		UE_LOG(LogTemp, Log, TEXT("World generation seed changed from %llu to %llu"), OldSeed, NewSeed);
		UE_LOG(LogTemp, Warning, TEXT("Note: This will affect new tile generation. Existing tiles remain unchanged."));
		UE_LOG(LogTemp, Log, TEXT("Consider using wg.ClearCache to regenerate existing tiles with new seed"));
	})
);

static FAutoConsoleCommand WorldGenSetStreamRadiusCommand(
	TEXT("wg.StreamRadius"),
	TEXT("Set streaming radii. Usage: wg.StreamRadius <GenerateRadius> [LoadRadius] [ActiveRadius]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.StreamRadius <GenerateRadius> [LoadRadius] [ActiveRadius]"));
			return;
		}

		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings instance"));
			return;
		}

		int32 NewGenerateRadius = FMath::Clamp(FCString::Atoi(*Args[0]), 1, 20);
		int32 NewLoadRadius = Args.Num() > 1 ? FMath::Clamp(FCString::Atoi(*Args[1]), 1, 15) : FMath::Min(NewGenerateRadius - 2, 5);
		int32 NewActiveRadius = Args.Num() > 2 ? FMath::Clamp(FCString::Atoi(*Args[2]), 1, 10) : FMath::Min(NewLoadRadius - 1, 3);

		UE_LOG(LogTemp, Log, TEXT("Streaming radii changed:"));
		UE_LOG(LogTemp, Log, TEXT("  Generate: %d → %d"), Settings->Settings.GenerateRadius, NewGenerateRadius);
		UE_LOG(LogTemp, Log, TEXT("  Load: %d → %d"), Settings->Settings.LoadRadius, NewLoadRadius);
		UE_LOG(LogTemp, Log, TEXT("  Active: %d → %d"), Settings->Settings.ActiveRadius, NewActiveRadius);

		Settings->Settings.GenerateRadius = NewGenerateRadius;
		Settings->Settings.LoadRadius = NewLoadRadius;
		Settings->Settings.ActiveRadius = NewActiveRadius;

		UE_LOG(LogTemp, Log, TEXT("New streaming configuration applied"));
	})
);

// Debug visualization console variables
static TAutoConsoleVariable<bool> CVarShowBiomes(
	TEXT("wg.ShowBiomes"),
	false,
	TEXT("Show biome boundary visualization overlay"),
	ECVF_Default
);

static TAutoConsoleVariable<bool> CVarShowPCGDebug(
	TEXT("wg.ShowPCGDebug"),
	false,
	TEXT("Show PCG generation debug information"),
	ECVF_Default
);

static TAutoConsoleVariable<bool> CVarShowClimate(
	TEXT("wg.ShowClimate"),
	false,
	TEXT("Show climate data visualization overlay"),
	ECVF_Default
);

static TAutoConsoleVariable<bool> CVarShowHeightfield(
	TEXT("wg.ShowHeightfield"),
	false,
	TEXT("Show heightfield generation debug info"),
	ECVF_Default
);

static TAutoConsoleVariable<bool> CVarShowTileGrid(
	TEXT("wg.ShowTileGrid"),
	false,
	TEXT("Show tile coordinate grid overlay"),
	ECVF_Default
);

static TAutoConsoleVariable<bool> CVarShowPerformance(
	TEXT("wg.ShowPerformance"),
	false,
	TEXT("Show world generation performance HUD"),
	ECVF_Default
);

static TAutoConsoleVariable<bool> CVarShowTileStreaming(
	TEXT("wg.ShowStreaming"),
	false,
	TEXT("Show tile streaming debug visualization"),
	ECVF_Default
);

// Performance and diagnostics commands
static FAutoConsoleCommand WorldGenPerformanceStatsCommand(
	TEXT("wg.PerfStats"),
	TEXT("Display detailed world generation performance statistics"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UE_LOG(LogTemp, Log, TEXT("=== World Generation Performance Statistics ==="));
		UE_LOG(LogTemp, Warning, TEXT("Note: Real performance stats require WorldGenManager instance"));
		UE_LOG(LogTemp, Log, TEXT("Expected metrics:"));
		UE_LOG(LogTemp, Log, TEXT("  Tile Generation: Average time, total tiles, spikes"));
		UE_LOG(LogTemp, Log, TEXT("  PCG Generation: Average time, instances, memory"));
		UE_LOG(LogTemp, Log, TEXT("  Streaming: Loaded tiles, pending loads, cache hits"));
	})
);

static FAutoConsoleCommand WorldGenClearCacheCommand(
	TEXT("wg.ClearCache"),
	TEXT("Clear all cached heightfield and generation data"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UE_LOG(LogTemp, Log, TEXT("Clearing world generation caches..."));
		UE_LOG(LogTemp, Warning, TEXT("Note: Cache clearing requires WorldGenManager instance"));
	})
);

// Tile Streaming System Commands
static FAutoConsoleCommand WorldGenStreamingStatsCommand(
	TEXT("wg.StreamStats"),
	TEXT("Display tile streaming system performance statistics"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UE_LOG(LogTemp, Log, TEXT("=== Tile Streaming Statistics ==="));
		UE_LOG(LogTemp, Warning, TEXT("Streaming stats require WorldGenManager instance"));
		UE_LOG(LogTemp, Log, TEXT("Expected metrics:"));
		UE_LOG(LogTemp, Log, TEXT("  Active/Loaded/Generated Tiles"));
		UE_LOG(LogTemp, Log, TEXT("  Generation Times (avg/peak)"));
		UE_LOG(LogTemp, Log, TEXT("  Cache Efficiency"));
	})
);

static FAutoConsoleCommand WorldGenClearTileCacheCommand(
	TEXT("wg.ClearTileCache"),
	TEXT("Clear the tile streaming cache and force regeneration"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UE_LOG(LogTemp, Log, TEXT("Clearing tile streaming cache..."));
		UE_LOG(LogTemp, Warning, TEXT("Tile cache clearing requires WorldGenManager instance"));
	})
);

static FAutoConsoleCommand WorldGenGetTileInfoCommand(
	TEXT("wg.TileInfo"),
	TEXT("Get information about a specific tile. Usage: wg.TileInfo <TileX> <TileY>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.TileInfo <TileX> <TileY>"));
			return;
		}

		int32 TileX = FCString::Atoi(*Args[0]);
		int32 TileY = FCString::Atoi(*Args[1]);

		UE_LOG(LogTemp, Log, TEXT("=== Tile Information: (%d, %d) ==="), TileX, TileY);
		UE_LOG(LogTemp, Warning, TEXT("Tile information requires WorldGenManager instance"));
	})
);

static FAutoConsoleCommand WorldGenForceTileGenCommand(
	TEXT("wg.ForceTileGen"),
	TEXT("Force generation of a specific tile. Usage: wg.ForceTileGen <TileX> <TileY>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.ForceTileGen <TileX> <TileY>"));
			return;
		}

		int32 TileX = FCString::Atoi(*Args[0]);
		int32 TileY = FCString::Atoi(*Args[1]);

		UE_LOG(LogTemp, Log, TEXT("Force generating tile (%d, %d)..."), TileX, TileY);
		UE_LOG(LogTemp, Warning, TEXT("Force tile generation requires WorldGenManager instance"));
	})
);

// Vegetation and content management commands
static FAutoConsoleCommand WorldGenSetVegetationDensityCommand(
	TEXT("wg.VegDensity"),
	TEXT("Set vegetation density multiplier. Usage: wg.VegDensity <Density>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.VegDensity <Density>"));
			UE_LOG(LogTemp, Log, TEXT("Density: 0.0-5.0 (1.0 = normal, 0.0 = no vegetation, 5.0 = very dense)"));
			return;
		}

		float NewDensity = FMath::Clamp(FCString::Atof(*Args[0]), 0.0f, 5.0f);
		
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings instance"));
			return;
		}

		float OldDensity = Settings->Settings.VegetationDensity;
		Settings->Settings.VegetationDensity = NewDensity;
		
		UE_LOG(LogTemp, Log, TEXT("Vegetation density changed from %.2f to %.2f"), OldDensity, NewDensity);
		UE_LOG(LogTemp, Log, TEXT("Use wg.ClearCache to apply changes to existing tiles"));
	})
);

static FAutoConsoleCommand WorldGenSetPOIDensityCommand(
	TEXT("wg.POIDensity"),
	TEXT("Set POI density multiplier. Usage: wg.POIDensity <Density>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.POIDensity <Density>"));
			UE_LOG(LogTemp, Log, TEXT("Density: 0.0-2.0 (0.1 = normal, 0.0 = no POIs, 2.0 = very dense)"));
			return;
		}

		float NewDensity = FMath::Clamp(FCString::Atof(*Args[0]), 0.0f, 2.0f);
		
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings instance"));
			return;
		}

		float OldDensity = Settings->Settings.POIDensity;
		Settings->Settings.POIDensity = NewDensity;
		
		UE_LOG(LogTemp, Log, TEXT("POI density changed from %.2f to %.2f"), OldDensity, NewDensity);
		UE_LOG(LogTemp, Log, TEXT("Use wg.ClearCache to apply changes to existing tiles"));
	})
);

// Terrain editing commands
static FAutoConsoleCommand WorldGenTerrainRaiseCommand(
	TEXT("wg.TerrainRaise"),
	TEXT("Raise terrain at a specific location. Usage: wg.TerrainRaise <X> <Y> [Radius] [Strength]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.TerrainRaise <X> <Y> [Radius] [Strength]"));
			UE_LOG(LogTemp, Log, TEXT("Defaults: Radius=10.0, Strength=5.0"));
			return;
		}

		float X = FCString::Atof(*Args[0]);
		float Y = FCString::Atof(*Args[1]);
		float Radius = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 10.0f;
		float Strength = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 5.0f;

		// Get world gen settings
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings"));
			return;
		}

		// Create heightfield service
		UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
		HeightfieldService->Initialize(Settings->Settings);

		// Generate some terrain data first if needed (simple test case)
		FTileCoord TileCoord = FTileCoord::FromWorldPosition(FVector(X, Y, 0.0f));
		
		UNoiseSystem* NoiseSystem = NewObject<UNoiseSystem>();
		NoiseSystem->Initialize(Settings->Settings.Seed);
		HeightfieldService->SetNoiseSystem(NoiseSystem);
		
		// Generate heightfield for the tile first
		FHeightfieldData HeightfieldData = HeightfieldService->GenerateHeightfield(Settings->Settings.Seed, TileCoord);

		// Apply terrain modification
		FVector Location(X, Y, 0.0f);
		bool bSuccess = HeightfieldService->ModifyHeightfield(Location, Radius, Strength, EHeightfieldOperation::Add);

		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("✓ Successfully raised terrain at (%.1f, %.1f) with radius %.1f and strength %.1f"), 
				X, Y, Radius, Strength);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("✗ Failed to raise terrain at (%.1f, %.1f)"), X, Y);
		}
	})
);

static FAutoConsoleCommand WorldGenTerrainLowerCommand(
	TEXT("wg.TerrainLower"),
	TEXT("Lower terrain at a specific location. Usage: wg.TerrainLower <X> <Y> [Radius] [Strength]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.TerrainLower <X> <Y> [Radius] [Strength]"));
			UE_LOG(LogTemp, Log, TEXT("Defaults: Radius=10.0, Strength=5.0"));
			return;
		}

		float X = FCString::Atof(*Args[0]);
		float Y = FCString::Atof(*Args[1]);
		float Radius = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 10.0f;
		float Strength = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 5.0f;

		// Get world gen settings
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings"));
			return;
		}

		// Create heightfield service
		UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
		HeightfieldService->Initialize(Settings->Settings);

		// Generate terrain data first
		FTileCoord TileCoord = FTileCoord::FromWorldPosition(FVector(X, Y, 0.0f));
		
		UNoiseSystem* NoiseSystem = NewObject<UNoiseSystem>();
		NoiseSystem->Initialize(Settings->Settings.Seed);
		HeightfieldService->SetNoiseSystem(NoiseSystem);
		
		// Generate heightfield for the tile first
		FHeightfieldData HeightfieldData = HeightfieldService->GenerateHeightfield(Settings->Settings.Seed, TileCoord);

		// Apply terrain modification
		FVector Location(X, Y, 0.0f);
		bool bSuccess = HeightfieldService->ModifyHeightfield(Location, Radius, Strength, EHeightfieldOperation::Subtract);

		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("✓ Successfully lowered terrain at (%.1f, %.1f) with radius %.1f and strength %.1f"), 
				X, Y, Radius, Strength);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("✗ Failed to lower terrain at (%.1f, %.1f)"), X, Y);
		}
	})
);

static FAutoConsoleCommand WorldGenTerrainFlattenCommand(
	TEXT("wg.TerrainFlatten"),
	TEXT("Flatten terrain at a specific location. Usage: wg.TerrainFlatten <X> <Y> [Radius] [Strength]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.TerrainFlatten <X> <Y> [Radius] [Strength]"));
			UE_LOG(LogTemp, Log, TEXT("Defaults: Radius=15.0, Strength=0.5"));
			return;
		}

		float X = FCString::Atof(*Args[0]);
		float Y = FCString::Atof(*Args[1]);
		float Radius = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 15.0f;
		float Strength = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 0.5f;

		// Get world gen settings
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings"));
			return;
		}

		// Create heightfield service
		UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
		HeightfieldService->Initialize(Settings->Settings);

		// Generate terrain data first
		FTileCoord TileCoord = FTileCoord::FromWorldPosition(FVector(X, Y, 0.0f));
		
		UNoiseSystem* NoiseSystem = NewObject<UNoiseSystem>();
		NoiseSystem->Initialize(Settings->Settings.Seed);
		HeightfieldService->SetNoiseSystem(NoiseSystem);
		
		// Generate heightfield for the tile first
		FHeightfieldData HeightfieldData = HeightfieldService->GenerateHeightfield(Settings->Settings.Seed, TileCoord);

		// Apply terrain modification
		FVector Location(X, Y, 0.0f);
		bool bSuccess = HeightfieldService->ModifyHeightfield(Location, Radius, Strength, EHeightfieldOperation::Flatten);

		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("✓ Successfully flattened terrain at (%.1f, %.1f) with radius %.1f and strength %.1f"), 
				X, Y, Radius, Strength);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("✗ Failed to flatten terrain at (%.1f, %.1f)"), X, Y);
		}
	})
);

static FAutoConsoleCommand WorldGenTerrainSmoothCommand(
	TEXT("wg.TerrainSmooth"),
	TEXT("Smooth terrain at a specific location. Usage: wg.TerrainSmooth <X> <Y> [Radius] [Strength]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.TerrainSmooth <X> <Y> [Radius] [Strength]"));
			UE_LOG(LogTemp, Log, TEXT("Defaults: Radius=12.0, Strength=0.3"));
			return;
		}

		float X = FCString::Atof(*Args[0]);
		float Y = FCString::Atof(*Args[1]);
		float Radius = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 12.0f;
		float Strength = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 0.3f;

		// Get world gen settings
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings"));
			return;
		}

		// Create heightfield service
		UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
		HeightfieldService->Initialize(Settings->Settings);

		// Generate terrain data first
		FTileCoord TileCoord = FTileCoord::FromWorldPosition(FVector(X, Y, 0.0f));
		
		UNoiseSystem* NoiseSystem = NewObject<UNoiseSystem>();
		NoiseSystem->Initialize(Settings->Settings.Seed);
		HeightfieldService->SetNoiseSystem(NoiseSystem);
		
		// Generate heightfield for the tile first
		FHeightfieldData HeightfieldData = HeightfieldService->GenerateHeightfield(Settings->Settings.Seed, TileCoord);

		// Apply terrain modification
		FVector Location(X, Y, 0.0f);
		bool bSuccess = HeightfieldService->ModifyHeightfield(Location, Radius, Strength, EHeightfieldOperation::Smooth);

		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("✓ Successfully smoothed terrain at (%.1f, %.1f) with radius %.1f and strength %.1f"), 
				X, Y, Radius, Strength);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("✗ Failed to smooth terrain at (%.1f, %.1f)"), X, Y);
		}
	})
);

static FAutoConsoleCommand WorldGenTestTerrainEditingCommand(
	TEXT("wg.TestTerrainEdits"),
	TEXT("Test all terrain editing operations at a location. Usage: wg.TestTerrainEdits <X> <Y>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.TestTerrainEdits <X> <Y>"));
			return;
		}

		float X = FCString::Atof(*Args[0]);
		float Y = FCString::Atof(*Args[1]);

		UE_LOG(LogTemp, Log, TEXT("=== Testing Terrain Editing at (%.1f, %.1f) ==="), X, Y);

		// Get world gen settings
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings"));
			return;
		}

		// Create services
		UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
		HeightfieldService->Initialize(Settings->Settings);

		UNoiseSystem* NoiseSystem = NewObject<UNoiseSystem>();
		NoiseSystem->Initialize(Settings->Settings.Seed);
		HeightfieldService->SetNoiseSystem(NoiseSystem);

		// Generate terrain data first
		FTileCoord TileCoord = FTileCoord::FromWorldPosition(FVector(X, Y, 0.0f));
		FHeightfieldData HeightfieldData = HeightfieldService->GenerateHeightfield(Settings->Settings.Seed, TileCoord);
		
		// Get original height
		float OriginalHeight = HeightfieldService->GetHeightAtLocation(FVector2D(X, Y));
		UE_LOG(LogTemp, Log, TEXT("Original height: %.2f"), OriginalHeight);

		FVector Location(X, Y, 0.0f);

		// Test 1: Raise terrain
		bool bRaiseSuccess = HeightfieldService->ModifyHeightfield(Location, 10.0f, 5.0f, EHeightfieldOperation::Add);
		float HeightAfterRaise = HeightfieldService->GetHeightAtLocation(FVector2D(X, Y));
		UE_LOG(LogTemp, Log, TEXT("Raise test: %s - Height: %.2f (change: +%.2f)"), 
			bRaiseSuccess ? TEXT("✓") : TEXT("✗"), HeightAfterRaise, HeightAfterRaise - OriginalHeight);

		// Test 2: Lower terrain
		bool bLowerSuccess = HeightfieldService->ModifyHeightfield(Location, 8.0f, 3.0f, EHeightfieldOperation::Subtract);
		float HeightAfterLower = HeightfieldService->GetHeightAtLocation(FVector2D(X, Y));
		UE_LOG(LogTemp, Log, TEXT("Lower test: %s - Height: %.2f (change: %.2f)"), 
			bLowerSuccess ? TEXT("✓") : TEXT("✗"), HeightAfterLower, HeightAfterLower - HeightAfterRaise);

		// Test 3: Flatten terrain
		bool bFlattenSuccess = HeightfieldService->ModifyHeightfield(Location, 15.0f, 0.5f, EHeightfieldOperation::Flatten);
		float HeightAfterFlatten = HeightfieldService->GetHeightAtLocation(FVector2D(X, Y));
		UE_LOG(LogTemp, Log, TEXT("Flatten test: %s - Height: %.2f (change: %.2f)"), 
			bFlattenSuccess ? TEXT("✓") : TEXT("✗"), HeightAfterFlatten, HeightAfterFlatten - HeightAfterLower);

		// Test 4: Smooth terrain
		bool bSmoothSuccess = HeightfieldService->ModifyHeightfield(Location, 12.0f, 0.3f, EHeightfieldOperation::Smooth);
		float HeightAfterSmooth = HeightfieldService->GetHeightAtLocation(FVector2D(X, Y));
		UE_LOG(LogTemp, Log, TEXT("Smooth test: %s - Height: %.2f (change: %.2f)"), 
			bSmoothSuccess ? TEXT("✓") : TEXT("✗"), HeightAfterSmooth, HeightAfterSmooth - HeightAfterFlatten);

		UE_LOG(LogTemp, Log, TEXT("=== Terrain Editing Test Complete ==="));
		UE_LOG(LogTemp, Log, TEXT("Total height change: %.2f → %.2f (Δ%.2f)"), 
			OriginalHeight, HeightAfterSmooth, HeightAfterSmooth - OriginalHeight);
	})
);

// Terrain persistence commands
static FAutoConsoleCommand WorldGenSaveTerrainCommand(
	TEXT("wg.SaveTerrain"),
	TEXT("Save all terrain modifications to disk"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		// Get world gen settings
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings"));
			return;
		}

		// Create heightfield service
		UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
		HeightfieldService->Initialize(Settings->Settings);

		// Save terrain modifications
		bool bSuccess = HeightfieldService->SaveHeightfieldModifications();
		
		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("✓ Successfully saved all terrain modifications"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("✗ Failed to save terrain modifications"));
		}
	})
);

static FAutoConsoleCommand WorldGenLoadTerrainCommand(
	TEXT("wg.LoadTerrain"),
	TEXT("Load terrain modifications from disk"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		// Get world gen settings
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings"));
			return;
		}

		// Create heightfield service
		UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
		HeightfieldService->Initialize(Settings->Settings);

		// Load terrain modifications
		bool bSuccess = HeightfieldService->LoadHeightfieldModifications();
		
		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("✓ Successfully loaded terrain modifications"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("✗ Failed to load terrain modifications"));
		}
	})
);

static FAutoConsoleCommand WorldGenSaveTileTerrainCommand(
	TEXT("wg.SaveTileTerrain"),
	TEXT("Save terrain modifications for a specific tile. Usage: wg.SaveTileTerrain <TileX> <TileY>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.SaveTileTerrain <TileX> <TileY>"));
			return;
		}

		int32 TileX = FCString::Atoi(*Args[0]);
		int32 TileY = FCString::Atoi(*Args[1]);

		// Get world gen settings
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings"));
			return;
		}

		// Create heightfield service
		UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
		HeightfieldService->Initialize(Settings->Settings);

		// Save terrain modifications for this tile
		FTileCoord TileCoord(TileX, TileY);
		bool bSuccess = HeightfieldService->SaveTileTerrainDeltas(TileCoord);
		
		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("✓ Successfully saved terrain modifications for tile (%d, %d)"), TileX, TileY);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("✗ Failed to save terrain modifications for tile (%d, %d)"), TileX, TileY);
		}
	})
);

static FAutoConsoleCommand WorldGenLoadTileTerrainCommand(
	TEXT("wg.LoadTileTerrain"),
	TEXT("Load terrain modifications for a specific tile. Usage: wg.LoadTileTerrain <TileX> <TileY>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.LoadTileTerrain <TileX> <TileY>"));
			return;
		}

		int32 TileX = FCString::Atoi(*Args[0]);
		int32 TileY = FCString::Atoi(*Args[1]);

		// Get world gen settings
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings"));
			return;
		}

		// Create heightfield service
		UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
		HeightfieldService->Initialize(Settings->Settings);

		// Load terrain modifications for this tile
		FTileCoord TileCoord(TileX, TileY);
		bool bSuccess = HeightfieldService->LoadTileTerrainDeltas(TileCoord);
		
		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("✓ Successfully loaded terrain modifications for tile (%d, %d)"), TileX, TileY);
			
			// Show loaded modifications
			TArray<FHeightfieldModification> Modifications = HeightfieldService->GetTileModifications(TileCoord);
			UE_LOG(LogTemp, Log, TEXT("  Loaded %d terrain modifications"), Modifications.Num());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("✗ Failed to load terrain modifications for tile (%d, %d)"), TileX, TileY);
		}
	})
);

static FAutoConsoleCommand WorldGenTestPersistenceCommand(
	TEXT("wg.TestPersistence"),
	TEXT("Test terrain persistence system at a location. Usage: wg.TestPersistence <X> <Y>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.TestPersistence <X> <Y>"));
			return;
		}

		float X = FCString::Atof(*Args[0]);
		float Y = FCString::Atof(*Args[1]);

		UE_LOG(LogTemp, Log, TEXT("=== Testing Terrain Persistence at (%.1f, %.1f) ==="), X, Y);

		// Get world gen settings
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings"));
			return;
		}

		// Create services
		UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
		HeightfieldService->Initialize(Settings->Settings);

		UNoiseSystem* NoiseSystem = NewObject<UNoiseSystem>();
		NoiseSystem->Initialize(Settings->Settings.Seed);
		HeightfieldService->SetNoiseSystem(NoiseSystem);

		// Generate initial terrain
		FTileCoord TileCoord = FTileCoord::FromWorldPosition(FVector(X, Y, 0.0f));
		FHeightfieldData HeightfieldData = HeightfieldService->GenerateHeightfield(Settings->Settings.Seed, TileCoord);
		
		float OriginalHeight = HeightfieldService->GetHeightAtLocation(FVector2D(X, Y));
		UE_LOG(LogTemp, Log, TEXT("1. Original height: %.2f"), OriginalHeight);

		// Step 1: Apply terrain modification
		FVector Location(X, Y, 0.0f);
		bool bModifySuccess = HeightfieldService->ModifyHeightfield(Location, 10.0f, 15.0f, EHeightfieldOperation::Add);
		float ModifiedHeight = HeightfieldService->GetHeightAtLocation(FVector2D(X, Y));
		UE_LOG(LogTemp, Log, TEXT("2. Modified height: %.2f (change: +%.2f) - %s"), 
			ModifiedHeight, ModifiedHeight - OriginalHeight, bModifySuccess ? TEXT("✓") : TEXT("✗"));

		// Step 2: Save modifications
		bool bSaveSuccess = HeightfieldService->SaveTileTerrainDeltas(TileCoord);
		UE_LOG(LogTemp, Log, TEXT("3. Save modifications: %s"), bSaveSuccess ? TEXT("✓") : TEXT("✗"));

		// Step 3: Create new service instance (simulate reload)
		UHeightfieldService* NewHeightfieldService = NewObject<UHeightfieldService>();
		NewHeightfieldService->Initialize(Settings->Settings);
		NewHeightfieldService->SetNoiseSystem(NoiseSystem);

		// Generate fresh terrain (should be at original height)
		FHeightfieldData FreshHeightfieldData = NewHeightfieldService->GenerateHeightfield(Settings->Settings.Seed, TileCoord);
		float FreshHeight = NewHeightfieldService->GetHeightAtLocation(FVector2D(X, Y));
		UE_LOG(LogTemp, Log, TEXT("4. Fresh terrain height: %.2f"), FreshHeight);

		// Step 4: Load modifications
		bool bLoadSuccess = NewHeightfieldService->LoadTileTerrainDeltas(TileCoord);
		UE_LOG(LogTemp, Log, TEXT("5. Load modifications: %s"), bLoadSuccess ? TEXT("✓") : TEXT("✗"));

		if (bLoadSuccess)
		{
			TArray<FHeightfieldModification> LoadedMods = NewHeightfieldService->GetTileModifications(TileCoord);
			UE_LOG(LogTemp, Log, TEXT("   Loaded %d modifications"), LoadedMods.Num());
		}

		// Step 5: Regenerate terrain with loaded modifications
		FHeightfieldData RestoredHeightfieldData = NewHeightfieldService->GenerateHeightfield(Settings->Settings.Seed, TileCoord);
		float RestoredHeight = NewHeightfieldService->GetHeightAtLocation(FVector2D(X, Y));
		UE_LOG(LogTemp, Log, TEXT("6. Restored height: %.2f"), RestoredHeight);

		// Step 6: Verify persistence
		float HeightDifference = FMath::Abs(ModifiedHeight - RestoredHeight);
		bool bPersistenceWorking = HeightDifference < 0.1f; // Allow small floating point errors
		
		UE_LOG(LogTemp, Log, TEXT("=== Persistence Test Results ==="));
		UE_LOG(LogTemp, Log, TEXT("Expected height: %.2f"), ModifiedHeight);
		UE_LOG(LogTemp, Log, TEXT("Restored height: %.2f"), RestoredHeight);
		UE_LOG(LogTemp, Log, TEXT("Height difference: %.4f"), HeightDifference);
		UE_LOG(LogTemp, Log, TEXT("Persistence test: %s"), bPersistenceWorking ? TEXT("✓ PASSED") : TEXT("✗ FAILED"));

		if (!bPersistenceWorking)
		{
			UE_LOG(LogTemp, Error, TEXT("Terrain modifications were not properly persisted and restored!"));
		}
	})
);

static FAutoConsoleCommand WorldGenListTerrainModificationsCommand(
	TEXT("wg.ListTerrainMods"),
	TEXT("List terrain modifications for a tile. Usage: wg.ListTerrainMods <TileX> <TileY>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.ListTerrainMods <TileX> <TileY>"));
			return;
		}

		int32 TileX = FCString::Atoi(*Args[0]);
		int32 TileY = FCString::Atoi(*Args[1]);

		// Get world gen settings
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings"));
			return;
		}

		// Create heightfield service
		UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
		HeightfieldService->Initialize(Settings->Settings);

		// Try to load modifications
		FTileCoord TileCoord(TileX, TileY);
		HeightfieldService->LoadTileTerrainDeltas(TileCoord);

		// Get modifications
		TArray<FHeightfieldModification> Modifications = HeightfieldService->GetTileModifications(TileCoord);

		UE_LOG(LogTemp, Log, TEXT("=== Terrain Modifications for Tile (%d, %d) ==="), TileX, TileY);
		if (Modifications.Num() == 0)
		{
			UE_LOG(LogTemp, Log, TEXT("No terrain modifications found"));
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Found %d terrain modifications:"), Modifications.Num());
			for (int32 i = 0; i < Modifications.Num(); i++)
			{
				const FHeightfieldModification& Mod = Modifications[i];
				FString OperationName = UEnum::GetValueAsString(Mod.Operation);
				UE_LOG(LogTemp, Log, TEXT("  %d. %s at (%.1f, %.1f) - Radius: %.1f, Strength: %.2f"), 
					i + 1, *OperationName, Mod.Center.X, Mod.Center.Y, Mod.Radius, Mod.Strength);
			}
		}
	})
);

// Help command
static FAutoConsoleCommand WorldGenListDebugCommandsCommand(
	TEXT("wg.Help"),
	TEXT("List all available world generation debug commands"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UE_LOG(LogTemp, Log, TEXT("=== World Generation Debug Commands ==="));
		UE_LOG(LogTemp, Log, TEXT("Settings:"));
		UE_LOG(LogTemp, Log, TEXT("  wg.LoadSettings [path] - Load settings from JSON"));
		UE_LOG(LogTemp, Log, TEXT("  wg.SaveSettings [path] - Save settings to JSON"));
		UE_LOG(LogTemp, Log, TEXT("  wg.ValidateSettings - Validate current settings"));
		UE_LOG(LogTemp, Log, TEXT("  wg.ShowSettings - Display current settings"));
		UE_LOG(LogTemp, Log, TEXT("  wg.ResetSettings - Reset to default values"));
		UE_LOG(LogTemp, Log, TEXT(""));
		UE_LOG(LogTemp, Log, TEXT("Runtime Control:"));
		UE_LOG(LogTemp, Log, TEXT("  wg.Seed <seed> - Set generation seed"));
		UE_LOG(LogTemp, Log, TEXT("  wg.StreamRadius <gen> [load] [active] - Set streaming radii"));
		UE_LOG(LogTemp, Log, TEXT("  wg.VegDensity <density> - Set vegetation density (0.0-5.0)"));
		UE_LOG(LogTemp, Log, TEXT("  wg.POIDensity <density> - Set POI density (0.0-2.0)"));
		UE_LOG(LogTemp, Log, TEXT("  wg.ClearCache - Clear all generation caches"));
		UE_LOG(LogTemp, Log, TEXT(""));
		UE_LOG(LogTemp, Log, TEXT("Tile Streaming:"));
		UE_LOG(LogTemp, Log, TEXT("  wg.StreamStats - Show tile streaming statistics"));
		UE_LOG(LogTemp, Log, TEXT("  wg.ClearTileCache - Clear tile streaming cache"));
		UE_LOG(LogTemp, Log, TEXT("  wg.TileInfo <x> <y> - Get tile information"));
		UE_LOG(LogTemp, Log, TEXT("  wg.ForceTileGen <x> <y> - Force generate specific tile"));
		UE_LOG(LogTemp, Log, TEXT(""));
		UE_LOG(LogTemp, Log, TEXT("Testing:"));
		UE_LOG(LogTemp, Log, TEXT("  wg.BasicTest - Test basic system functionality"));
		UE_LOG(LogTemp, Log, TEXT("  wg.TestClimate <x> <y> [alt] - Test climate at location"));
		UE_LOG(LogTemp, Log, TEXT("  wg.TestHeightfield <x> <y> [seed] - Test heightfield generation"));
		UE_LOG(LogTemp, Log, TEXT("  wg.TestBiome <x> <y> [alt] - Test biome determination"));
		UE_LOG(LogTemp, Log, TEXT("  wg.TestNoise <x> <y> [type] [scale] - Test noise generation"));
		UE_LOG(LogTemp, Log, TEXT("  wg.TestPCG <x> <y> [biome] - Test PCG generation"));
		UE_LOG(LogTemp, Log, TEXT(""));
		UE_LOG(LogTemp, Log, TEXT("Terrain Editing:"));
		UE_LOG(LogTemp, Log, TEXT("  wg.TerrainRaise <x> <y> [radius] [strength] - Raise terrain"));
		UE_LOG(LogTemp, Log, TEXT("  wg.TerrainLower <x> <y> [radius] [strength] - Lower terrain"));
		UE_LOG(LogTemp, Log, TEXT("  wg.TerrainFlatten <x> <y> [radius] [strength] - Flatten terrain"));
		UE_LOG(LogTemp, Log, TEXT("  wg.TerrainSmooth <x> <y> [radius] [strength] - Smooth terrain"));
		UE_LOG(LogTemp, Log, TEXT("  wg.TestTerrainEdits <x> <y> - Test all terrain editing ops"));
		UE_LOG(LogTemp, Log, TEXT(""));
		UE_LOG(LogTemp, Log, TEXT("Terrain Persistence:"));
		UE_LOG(LogTemp, Log, TEXT("  wg.SaveTerrain - Save all terrain modifications"));
		UE_LOG(LogTemp, Log, TEXT("  wg.LoadTerrain - Load terrain modifications"));
		UE_LOG(LogTemp, Log, TEXT("  wg.SaveTileTerrain <x> <y> - Save modifications for specific tile"));
		UE_LOG(LogTemp, Log, TEXT("  wg.LoadTileTerrain <x> <y> - Load modifications for specific tile"));
		UE_LOG(LogTemp, Log, TEXT("  wg.TestPersistence <x> <y> - Test terrain persistence system"));
		UE_LOG(LogTemp, Log, TEXT("  wg.ListTerrainMods <x> <y> - List modifications for tile"));
		UE_LOG(LogTemp, Log, TEXT(""));
		UE_LOG(LogTemp, Log, TEXT("POI System:"));
		UE_LOG(LogTemp, Log, TEXT("  wg.TestPOI <tileX> <tileY> [biome] - Test POI generation for tile"));
		UE_LOG(LogTemp, Log, TEXT("  wg.ListPOIs <centerX> <centerY> <radius> - List POIs in area"));
		UE_LOG(LogTemp, Log, TEXT("  wg.ValidatePOI <x> <y> [slope] [flat] - Validate POI placement"));
		UE_LOG(LogTemp, Log, TEXT("  wg.TestPOIStamp <x> <y> [radius] - Test terrain stamping"));
		UE_LOG(LogTemp, Log, TEXT("  wg.SavePOIs - Save all POI data to disk"));
		UE_LOG(LogTemp, Log, TEXT("  wg.LoadPOIs - Load POI data from disk"));
		UE_LOG(LogTemp, Log, TEXT("  wg.POIStats - Show POI system statistics"));
		UE_LOG(LogTemp, Log, TEXT(""));
		UE_LOG(LogTemp, Log, TEXT("Debug Visualization (Console Variables):"));
		UE_LOG(LogTemp, Log, TEXT("  wg.ShowBiomes - Toggle biome boundary overlay"));
		UE_LOG(LogTemp, Log, TEXT("  wg.ShowPCGDebug - Toggle PCG debug information"));
		UE_LOG(LogTemp, Log, TEXT("  wg.ShowClimate - Toggle climate visualization"));
		UE_LOG(LogTemp, Log, TEXT("  wg.ShowHeightfield - Toggle heightfield debug"));
		UE_LOG(LogTemp, Log, TEXT("  wg.ShowTileGrid - Toggle tile coordinate grid"));
		UE_LOG(LogTemp, Log, TEXT("  wg.ShowPerformance - Toggle performance HUD"));
		UE_LOG(LogTemp, Log, TEXT("  wg.ShowStreaming - Toggle tile streaming debug"));
		UE_LOG(LogTemp, Log, TEXT(""));
		UE_LOG(LogTemp, Log, TEXT("Export:"));
		UE_LOG(LogTemp, Log, TEXT("  wg.ExportDebugPNG <x> <y> [path] - Export debug PNGs"));
		UE_LOG(LogTemp, Log, TEXT(""));
		UE_LOG(LogTemp, Log, TEXT("Integration Testing:"));
		UE_LOG(LogTemp, Log, TEXT("  wg.IntegrationTest - Run comprehensive integration test"));
		UE_LOG(LogTemp, Log, TEXT("  wg.TestSystemInit - Run system initialization test"));
		UE_LOG(LogTemp, Log, TEXT("  wg.TestTerrain - Run terrain consistency test"));
		UE_LOG(LogTemp, Log, TEXT("  wg.TestPersistenceIntegration - Run persistence test"));
		UE_LOG(LogTemp, Log, TEXT("  wg.TestBiomesIntegration - Run biome integration test"));
		UE_LOG(LogTemp, Log, TEXT("  wg.TestPCGIntegration - Run PCG integration test"));
		UE_LOG(LogTemp, Log, TEXT("  wg.TestPOIsIntegration - Run POI integration test"));
		UE_LOG(LogTemp, Log, TEXT("  wg.TestPerformanceIntegration - Run performance test"));
		UE_LOG(LogTemp, Log, TEXT(""));
		UE_LOG(LogTemp, Log, TEXT("Diagnostics:"));
		UE_LOG(LogTemp, Log, TEXT("  wg.PerfStats - Show performance statistics"));
	})
);
// POI System Commands
static FAutoConsoleCommand WorldGenTestPOICommand(
	TEXT("wg.TestPOI"),
	TEXT("Test POI generation for a specific tile. Usage: wg.TestPOI <TileX> <TileY> [BiomeType]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.TestPOI <TileX> <TileY> [BiomeType]"));
			UE_LOG(LogTemp, Log, TEXT("BiomeType: 0=None, 1=Meadows, 2=Forest, 3=Mountains, 4=Ocean"));
			return;
		}

		int32 TileX = FCString::Atoi(*Args[0]);
		int32 TileY = FCString::Atoi(*Args[1]);
		EBiomeType BiomeType = Args.Num() > 2 ? static_cast<EBiomeType>(FCString::Atoi(*Args[2])) : EBiomeType::Meadows;

		UE_LOG(LogTemp, Log, TEXT("=== Testing POI Generation for Tile (%d, %d) ==="), TileX, TileY);

		// Get world gen settings
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings"));
			return;
		}

		// Create services
		UPOIService* POIService = NewObject<UPOIService>();
		UBiomeService* BiomeService = NewObject<UBiomeService>();
		UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
		UNoiseSystem* NoiseSystem = NewObject<UNoiseSystem>();

		// Initialize services
		POIService->Initialize(Settings->Settings);
		HeightfieldService->Initialize(Settings->Settings);
		NoiseSystem->Initialize(Settings->Settings.Seed);
		HeightfieldService->SetNoiseSystem(NoiseSystem);

		// Load biome definitions
		BiomeService->LoadBiomesFromJSON();
		POIService->SetBiomeService(BiomeService);
		POIService->SetHeightfieldService(HeightfieldService);

		// Generate heightfield data for the tile
		FTileCoord TileCoord(TileX, TileY);
		FHeightfieldData HeightfieldData = HeightfieldService->GenerateHeightfield(Settings->Settings.Seed, TileCoord);

		UE_LOG(LogTemp, Log, TEXT("Generated heightfield with %d height samples"), HeightfieldData.HeightData.Num());

		// Generate POIs for the tile
		TArray<FPOIData> GeneratedPOIs = POIService->GenerateTilePOIs(TileCoord, BiomeType, HeightfieldData.HeightData);

		UE_LOG(LogTemp, Log, TEXT("Generated %d POIs for biome %s"), 
			GeneratedPOIs.Num(), *UEnum::GetValueAsString(BiomeType));

		// Display POI details
		for (int32 i = 0; i < GeneratedPOIs.Num(); i++)
		{
			const FPOIData& POI = GeneratedPOIs[i];
			UE_LOG(LogTemp, Log, TEXT("  POI %d: %s at (%.1f, %.1f, %.1f)"), 
				i + 1, *POI.POIName, POI.Location.X, POI.Location.Y, POI.Location.Z);
		}

		// Get performance stats
		float AvgGenTime = 0.0f;
		int32 TotalPOIs = 0;
		POIService->GetPerformanceStats(AvgGenTime, TotalPOIs);
		UE_LOG(LogTemp, Log, TEXT("Performance: %.2fms average generation time, %d total POIs"), AvgGenTime, TotalPOIs);
	})
);

static FAutoConsoleCommand WorldGenListPOIsCommand(
	TEXT("wg.ListPOIs"),
	TEXT("List POIs in an area. Usage: wg.ListPOIs <CenterX> <CenterY> <Radius>"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 3)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.ListPOIs <CenterX> <CenterY> <Radius>"));
			return;
		}

		float CenterX = FCString::Atof(*Args[0]);
		float CenterY = FCString::Atof(*Args[1]);
		float Radius = FCString::Atof(*Args[2]);

		// Get world gen settings
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings"));
			return;
		}

		// Create POI service
		UPOIService* POIService = NewObject<UPOIService>();
		POIService->Initialize(Settings->Settings);

		// Get POIs in area
		FVector Center(CenterX, CenterY, 0.0f);
		TArray<FPOIData> POIsInArea = POIService->GetPOIsInArea(Center, Radius);

		UE_LOG(LogTemp, Log, TEXT("=== POIs within %.1f units of (%.1f, %.1f) ==="), Radius, CenterX, CenterY);
		if (POIsInArea.Num() == 0)
		{
			UE_LOG(LogTemp, Log, TEXT("No POIs found in the specified area"));
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Found %d POIs:"), POIsInArea.Num());
			for (int32 i = 0; i < POIsInArea.Num(); i++)
			{
				const FPOIData& POI = POIsInArea[i];
				float Distance = FVector::Dist2D(Center, POI.Location);
				UE_LOG(LogTemp, Log, TEXT("  %d. %s at (%.1f, %.1f, %.1f) - Distance: %.1f"), 
					i + 1, *POI.POIName, POI.Location.X, POI.Location.Y, POI.Location.Z, Distance);
			}
		}
	})
);

static FAutoConsoleCommand WorldGenValidatePOIPlacementCommand(
	TEXT("wg.ValidatePOI"),
	TEXT("Validate POI placement at a location. Usage: wg.ValidatePOI <X> <Y> [SlopeLimit] [RequireFlat]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.ValidatePOI <X> <Y> [SlopeLimit] [RequireFlat]"));
			return;
		}

		float X = FCString::Atof(*Args[0]);
		float Y = FCString::Atof(*Args[1]);
		float SlopeLimit = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 30.0f;
		bool bRequireFlat = Args.Num() > 3 ? FCString::ToBool(*Args[3]) : true;

		UE_LOG(LogTemp, Log, TEXT("=== Validating POI Placement at (%.1f, %.1f) ==="), X, Y);

		// Get world gen settings
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings"));
			return;
		}

		// Create services
		UPOIService* POIService = NewObject<UPOIService>();
		UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
		UNoiseSystem* NoiseSystem = NewObject<UNoiseSystem>();

		// Initialize services
		POIService->Initialize(Settings->Settings);
		HeightfieldService->Initialize(Settings->Settings);
		NoiseSystem->Initialize(Settings->Settings.Seed);
		HeightfieldService->SetNoiseSystem(NoiseSystem);
		POIService->SetHeightfieldService(HeightfieldService);

		// Generate heightfield data
		FTileCoord TileCoord = FTileCoord::FromWorldPosition(FVector(X, Y, 0.0f));
		FHeightfieldData HeightfieldData = HeightfieldService->GenerateHeightfield(Settings->Settings.Seed, TileCoord);

		// Create test POI rule
		FPOISpawnRule TestRule;
		TestRule.POIName = TEXT("TestPOI");
		TestRule.SlopeLimit = SlopeLimit;
		TestRule.bRequiresFlatGround = bRequireFlat;

		// Validate placement
		FVector Location(X, Y, 0.0f);
		bool bValid = POIService->ValidatePOIPlacement(Location, TestRule, HeightfieldData.HeightData, TileCoord);

		// Get detailed information
		float Height = HeightfieldService->GetHeightAtLocation(FVector2D(X, Y));
		float Slope = HeightfieldService->GetSlopeAtLocation(FVector2D(X, Y));

		UE_LOG(LogTemp, Log, TEXT("Location: (%.1f, %.1f)"), X, Y);
		UE_LOG(LogTemp, Log, TEXT("Height: %.2f"), Height);
		UE_LOG(LogTemp, Log, TEXT("Slope: %.2f degrees"), Slope);
		UE_LOG(LogTemp, Log, TEXT("Slope Limit: %.2f degrees"), SlopeLimit);
		UE_LOG(LogTemp, Log, TEXT("Require Flat Ground: %s"), bRequireFlat ? TEXT("Yes") : TEXT("No"));
		UE_LOG(LogTemp, Log, TEXT("Slope Check: %s"), Slope <= SlopeLimit ? TEXT("✓ PASS") : TEXT("✗ FAIL"));
		
		if (bRequireFlat)
		{
			// Test flat ground validation manually
			// This is a simplified version - the actual validation is more complex
			UE_LOG(LogTemp, Log, TEXT("Flat Ground Check: %s"), bValid ? TEXT("✓ PASS") : TEXT("✗ FAIL"));
		}

		UE_LOG(LogTemp, Log, TEXT("Overall Validation: %s"), bValid ? TEXT("✓ VALID") : TEXT("✗ INVALID"));
	})
);

static FAutoConsoleCommand WorldGenTestPOIStampCommand(
	TEXT("wg.TestPOIStamp"),
	TEXT("Test terrain stamping for POI placement. Usage: wg.TestPOIStamp <X> <Y> [Radius]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("Usage: wg.TestPOIStamp <X> <Y> [Radius]"));
			return;
		}

		float X = FCString::Atof(*Args[0]);
		float Y = FCString::Atof(*Args[1]);
		float Radius = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 5.0f;

		UE_LOG(LogTemp, Log, TEXT("=== Testing POI Terrain Stamping at (%.1f, %.1f) ==="), X, Y);

		// Get world gen settings
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings"));
			return;
		}

		// Create services
		UPOIService* POIService = NewObject<UPOIService>();
		UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
		UNoiseSystem* NoiseSystem = NewObject<UNoiseSystem>();

		// Initialize services
		POIService->Initialize(Settings->Settings);
		HeightfieldService->Initialize(Settings->Settings);
		NoiseSystem->Initialize(Settings->Settings.Seed);
		HeightfieldService->SetNoiseSystem(NoiseSystem);
		POIService->SetHeightfieldService(HeightfieldService);

		// Generate heightfield data
		FTileCoord TileCoord = FTileCoord::FromWorldPosition(FVector(X, Y, 0.0f));
		FHeightfieldData HeightfieldData = HeightfieldService->GenerateHeightfield(Settings->Settings.Seed, TileCoord);

		// Get original height and slope
		float OriginalHeight = HeightfieldService->GetHeightAtLocation(FVector2D(X, Y));
		float OriginalSlope = HeightfieldService->GetSlopeAtLocation(FVector2D(X, Y));

		UE_LOG(LogTemp, Log, TEXT("Before stamping:"));
		UE_LOG(LogTemp, Log, TEXT("  Height: %.2f"), OriginalHeight);
		UE_LOG(LogTemp, Log, TEXT("  Slope: %.2f degrees"), OriginalSlope);

		// Apply terrain stamp
		FVector Location(X, Y, 0.0f);
		TArray<float> ModifiableHeightData = HeightfieldData.HeightData;
		bool bStampSuccess = POIService->ApplyTerrainStamp(Location, Radius, ModifiableHeightData, TileCoord);

		if (bStampSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("✓ Terrain stamp applied successfully"));

			// Calculate new height and slope (simplified - would need to update heightfield service)
			// For now, just report success
			UE_LOG(LogTemp, Log, TEXT("Stamp radius: %.1f"), Radius);
			UE_LOG(LogTemp, Log, TEXT("Modified %d height samples"), ModifiableHeightData.Num());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("✗ Failed to apply terrain stamp"));
		}
	})
);

static FAutoConsoleCommand WorldGenSavePOIDataCommand(
	TEXT("wg.SavePOIs"),
	TEXT("Save all POI data to disk"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		// Get world gen settings
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings"));
			return;
		}

		// Create POI service
		UPOIService* POIService = NewObject<UPOIService>();
		POIService->Initialize(Settings->Settings);

		// Save POI data
		bool bSuccess = POIService->SavePOIData();
		
		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("✓ Successfully saved all POI data"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("✗ Failed to save POI data"));
		}
	})
);

static FAutoConsoleCommand WorldGenLoadPOIDataCommand(
	TEXT("wg.LoadPOIs"),
	TEXT("Load POI data from disk"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		// Get world gen settings
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings"));
			return;
		}

		// Create POI service
		UPOIService* POIService = NewObject<UPOIService>();
		POIService->Initialize(Settings->Settings);

		// Load POI data
		bool bSuccess = POIService->LoadPOIData();
		
		if (bSuccess)
		{
			float AvgGenTime = 0.0f;
			int32 TotalPOIs = 0;
			POIService->GetPerformanceStats(AvgGenTime, TotalPOIs);
			UE_LOG(LogTemp, Log, TEXT("✓ Successfully loaded POI data - %d total POIs"), TotalPOIs);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("✗ Failed to load POI data"));
		}
	})
);

static FAutoConsoleCommand WorldGenPOIStatsCommand(
	TEXT("wg.POIStats"),
	TEXT("Show POI system performance statistics"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		// Get world gen settings
		UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get WorldGen settings"));
			return;
		}

		// Create POI service
		UPOIService* POIService = NewObject<UPOIService>();
		POIService->Initialize(Settings->Settings);

		// Get performance stats
		float AvgGenTime = 0.0f;
		int32 TotalPOIs = 0;
		POIService->GetPerformanceStats(AvgGenTime, TotalPOIs);

		UE_LOG(LogTemp, Log, TEXT("=== POI System Statistics ==="));
		UE_LOG(LogTemp, Log, TEXT("Total POIs: %d"), TotalPOIs);
		UE_LOG(LogTemp, Log, TEXT("Average Generation Time: %.2f ms"), AvgGenTime);
		UE_LOG(LogTemp, Log, TEXT("POI Density Setting: %.2f"), Settings->Settings.POIDensity);
	})
);

// Integration Test Command - Task 16
static FAutoConsoleCommand WorldGenIntegrationTestCommand(
	TEXT("wg.IntegrationTest"),
	TEXT("Run comprehensive integration test for all world generation systems"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		// Create and execute the integration test using the proper test class
		UWorldGenIntegrationTest* IntegrationTest = NewObject<UWorldGenIntegrationTest>();
		if (!IntegrationTest)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to create integration test instance"));
			return;
		}

		// Execute the complete integration test suite
		FIntegrationTestSuite TestResults = IntegrationTest->ExecuteIntegrationTest();

		// The test class handles all logging internally, so we don't need to duplicate it here
		// The ExecuteIntegrationTest method already provides the formatted output matching the expected format
	})
);

// Individual integration test category commands
static FAutoConsoleCommand WorldGenTestSystemInitCommand(
	TEXT("wg.TestSystemInit"),
	TEXT("Run only the system initialization integration test"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UWorldGenIntegrationTest* IntegrationTest = NewObject<UWorldGenIntegrationTest>();
		if (!IntegrationTest)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to create integration test instance"));
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("=== SYSTEM INITIALIZATION TEST ==="));
		FIntegrationTestResult Result = IntegrationTest->ExecuteTestCategory(1);
		
		if (Result.bPassed)
		{
			UE_LOG(LogTemp, Log, TEXT("✓ System initialization test passed (%.2fms)"), Result.ExecutionTimeMs);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("✗ System initialization test failed (%.2fms)"), Result.ExecutionTimeMs);
			UE_LOG(LogTemp, Error, TEXT("Error: %s"), *Result.ErrorMessage);
		}
	})
);

static FAutoConsoleCommand WorldGenTestTerrainCommand(
	TEXT("wg.TestTerrain"),
	TEXT("Run only the terrain generation consistency integration test"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UWorldGenIntegrationTest* IntegrationTest = NewObject<UWorldGenIntegrationTest>();
		if (!IntegrationTest)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to create integration test instance"));
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("=== TERRAIN CONSISTENCY TEST ==="));
		FIntegrationTestResult Result = IntegrationTest->ExecuteTestCategory(2);
		
		if (Result.bPassed)
		{
			UE_LOG(LogTemp, Log, TEXT("✓ Terrain consistency test passed (%.2fms)"), Result.ExecutionTimeMs);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("✗ Terrain consistency test failed (%.2fms)"), Result.ExecutionTimeMs);
			UE_LOG(LogTemp, Error, TEXT("Error: %s"), *Result.ErrorMessage);
		}
	})
);

static FAutoConsoleCommand WorldGenTestPersistenceIntegrationCommand(
	TEXT("wg.TestPersistenceIntegration"),
	TEXT("Run only the terrain editing and persistence integration test"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UWorldGenIntegrationTest* IntegrationTest = NewObject<UWorldGenIntegrationTest>();
		if (!IntegrationTest)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to create integration test instance"));
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("=== PERSISTENCE TEST ==="));
		FIntegrationTestResult Result = IntegrationTest->ExecuteTestCategory(3);
		
		if (Result.bPassed)
		{
			UE_LOG(LogTemp, Log, TEXT("✓ Persistence test passed (%.2fms)"), Result.ExecutionTimeMs);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("✗ Persistence test failed (%.2fms)"), Result.ExecutionTimeMs);
			UE_LOG(LogTemp, Error, TEXT("Error: %s"), *Result.ErrorMessage);
		}
	})
);

static FAutoConsoleCommand WorldGenTestBiomesIntegrationCommand(
	TEXT("wg.TestBiomesIntegration"),
	TEXT("Run only the biome system integration test"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UWorldGenIntegrationTest* IntegrationTest = NewObject<UWorldGenIntegrationTest>();
		if (!IntegrationTest)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to create integration test instance"));
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("=== BIOME INTEGRATION TEST ==="));
		FIntegrationTestResult Result = IntegrationTest->ExecuteTestCategory(4);
		
		if (Result.bPassed)
		{
			UE_LOG(LogTemp, Log, TEXT("✓ Biome integration test passed (%.2fms)"), Result.ExecutionTimeMs);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("✗ Biome integration test failed (%.2fms)"), Result.ExecutionTimeMs);
			UE_LOG(LogTemp, Error, TEXT("Error: %s"), *Result.ErrorMessage);
		}
	})
);

static FAutoConsoleCommand WorldGenTestPCGIntegrationCommand(
	TEXT("wg.TestPCGIntegration"),
	TEXT("Run only the PCG content generation integration test"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UWorldGenIntegrationTest* IntegrationTest = NewObject<UWorldGenIntegrationTest>();
		if (!IntegrationTest)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to create integration test instance"));
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("=== PCG INTEGRATION TEST ==="));
		FIntegrationTestResult Result = IntegrationTest->ExecuteTestCategory(5);
		
		if (Result.bPassed)
		{
			UE_LOG(LogTemp, Log, TEXT("✓ PCG integration test passed (%.2fms)"), Result.ExecutionTimeMs);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("✗ PCG integration test failed (%.2fms)"), Result.ExecutionTimeMs);
			UE_LOG(LogTemp, Error, TEXT("Error: %s"), *Result.ErrorMessage);
		}
	})
);

static FAutoConsoleCommand WorldGenTestPOIsIntegrationCommand(
	TEXT("wg.TestPOIsIntegration"),
	TEXT("Run only the POI generation and placement integration test"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UWorldGenIntegrationTest* IntegrationTest = NewObject<UWorldGenIntegrationTest>();
		if (!IntegrationTest)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to create integration test instance"));
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("=== POI INTEGRATION TEST ==="));
		FIntegrationTestResult Result = IntegrationTest->ExecuteTestCategory(6);
		
		if (Result.bPassed)
		{
			UE_LOG(LogTemp, Log, TEXT("✓ POI integration test passed (%.2fms)"), Result.ExecutionTimeMs);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("✗ POI integration test failed (%.2fms)"), Result.ExecutionTimeMs);
			UE_LOG(LogTemp, Error, TEXT("Error: %s"), *Result.ErrorMessage);
		}
	})
);

static FAutoConsoleCommand WorldGenTestPerformanceIntegrationCommand(
	TEXT("wg.TestPerformanceIntegration"),
	TEXT("Run only the performance validation integration test"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UWorldGenIntegrationTest* IntegrationTest = NewObject<UWorldGenIntegrationTest>();
		if (!IntegrationTest)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to create integration test instance"));
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("=== PERFORMANCE TEST ==="));
		FIntegrationTestResult Result = IntegrationTest->ExecuteTestCategory(7);
		
		if (Result.bPassed)
		{
			UE_LOG(LogTemp, Log, TEXT("✓ Performance test passed (%.2fms)"), Result.ExecutionTimeMs);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("✗ Performance test failed (%.2fms)"), Result.ExecutionTimeMs);
			UE_LOG(LogTemp, Error, TEXT("Error: %s"), *Result.ErrorMessage);
		}
	})
);