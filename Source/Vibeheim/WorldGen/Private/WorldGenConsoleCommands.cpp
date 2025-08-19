#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "WorldGenSettings.h"

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

// Climate and Heightfield System Commands
#include "Services/NoiseSystem.h"
#include "Services/ClimateSystem.h"
#include "Services/HeightfieldService.h"
#include "Services/BiomeService.h"

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
);static FAutoConsoleCommand WorldGenTestNoiseCommand(
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
);static FAutoConsoleCommand WorldGenBasicSystemTestCommand(
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
#include "Services/PCGWorldService.h"

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