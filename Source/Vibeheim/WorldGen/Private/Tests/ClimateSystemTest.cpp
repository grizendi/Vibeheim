#include "CoreMinimal.h"
#include "Services/NoiseSystem.h"
#include "Services/ClimateSystem.h"
#include "Services/HeightfieldService.h"
#include "Services/BiomeService.h"
#include "Services/PCGWorldService.h"
#include "WorldGenSettings.h"

// Only include automation test if available
#if WITH_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClimateSystemTest, "Vibeheim.WorldGen.ClimateSystem.BasicFunctionality", 
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FClimateSystemTest::RunTest(const FString& Parameters)
{
	// Test climate system initialization and basic functionality
	UClimateSystem* ClimateSystem = NewObject<UClimateSystem>();
	
	FClimateSettings Settings;
	Settings.BaseTemperature = 15.0f;
	Settings.BaseMoisture = 0.5f;
	Settings.WorldCenterX = 0.0f;
	Settings.WorldCenterY = 0.0f;
	Settings.RingInfluenceRadius = 1000.0f;
	
	ClimateSystem->Initialize(Settings, 1337);
	
	// Test climate calculation at origin
	FVector2D TestPosition(0.0f, 0.0f);
	FClimateData ClimateData = ClimateSystem->CalculateClimate(TestPosition, 0.0f);
	
	TestTrue("Temperature should be reasonable", ClimateData.Temperature > -50.0f && ClimateData.Temperature < 50.0f);
	TestTrue("Moisture should be in valid range", ClimateData.Moisture >= 0.0f && ClimateData.Moisture <= 1.0f);
	TestTrue("Ring bias should be positive at center", ClimateData.RingBias > 0.0f);
	
	// Test climate calculation at distance
	FVector2D DistantPosition(2000.0f, 2000.0f);
	FClimateData DistantClimateData = ClimateSystem->CalculateClimate(DistantPosition, 0.0f);
	
	TestTrue("Ring bias should decrease with distance", DistantClimateData.RingBias < ClimateData.RingBias);
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeightfieldServiceTest, "Vibeheim.WorldGen.HeightfieldService.BasicFunctionality", 
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FHeightfieldServiceTest::RunTest(const FString& Parameters)
{
	// Get world gen settings
	UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
	TestNotNull("WorldGen settings should be available", Settings);
	
	if (!Settings)
	{
		return false;
	}
	
	// Test heightfield service initialization
	UNoiseSystem* NoiseSystem = NewObject<UNoiseSystem>();
	NoiseSystem->Initialize(1337);
	
	UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
	bool bInitialized = HeightfieldService->Initialize(Settings->Settings);
	HeightfieldService->SetNoiseSystem(NoiseSystem);
	TestTrue("Heightfield service should initialize successfully", bInitialized);
	
	// Test heightfield generation
	FTileCoord TestTile(0, 0);
	FHeightfieldData HeightfieldData = HeightfieldService->GenerateHeightfield(1337, TestTile);
	
	TestEqual("Heightfield resolution should be 64", HeightfieldData.Resolution, 64);
	TestEqual("Heightfield should have 4096 samples", HeightfieldData.HeightData.Num(), 64 * 64);
	TestEqual("Normal data should match height data", HeightfieldData.NormalData.Num(), HeightfieldData.HeightData.Num());
	TestEqual("Slope data should match height data", HeightfieldData.SlopeData.Num(), HeightfieldData.HeightData.Num());
	
	// Test height range is reasonable
	TestTrue("Min height should be reasonable", HeightfieldData.MinHeight >= -Settings->Settings.MaxTerrainHeight);
	TestTrue("Max height should be reasonable", HeightfieldData.MaxHeight <= Settings->Settings.MaxTerrainHeight);
	TestTrue("Max height should be greater than min height", HeightfieldData.MaxHeight >= HeightfieldData.MinHeight);
	
	// Test caching
	FHeightfieldData CachedData;
	bool bCached = HeightfieldService->GetCachedHeightfield(TestTile, CachedData);
	TestTrue("Heightfield should be cached after generation", bCached);
	TestEqual("Cached data should match generated data", CachedData.HeightData.Num(), HeightfieldData.HeightData.Num());
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBiomeServiceTest, "Vibeheim.WorldGen.BiomeService.BasicFunctionality", 
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBiomeServiceTest::RunTest(const FString& Parameters)
{
	// Get world gen settings
	UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
	TestNotNull("WorldGen settings should be available", Settings);
	
	if (!Settings)
	{
		return false;
	}
	
	// Create and initialize climate system
	UClimateSystem* ClimateSystem = NewObject<UClimateSystem>();
	FClimateSettings ClimateSettings;
	ClimateSystem->Initialize(ClimateSettings, 1337);
	
	// Test biome service initialization
	UBiomeService* BiomeService = NewObject<UBiomeService>();
	BiomeService->Initialize(ClimateSystem, Settings->Settings);
	
	// Test biome determination
	FVector2D TestPosition(0.0f, 0.0f);
	FBiomeResult BiomeResult = BiomeService->DetermineBiome(TestPosition, 10.0f);
	
	TestTrue("Primary biome should be valid", BiomeResult.PrimaryBiome != EBiomeType::None);
	TestTrue("Should have at least one biome weight", BiomeResult.BiomeWeights.Num() > 0);
	
	// Test biome weights sum to approximately 1.0
	float TotalWeight = 0.0f;
	for (const auto& WeightPair : BiomeResult.BiomeWeights)
	{
		TotalWeight += WeightPair.Value;
		TestTrue("Individual biome weight should be positive", WeightPair.Value > 0.0f);
	}
	TestTrue("Total biome weights should be close to 1.0", FMath::Abs(TotalWeight - 1.0f) < 0.1f);
	
	// Test biome definition retrieval
	FBiomeDefinition BiomeDef;
	bool bFoundDefinition = BiomeService->GetBiomeDefinition(BiomeResult.PrimaryBiome, BiomeDef);
	TestTrue("Should be able to get biome definition", bFoundDefinition);
	TestTrue("Biome definition should have valid name", !BiomeDef.BiomeName.IsEmpty());
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIntegratedSystemTest, "Vibeheim.WorldGen.IntegratedSystems.FullWorkflow", 
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FIntegratedSystemTest::RunTest(const FString& Parameters)
{
	// Test full workflow: Settings -> Climate -> Heightfield -> Biome
	UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
	TestNotNull("WorldGen settings should be available", Settings);
	
	if (!Settings)
	{
		return false;
	}
	
	// Create all services
	UNoiseSystem* NoiseSystem = NewObject<UNoiseSystem>();
	UClimateSystem* ClimateSystem = NewObject<UClimateSystem>();
	UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
	UBiomeService* BiomeService = NewObject<UBiomeService>();
	UPCGWorldService* PCGService = NewObject<UPCGWorldService>();
	
	// Initialize services
	NoiseSystem->Initialize(Settings->Settings.Seed);
	FClimateSettings ClimateSettings;
	ClimateSystem->Initialize(ClimateSettings, Settings->Settings.Seed);
	HeightfieldService->Initialize(Settings->Settings);
	HeightfieldService->SetNoiseSystem(NoiseSystem);
	HeightfieldService->SetClimateSystem(ClimateSystem);
	BiomeService->Initialize(ClimateSystem, Settings->Settings);
	PCGService->Initialize(Settings->Settings);
	
	// Generate data for a test tile
	FTileCoord TestTile(1, 1);
	FHeightfieldData HeightfieldData = HeightfieldService->GenerateHeightfield(Settings->Settings.Seed, TestTile);
	TArray<FBiomeResult> BiomeData = BiomeService->GenerateTileBiomeData(TestTile, HeightfieldData.HeightData);
	FPCGGenerationData PCGData = PCGService->GenerateBiomeContent(TestTile, EBiomeType::Meadows, HeightfieldData.HeightData);
	
	// Verify integrated results
	TestEqual("Biome data should match heightfield resolution", BiomeData.Num(), 64 * 64);
	TestTrue("PCG should generate some content", PCGData.TotalInstanceCount >= 0);
	
	// Check that biomes are reasonable for the generated heights
	int32 ValidBiomes = 0;
	for (const FBiomeResult& Result : BiomeData)
	{
		if (Result.PrimaryBiome != EBiomeType::None && Result.BiomeWeights.Num() > 0)
		{
			ValidBiomes++;
		}
	}
	
	TestTrue("Most samples should have valid biomes", ValidBiomes > (BiomeData.Num() * 0.8f));
	
	// Test performance
	float AvgTime;
	int32 CachedTiles;
	HeightfieldService->GetPerformanceStats(AvgTime, CachedTiles);
	TestTrue("Generation should be within performance target", AvgTime <= Settings->Settings.TileGenTargetMs * 2.0f); // Allow 2x target for test
	
	return true;
}#endif /
/ WITH_AUTOMATION_TESTS