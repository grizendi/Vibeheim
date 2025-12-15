#if WITH_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "PCGVersionGuard.h"
#include "Services/BiomeService.h"
#include "Services/ClimateSystem.h"
#include "Services/HeightfieldService.h"
#include "WorldGenExternalDataProvider.h"

#if VHM_PCG_ENABLED

/**
 * Property 7: PCG External Data Determinism
 * Verifies that repeated external-data queries for the same cell produce identical aggregates.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWorldGenExternalDataProviderDeterminismTest,
    "Vibeheim.WorldGen.ExternalData.Property.ExternalDataDeterminism",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWorldGenExternalDataProviderDeterminismTest::RunTest(const FString& Parameters)
{
        FWorldGenConfig Config;
        Config.Seed = 777;
        Config.TileSizeMeters = 64.0f;
        Config.SampleSpacingMeters = 1.0f;

        UHeightfieldService* HeightService = NewObject<UHeightfieldService>();
        TestTrue(TEXT("HeightfieldService initializes"), HeightService && HeightService->Initialize(Config));

        UClimateSystem* ClimateSystem = NewObject<UClimateSystem>();
        TestNotNull(TEXT("ClimateSystem allocates"), ClimateSystem);
        if (ClimateSystem)
        {
                FClimateSettings ClimateSettings;
                ClimateSystem->InitializeWithConfig(ClimateSettings, Config.Seed, Config);
        }

        UBiomeService* BiomeService = NewObject<UBiomeService>();
        TestNotNull(TEXT("BiomeService allocates"), BiomeService);
        if (BiomeService && ClimateSystem)
        {
                BiomeService->Initialize(ClimateSystem, Config);
        }

        UWorldGenExternalDataProvider* Provider = NewObject<UWorldGenExternalDataProvider>();
        TestNotNull(TEXT("External data provider allocates"), Provider);
        if (!Provider)
        {
                return false;
        }

        Provider->Initialize(Config, HeightService, BiomeService, ClimateSystem, nullptr);

        auto BuildSignature = [&](const FIntPoint& Cell) -> uint32 {
                FTileCoord TileCoord;
                FPCGTileMetrics Metrics;
                FBiomeResult Biome;
                FClimateData Climate;
                int32 TileSeed = 0;
                if (!FWorldGenExternalDataProviderTestAccessor::BuildCellData(
                        Provider, Cell, TileCoord, Metrics, Biome, Climate, TileSeed))
                {
                        AddError(FString::Printf(TEXT("Failed to build cell data for (%d,%d)"), Cell.X, Cell.Y));
                        return 0;
                }

                const float BiomeWeight =
                    Biome.BiomeWeights.FindRef(Biome.PrimaryBiome);

                uint32 Signature = 0;
                Signature = HashCombine(Signature, GetTypeHash(TileCoord.X));
                Signature = HashCombine(Signature, GetTypeHash(TileCoord.Y));
                Signature = HashCombine(Signature, GetTypeHash(Metrics.AverageHeight));
                Signature = HashCombine(Signature, GetTypeHash(Metrics.MinHeight));
                Signature = HashCombine(Signature, GetTypeHash(Metrics.MaxHeight));
                Signature = HashCombine(Signature, GetTypeHash(Metrics.AverageSlope));
                Signature = HashCombine(Signature, GetTypeHash(Metrics.MaxSlope));
                Signature = HashCombine(Signature, GetTypeHash(Metrics.WaterCoverageRatio));
                Signature = HashCombine(Signature, GetTypeHash(Metrics.AverageAboveWater));
                Signature = HashCombine(Signature, GetTypeHash(Metrics.AverageBelowWater));
                Signature = HashCombine(Signature, GetTypeHash(Metrics.MinAbsWaterDistance));
                Signature = HashCombine(Signature, GetTypeHash(static_cast<int32>(Biome.PrimaryBiome)));
                Signature = HashCombine(Signature, GetTypeHash(BiomeWeight));
                Signature = HashCombine(Signature, GetTypeHash(Climate.Temperature));
                Signature = HashCombine(Signature, GetTypeHash(Climate.Moisture));
                Signature = HashCombine(Signature, GetTypeHash(Climate.RingBias));
                Signature = HashCombine(Signature, GetTypeHash(TileSeed));
                return Signature;
        };

        FRandomStream RandomStream(12345);
        constexpr int32 Iterations = 8;
        for (int32 Index = 0; Index < Iterations; ++Index)
        {
                const FIntPoint Cell(RandomStream.RandRange(-5, 5), RandomStream.RandRange(-5, 5));
                const uint32 SignatureA = BuildSignature(Cell);
                const uint32 SignatureB = BuildSignature(Cell);
                TestEqual(FString::Printf(TEXT("Signatures match for cell (%d,%d)"), Cell.X, Cell.Y),
                          SignatureA, SignatureB);

                UPCGParamData* ParamData = Provider->CreateParamDataForCell(Cell);
                TestNotNull(TEXT("ParamData created"), ParamData);
        }

        return true;
}

#endif // VHM_PCG_ENABLED
#endif // WITH_AUTOMATION_TESTS
