#include "CoreMinimal.h"
#include "Utils/HashUtils.h"
#include "Services/HeightfieldService.h"
#include "Services/ClimateSystem.h"
#include "Services/NoiseSystem.h"
#include "WorldGenSettings.h"

#if WITH_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeterminismTest, "Vibeheim.WorldGen.Determinism.MultiRunConsistency", 
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeterminismTest::RunTest(const FString& Parameters)
{
	// Test 1: Multi-run determinism with fixed seeds
	TestTrue("Multi-run determinism test", TestMultiRunDeterminism());

	// Test 2: Border seam equality between adjacent tiles
	TestTrue("Border seam equality test", TestBorderSeamEquality());

	// Test 3: Checksum stability over multiple runs
	TestTrue("Checksum stability test", TestChecksumStability());

	// Test 4: Noise determinism validation
	TestTrue("Noise system determinism test", TestNoiseDeterminism());

	// Test 5: Climate system determinism
	TestTrue("Climate system determinism test", TestClimateDeterminism());

	return true;
}

bool FDeterminismTest::TestMultiRunDeterminism()
{
	UE_LOG(LogTemp, Log, TEXT("Testing multi-run determinism..."));

	// Create heightfield service
	UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
	UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
	
	if (!HeightfieldService || !Settings)
	{
		return false;
	}

	HeightfieldService->Initialize(Settings->Settings);

	const uint64 TestSeed = 12345;
	const FTileCoord TestTile(5, 7);
	const int32 NumRuns = 5;

	// Generate heightfield multiple times and compare
	TArray<FHeightfieldData> GeneratedTiles;
	TArray<FTileChecksum> GeneratedChecksums;

	for (int32 Run = 0; Run < NumRuns; Run++)
	{
		// Clear cache to ensure fresh generation
		HeightfieldService->ClearHeightfieldCache();

		// Generate heightfield
		FHeightfieldData HeightfieldData = HeightfieldService->GenerateHeightfield(TestSeed, TestTile);
		GeneratedTiles.Add(HeightfieldData);

		// Calculate checksum
		FTileChecksum Checksum = UHashUtils::CalculateHeightfieldChecksum(
			HeightfieldData, TestSeed, Settings->Settings.WorldGenVersion);
		GeneratedChecksums.Add(Checksum);

		UE_LOG(LogTemp, VeryVerbose, TEXT("Run %d: Tile (%d,%d) checksum: %llu"), 
			Run, TestTile.X, TestTile.Y, Checksum.CombinedChecksum);
	}

	// Validate all runs produced identical results
	for (int32 Run = 1; Run < NumRuns; Run++)
	{
		// Compare height data
		if (GeneratedTiles[0].HeightData.Num() != GeneratedTiles[Run].HeightData.Num())
		{
			UE_LOG(LogTemp, Error, TEXT("Height data array size mismatch between runs 0 and %d"), Run);
			return false;
		}

		for (int32 i = 0; i < GeneratedTiles[0].HeightData.Num(); i++)
		{
			if (FMath::Abs(GeneratedTiles[0].HeightData[i] - GeneratedTiles[Run].HeightData[i]) > 0.0001f)
			{
				UE_LOG(LogTemp, Error, TEXT("Height data mismatch at index %d between runs 0 and %d: %f vs %f"), 
					i, Run, GeneratedTiles[0].HeightData[i], GeneratedTiles[Run].HeightData[i]);
				return false;
			}
		}

		// Compare checksums
		if (GeneratedChecksums[0].CombinedChecksum != GeneratedChecksums[Run].CombinedChecksum)
		{
			UE_LOG(LogTemp, Error, TEXT("Checksum mismatch between runs 0 and %d: %llu vs %llu"), 
				Run, GeneratedChecksums[0].CombinedChecksum, GeneratedChecksums[Run].CombinedChecksum);
			return false;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Multi-run determinism test passed - all %d runs produced identical results"), NumRuns);
	return true;
}

bool FDeterminismTest::TestBorderSeamEquality()
{
	UE_LOG(LogTemp, Log, TEXT("Testing border seam equality..."));

	// Create heightfield service
	UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
	UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
	
	if (!HeightfieldService || !Settings)
	{
		return false;
	}

	HeightfieldService->Initialize(Settings->Settings);

	const uint64 TestSeed = 67890;
	const FTileCoord CenterTile(10, 10);

	// Generate center tile and all adjacent tiles
	FHeightfieldData CenterData = HeightfieldService->GenerateHeightfield(TestSeed, CenterTile);
	FHeightfieldData RightData = HeightfieldService->GenerateHeightfield(TestSeed, FTileCoord(11, 10));
	FHeightfieldData TopData = HeightfieldService->GenerateHeightfield(TestSeed, FTileCoord(10, 11));
	FHeightfieldData LeftData = HeightfieldService->GenerateHeightfield(TestSeed, FTileCoord(9, 10));
	FHeightfieldData BottomData = HeightfieldService->GenerateHeightfield(TestSeed, FTileCoord(10, 9));

	// Test border seams
	bool bRightSeamValid = UHashUtils::ValidateTileBorderSeam(CenterData, RightData);
	bool bTopSeamValid = UHashUtils::ValidateTileBorderSeam(CenterData, TopData);
	bool bLeftSeamValid = UHashUtils::ValidateTileBorderSeam(CenterData, LeftData);
	bool bBottomSeamValid = UHashUtils::ValidateTileBorderSeam(CenterData, BottomData);

	if (!bRightSeamValid)
	{
		UE_LOG(LogTemp, Error, TEXT("Right border seam validation failed"));
		return false;
	}

	if (!bTopSeamValid)
	{
		UE_LOG(LogTemp, Error, TEXT("Top border seam validation failed"));
		return false;
	}

	if (!bLeftSeamValid)
	{
		UE_LOG(LogTemp, Error, TEXT("Left border seam validation failed"));
		return false;
	}

	if (!bBottomSeamValid)
	{
		UE_LOG(LogTemp, Error, TEXT("Bottom border seam validation failed"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("Border seam equality test passed - all adjacent tiles have seamless borders"));
	return true;
}

bool FDeterminismTest::TestChecksumStability()
{
	UE_LOG(LogTemp, Log, TEXT("Testing checksum stability..."));

	// Create heightfield service
	UHeightfieldService* HeightfieldService = NewObject<UHeightfieldService>();
	UWorldGenSettings* Settings = UWorldGenSettings::GetWorldGenSettings();
	
	if (!HeightfieldService || !Settings)
	{
		return false;
	}

	HeightfieldService->Initialize(Settings->Settings);

	const uint64 TestSeed = 11111;
	const FTileCoord TestTile(3, 8);

	// Generate baseline heightfield and checksum
	FHeightfieldData BaselineData = HeightfieldService->GenerateHeightfield(TestSeed, TestTile);
	FTileChecksum BaselineChecksum = UHashUtils::CalculateHeightfieldChecksum(
		BaselineData, TestSeed, Settings->Settings.WorldGenVersion);

	// Test checksum validation
	bool bValidationPassed = UHashUtils::ValidateHeightfieldChecksum(BaselineData, BaselineChecksum);
	if (!bValidationPassed)
	{
		UE_LOG(LogTemp, Error, TEXT("Baseline checksum validation failed"));
		return false;
	}

	// Test multiple checksum calculations produce same result
	for (int32 i = 0; i < 10; i++)
	{
		FTileChecksum TestChecksum = UHashUtils::CalculateHeightfieldChecksum(
			BaselineData, TestSeed, Settings->Settings.WorldGenVersion);

		if (TestChecksum.CombinedChecksum != BaselineChecksum.CombinedChecksum)
		{
			UE_LOG(LogTemp, Error, TEXT("Checksum instability detected on iteration %d: %llu vs %llu"), 
				i, TestChecksum.CombinedChecksum, BaselineChecksum.CombinedChecksum);
			return false;
		}
	}

	// Test that modifying data breaks checksum validation
	FHeightfieldData ModifiedData = BaselineData;
	if (ModifiedData.HeightData.Num() > 0)
	{
		ModifiedData.HeightData[0] += 1.0f; // Modify first height value

		bool bShouldFail = UHashUtils::ValidateHeightfieldChecksum(ModifiedData, BaselineChecksum);
		if (bShouldFail)
		{
			UE_LOG(LogTemp, Error, TEXT("Modified data incorrectly passed checksum validation"));
			return false;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Checksum stability test passed"));
	return true;
}

bool FDeterminismTest::TestNoiseDeterminism()
{
	UE_LOG(LogTemp, Log, TEXT("Testing noise system determinism..."));

	// Create noise system
	UNoiseSystem* NoiseSystem = NewObject<UNoiseSystem>();
	if (!NoiseSystem)
	{
		return false;
	}

	const uint64 TestSeed = 22222;
	NoiseSystem->Initialize(TestSeed);

	FAdvancedNoiseSettings NoiseSettings;
	NoiseSettings.NoiseType = ENoiseType::Perlin;
	NoiseSettings.Scale = 0.01f;
	NoiseSettings.Amplitude = 1.0f;
	NoiseSettings.Octaves = 4;
	NoiseSettings.Persistence = 0.5f;
	NoiseSettings.Lacunarity = 2.0f;

	const FVector2D TestPosition(123.456f, 789.012f);
	const int32 NumTests = 20;

	// Generate noise values multiple times
	TArray<float> NoiseValues;
	for (int32 i = 0; i < NumTests; i++)
	{
		float NoiseValue = NoiseSystem->GenerateNoise(TestPosition, NoiseSettings);
		NoiseValues.Add(NoiseValue);
	}

	// Verify all values are identical
	for (int32 i = 1; i < NumTests; i++)
	{
		if (FMath::Abs(NoiseValues[0] - NoiseValues[i]) > 0.0001f)
		{
			UE_LOG(LogTemp, Error, TEXT("Noise determinism failed: values %f and %f differ"), 
				NoiseValues[0], NoiseValues[i]);
			return false;
		}
	}

	// Test noise field generation determinism
	TArray<float> NoiseField1 = NoiseSystem->GenerateNoiseField(
		FVector2D(0, 0), 8, 8, 1.0f, NoiseSettings);
	TArray<float> NoiseField2 = NoiseSystem->GenerateNoiseField(
		FVector2D(0, 0), 8, 8, 1.0f, NoiseSettings);

	if (NoiseField1.Num() != NoiseField2.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("Noise field size mismatch"));
		return false;
	}

	for (int32 i = 0; i < NoiseField1.Num(); i++)
	{
		if (FMath::Abs(NoiseField1[i] - NoiseField2[i]) > 0.0001f)
		{
			UE_LOG(LogTemp, Error, TEXT("Noise field determinism failed at index %d: %f vs %f"), 
				i, NoiseField1[i], NoiseField2[i]);
			return false;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Noise system determinism test passed"));
	return true;
}

bool FDeterminismTest::TestClimateDeterminism()
{
	UE_LOG(LogTemp, Log, TEXT("Testing climate system determinism..."));

	// Create climate system
	UClimateSystem* ClimateSystem = NewObject<UClimateSystem>();
	if (!ClimateSystem)
	{
		return false;
	}

	const uint64 TestSeed = 33333;
	FClimateSettings ClimateSettings;
	ClimateSystem->Initialize(ClimateSettings, TestSeed);

	const FVector2D TestPosition(456.789f, 123.456f);
	const float TestAltitude = 50.0f;
	const int32 NumTests = 15;

	// Test climate data determinism
	TArray<FClimateData> ClimateResults;
	for (int32 i = 0; i < NumTests; i++)
	{
		FClimateData ClimateData = ClimateSystem->CalculateClimate(TestPosition, TestAltitude);
		ClimateResults.Add(ClimateData);
	}

	// Verify all results are identical
	for (int32 i = 1; i < NumTests; i++)
	{
		if (FMath::Abs(ClimateResults[0].Temperature - ClimateResults[i].Temperature) > 0.0001f)
		{
			UE_LOG(LogTemp, Error, TEXT("Climate temperature determinism failed: %f vs %f"), 
				ClimateResults[0].Temperature, ClimateResults[i].Temperature);
			return false;
		}

		if (FMath::Abs(ClimateResults[0].Moisture - ClimateResults[i].Moisture) > 0.0001f)
		{
			UE_LOG(LogTemp, Error, TEXT("Climate moisture determinism failed: %f vs %f"), 
				ClimateResults[0].Moisture, ClimateResults[i].Moisture);
			return false;
		}

		if (FMath::Abs(ClimateResults[0].RingBias - ClimateResults[i].RingBias) > 0.0001f)
		{
			UE_LOG(LogTemp, Error, TEXT("Climate ring bias determinism failed: %f vs %f"), 
				ClimateResults[0].RingBias, ClimateResults[i].RingBias);
			return false;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Climate system determinism test passed"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHashUtilsTest, "Vibeheim.WorldGen.Determinism.HashUtils", 
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FHashUtilsTest::RunTest(const FString& Parameters)
{
	// Test xxHash64 consistency
	TArray<uint8> TestData = {'T', 'e', 's', 't', 'D', 'a', 't', 'a'};
	uint64 Hash1 = UHashUtils::CalculateXXHash64(TestData, 0);
	uint64 Hash2 = UHashUtils::CalculateXXHash64(TestData, 0);
	
	TestEqual("xxHash64 consistency", Hash1, Hash2);

	// Test different seeds produce different hashes
	uint64 Hash3 = UHashUtils::CalculateXXHash64(TestData, 12345);
	TestNotEqual("Different seeds produce different hashes", Hash1, Hash3);

	// Test tile seed generation
	FTileCoord TestTile(5, 10);
	uint64 TileSeed1 = UHashUtils::GenerateTileSeed(1000, TestTile, 0);
	uint64 TileSeed2 = UHashUtils::GenerateTileSeed(1000, TestTile, 0);
	TestEqual("Tile seed consistency", TileSeed1, TileSeed2);

	uint64 TileSeed3 = UHashUtils::GenerateTileSeed(1000, FTileCoord(5, 11), 0);
	TestNotEqual("Different tiles produce different seeds", TileSeed1, TileSeed3);

	// Test PCG seed generation
	uint64 PCGSeed1 = UHashUtils::GeneratePCGSeed(2000, TestTile, 1, 5);
	uint64 PCGSeed2 = UHashUtils::GeneratePCGSeed(2000, TestTile, 1, 5);
	TestEqual("PCG seed consistency", PCGSeed1, PCGSeed2);

	uint64 PCGSeed3 = UHashUtils::GeneratePCGSeed(2000, TestTile, 2, 5);
	TestNotEqual("Different prototype IDs produce different PCG seeds", PCGSeed1, PCGSeed3);

	// Test coordinate hashing
	uint64 CoordHash1 = UHashUtils::HashCoordinates(10, 20, 0);
	uint64 CoordHash2 = UHashUtils::HashCoordinates(10, 20, 0);
	TestEqual("Coordinate hash consistency", CoordHash1, CoordHash2);

	uint64 CoordHash3 = UHashUtils::HashCoordinates(10, 21, 0);
	TestNotEqual("Different coordinates produce different hashes", CoordHash1, CoordHash3);

	return true;
}

#endif // WITH_AUTOMATION_TESTS
