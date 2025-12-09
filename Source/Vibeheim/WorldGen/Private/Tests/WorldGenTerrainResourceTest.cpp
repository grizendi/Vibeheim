#if WITH_AUTOMATION_TESTS

#include "Data/WorldGenTerrainResource.h"
#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"

/**
 * Property 5: Prebaked Height Query Consistency
 * Ensures GetHeightAtWorldPosition returns the value stored in the prebaked height texture.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPrebakedHeightQueryConsistencyTest,
    "Vibeheim.WorldGen.TerrainResource.Property.PreBakedHeightQuery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPrebakedHeightQueryConsistencyTest::RunTest(const FString& Parameters)
{
    constexpr int32 TextureSize = 4;
    constexpr float TileSizeMeters = 4.0f;

    UWorldGenTerrainResource* TerrainResource = NewObject<UWorldGenTerrainResource>();
    TerrainResource->WorldOrigin = FVector2D::ZeroVector;
    TerrainResource->TileSizeMeters = TileSizeMeters;
    TerrainResource->SampleSpacingMeters = 1.0f;
    TerrainResource->SeaLevel = -9999.0f;

    // Create a deterministic height texture
    UTexture2D* HeightTexture = NewObject<UTexture2D>();
    HeightTexture->Source.Init(TextureSize, TextureSize, 1, 1, TSF_R32F);

    TArray<float> HeightValues;
    HeightValues.SetNumUninitialized(TextureSize * TextureSize);
    for (int32 Y = 0; Y < TextureSize; ++Y)
    {
        for (int32 X = 0; X < TextureSize; ++X)
        {
            HeightValues[Y * TextureSize + X] = 10.0f + static_cast<float>(Y * TextureSize + X);
        }
    }

    void* MipData = HeightTexture->Source.LockMip(0);
    FMemory::Memcpy(MipData, HeightValues.GetData(), HeightValues.Num() * sizeof(float));
    HeightTexture->Source.UnlockMip(0);
    HeightTexture->CompressionSettings = TC_HDR;
    HeightTexture->SRGB = 0;
    HeightTexture->UpdateResource();

    TerrainResource->HeightTextures.Add(FIntPoint::ZeroValue, HeightTexture);

    auto ComputeExpected = [&](const FVector& WorldPos) -> float
    {
        const float U = WorldPos.X / TileSizeMeters;
        const float V = WorldPos.Y / TileSizeMeters;
        const int32 PixelX = FMath::Clamp(FMath::FloorToInt(U * TextureSize), 0, TextureSize - 1);
        const int32 PixelY = FMath::Clamp(FMath::FloorToInt(V * TextureSize), 0, TextureSize - 1);
        const int32 Index = PixelY * TextureSize + PixelX;
        return HeightValues[Index];
    };

    FRandomStream RandomStream(12345);
    const int32 Iterations = 25;
    for (int32 Index = 0; Index < Iterations; ++Index)
    {
        FVector WorldPos(RandomStream.FRandRange(0.0f, TileSizeMeters - KINDA_SMALL_NUMBER),
                         RandomStream.FRandRange(0.0f, TileSizeMeters - KINDA_SMALL_NUMBER),
                         0.0f);
        const float ExpectedHeight = ComputeExpected(WorldPos);
        const float SampledHeight = TerrainResource->GetHeightAtWorldPosition(WorldPos);

        TestTrue(
            FString::Printf(TEXT("Sampled height matches prebaked value at (%.2f, %.2f)"), WorldPos.X, WorldPos.Y),
            FMath::IsNearlyEqual(SampledHeight, ExpectedHeight, KINDA_SMALL_NUMBER));
    }

    // Validate tile lookup helper
    TestTrue(TEXT("HasTileData should return true for existing tile"),
             TerrainResource->HasTileData(FTileCoord(0, 0)));
    TestFalse(TEXT("HasTileData should return false for missing tile"),
              TerrainResource->HasTileData(FTileCoord(1, 0)));

    return true;
}

#endif // WITH_AUTOMATION_TESTS
