#include "FallbackTerrainGenerator.h"
#include "Engine/Engine.h"
#include "HAL/PlatformMath.h"

DEFINE_LOG_CATEGORY(LogFallbackTerrain);

UFallbackTerrainGenerator::UFallbackTerrainGenerator()
    : bIsInitialized(false)
    , BaseHeight(0.0f)
    , HeightVariation(500.0f)
{
}

bool UFallbackTerrainGenerator::Initialize(const FWorldGenSettings& Settings)
{
    CurrentSettings = Settings;
    
    // Create simple noise generator for fallback terrain
    SimpleNoiseGenerator = MakeUnique<FNoiseGenerator>();
    if (!SimpleNoiseGenerator.IsValid())
    {
        UE_LOG(LogFallbackTerrain, Error, TEXT("Failed to create simple noise generator - Seed: %lld"), Settings.Seed);
        return false;
    }
    
    SimpleNoiseGenerator->Initialize(Settings);
    
    // Set up simple terrain parameters
    BaseHeight = 0.0f;
    HeightVariation = 500.0f; // 5 meters of variation
    
    bIsInitialized = true;
    
    UE_LOG(LogFallbackTerrain, Log, TEXT("Fallback terrain generator initialized - Seed: %lld, BaseHeight: %.2f, Variation: %.2f"), 
           Settings.Seed, BaseHeight, HeightVariation);
    
    return true;
}

bool UFallbackTerrainGenerator::GenerateHeightmapForChunk(const FIntVector& ChunkCoordinate, TArray<float>& OutHeightData, 
                                                         int64 Seed, const FIntVector& LoggingChunkCoord)
{
    if (!bIsInitialized || !SimpleNoiseGenerator.IsValid())
    {
        UE_LOG(LogFallbackTerrain, Error, TEXT("Cannot generate fallback heightmap - generator not initialized - Seed: %lld, Chunk: (%d, %d, %d)"), 
               Seed, LoggingChunkCoord.X, LoggingChunkCoord.Y, LoggingChunkCoord.Z);
        return false;
    }
    
    const int32 ChunkSize = CurrentSettings.ChunkSize;
    const float VoxelSize = CurrentSettings.VoxelSizeCm;
    const float ChunkSizeWorld = ChunkSize * VoxelSize;
    
    // Calculate world position of chunk origin
    const FVector ChunkWorldOrigin(
        ChunkCoordinate.X * ChunkSizeWorld,
        ChunkCoordinate.Y * ChunkSizeWorld,
        ChunkCoordinate.Z * ChunkSizeWorld
    );
    
    // Prepare output array
    OutHeightData.Empty();
    OutHeightData.Reserve(ChunkSize * ChunkSize);
    
    UE_LOG(LogFallbackTerrain, Log, TEXT("Generating fallback heightmap for chunk (%d, %d, %d) - Seed: %lld, Origin: (%.2f, %.2f, %.2f)"), 
           LoggingChunkCoord.X, LoggingChunkCoord.Y, LoggingChunkCoord.Z, Seed, 
           ChunkWorldOrigin.X, ChunkWorldOrigin.Y, ChunkWorldOrigin.Z);
    
    // Generate simple heightmap using single octave noise
    int32 GeneratedPoints = 0;
    for (int32 Y = 0; Y < ChunkSize; Y++)
    {
        for (int32 X = 0; X < ChunkSize; X++)
        {
            const float WorldX = ChunkWorldOrigin.X + (X * VoxelSize);
            const float WorldY = ChunkWorldOrigin.Y + (Y * VoxelSize);
            
            const float Height = GenerateSimpleHeight(WorldX, WorldY, Seed);
            OutHeightData.Add(Height);
            GeneratedPoints++;
        }
    }
    
    UE_LOG(LogFallbackTerrain, Log, TEXT("Successfully generated fallback heightmap - Seed: %lld, Chunk: (%d, %d, %d), Points: %d"), 
           Seed, LoggingChunkCoord.X, LoggingChunkCoord.Y, LoggingChunkCoord.Z, GeneratedPoints);
    
    return true;
}

bool UFallbackTerrainGenerator::GenerateProxyMesh(const FIntVector& ChunkCoordinate, int64 Seed, const FIntVector& LoggingChunkCoord)
{
    if (!bIsInitialized)
    {
        UE_LOG(LogFallbackTerrain, Error, TEXT("Cannot generate proxy mesh - generator not initialized - Seed: %lld, Chunk: (%d, %d, %d)"), 
               Seed, LoggingChunkCoord.X, LoggingChunkCoord.Y, LoggingChunkCoord.Z);
        return false;
    }
    
    // For now, just log that we would generate a proxy mesh
    // In a full implementation, this would create a simple gray grid mesh
    UE_LOG(LogFallbackTerrain, Warning, TEXT("Generating gray proxy mesh for failed chunk - Seed: %lld, Chunk: (%d, %d, %d)"), 
           Seed, LoggingChunkCoord.X, LoggingChunkCoord.Y, LoggingChunkCoord.Z);
    
    // This is a placeholder - in production you would:
    // 1. Create a simple procedural mesh component
    // 2. Generate a basic grid of vertices
    // 3. Apply gray material
    // 4. Set up basic collision if needed
    
    UE_LOG(LogFallbackTerrain, Log, TEXT("Proxy mesh generation completed - Seed: %lld, Chunk: (%d, %d, %d)"), 
           Seed, LoggingChunkCoord.X, LoggingChunkCoord.Y, LoggingChunkCoord.Z);
    
    return true;
}

float UFallbackTerrainGenerator::GenerateSimpleHeight(float X, float Y, int64 Seed)
{
    if (!SimpleNoiseGenerator.IsValid())
    {
        return BaseHeight;
    }
    
    // Use simple single-octave Perlin noise
    const float NoiseScale = 0.001f; // Very low frequency for gentle terrain
    const float NoiseValue = SimpleNoiseGenerator->GeneratePerlinNoise(X, Y, NoiseScale, ENoiseFeatureTag::Terrain);
    
    // Convert noise from [-1, 1] to height range
    const float Height = BaseHeight + (NoiseValue * HeightVariation);
    
    return Height;
}