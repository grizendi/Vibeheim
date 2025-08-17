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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBasicSystemTest, "Vibeheim.WorldGen.BasicSystems.Functionality", 
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBasicSystemTest::RunTest(const FString& Parameters)
{
	// Test basic system creation and initialization
	UNoiseSystem* NoiseSystem = NewObject<UNoiseSystem>();
	TestNotNull("NoiseSystem should be created", NoiseSystem);
	
	if (NoiseSystem)
	{
		NoiseSystem->Initialize(1337);
		TestEqual("NoiseSystem seed should be set", NoiseSystem->GetSeed(), static_cast<uint64>(1337));
		
		// Test basic noise generation
		FAdvancedNoiseSettings NoiseSettings;
		NoiseSettings.NoiseType = ENoiseType::Perlin;
		NoiseSettings.Scale = 0.01f;
		NoiseSettings.Amplitude = 1.0f;
		
		float NoiseValue = NoiseSystem->GenerateNoise(FVector2D(100.0f, 100.0f), NoiseSettings);
		TestTrue("Noise value should be in reasonable range", NoiseValue >= -2.0f && NoiseValue <= 2.0f);
	}
	
	// Test climate system
	UClimateSystem* ClimateSystem = NewObject<UClimateSystem>();
	TestNotNull("ClimateSystem should be created", ClimateSystem);
	
	if (ClimateSystem)
	{
		FClimateSettings ClimateSettings;
		ClimateSystem->Initialize(ClimateSettings, 1337);
		
		FClimateData ClimateData = ClimateSystem->CalculateClimate(FVector2D::ZeroVector, 0.0f);
		TestTrue("Temperature should be reasonable", ClimateData.Temperature > -100.0f && ClimateData.Temperature < 100.0f);
		TestTrue("Moisture should be in valid range", ClimateData.Moisture >= 0.0f && ClimateData.Moisture <= 1.0f);
	}
	
	// Test world gen settings
	UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
	TestNotNull("WorldGen settings should be available", Settings);
	
	if (Settings)
	{
		TestEqual("Tile size should be locked to 64m", Settings->Settings.TileSizeMeters, 64.0f);
		TestEqual("Sample spacing should be locked to 1m", Settings->Settings.SampleSpacingMeters, 1.0f);
		TestEqual("Max terrain height should be locked to 120m", Settings->Settings.MaxTerrainHeight, 120.0f);
	}
	
	// Test PCG system
	UPCGWorldService* PCGService = NewObject<UPCGWorldService>();
	TestNotNull("PCG World Service should be created", PCGService);
	
	if (PCGService && Settings)
	{
		bool bPCGInitialized = PCGService->Initialize(Settings->Settings);
		TestTrue("PCG service should initialize", bPCGInitialized);
		
		// Test basic PCG generation
		TArray<float> DummyHeightData;
		DummyHeightData.SetNum(64 * 64);
		for (int32 i = 0; i < DummyHeightData.Num(); i++)
		{
			DummyHeightData[i] = 10.0f; // Flat terrain for testing
		}
		
		FPCGGenerationData PCGData = PCGService->GenerateBiomeContent(FTileCoord(0, 0), EBiomeType::Meadows, DummyHeightData);
		TestTrue("PCG should generate content", PCGData.TotalInstanceCount >= 0);
		TestEqual("PCG tile coord should match", PCGData.TileCoord, FTileCoord(0, 0));
	}
	
	return true;
}

#endif // WITH_AUTOMATION_TESTS