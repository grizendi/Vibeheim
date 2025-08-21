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
		UE_LOG(LogTemp, Log, TEXT("Diagnostics:"));
		UE_LOG(LogTemp, Log, TEXT("  wg.PerfStats - Show performance statistics"));
	})
);
