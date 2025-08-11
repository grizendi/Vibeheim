#include "BiomeSystem.h"
#include "Data/WorldGenSettings.h"
#include "Engine/Engine.h"

/**
 * Example usage of the BiomeSystem for integration testing
 * This demonstrates how the BiomeSystem would be used in the actual world generation pipeline
 */
class FBiomeSystemExample
{
public:
    static void RunExample()
    {
        // Create world generation settings
        FWorldGenSettings Settings;
        Settings.Seed = 12345;
        Settings.BiomeBlendMeters = 24.0f;
        Settings.MeadowsScale = 0.0025f;
        Settings.BlackForestScale = 0.0030f;
        Settings.SwampScale = 0.0020f;

        // Initialize biome system
        FBiomeSystem BiomeSystem;
        BiomeSystem.Initialize(Settings);

        // Example: Generate biome data for a chunk
        const float ChunkSize = 1600.0f; // 32 voxels * 50cm = 1600cm = 16m
        const int32 SamplesPerChunk = 8; // Sample biome at 8x8 grid per chunk
        
        FVector ChunkWorldPosition(0.0f, 0.0f, 0.0f); // Example chunk at origin
        
        UE_LOG(LogTemp, Warning, TEXT("=== Biome System Example ==="));
        UE_LOG(LogTemp, Warning, TEXT("Chunk Position: %s"), *ChunkWorldPosition.ToString());
        UE_LOG(LogTemp, Warning, TEXT("Seed: %lld"), Settings.Seed);
        
        // Sample biome data across the chunk
        for (int32 X = 0; X < SamplesPerChunk; ++X)
        {
            for (int32 Y = 0; Y < SamplesPerChunk; ++Y)
            {
                // Calculate world position for this sample
                float WorldX = ChunkWorldPosition.X + (X * ChunkSize / SamplesPerChunk);
                float WorldY = ChunkWorldPosition.Y + (Y * ChunkSize / SamplesPerChunk);
                
                // Evaluate biome at this location
                FBiomeEvaluation BiomeEval = BiomeSystem.EvaluateBiome(WorldX, WorldY);
                
                // Log biome information
                FString DominantBiomeName = GetBiomeName(BiomeEval.BiomeWeights.DominantBiome);
                
                UE_LOG(LogTemp, Log, TEXT("Sample [%d,%d] at (%.1f, %.1f): Dominant=%s, HeightOffset=%.2f"), 
                    X, Y, WorldX, WorldY, *DominantBiomeName, BiomeEval.BlendedHeightOffset);
                
                // Log normalized weights
                UE_LOG(LogTemp, VeryVerbose, TEXT("  Weights: Meadows=%.3f, BlackForest=%.3f, Swamp=%.3f"),
                    BiomeEval.BiomeWeights.NormalizedWeights[static_cast<int32>(EBiomeType::Meadows)],
                    BiomeEval.BiomeWeights.NormalizedWeights[static_cast<int32>(EBiomeType::BlackForest)],
                    BiomeEval.BiomeWeights.NormalizedWeights[static_cast<int32>(EBiomeType::Swamp)]);
            }
        }
        
        // Demonstrate biome transition detection
        UE_LOG(LogTemp, Warning, TEXT("=== Biome Transition Test ==="));
        TestBiomeTransitions(BiomeSystem);
        
        UE_LOG(LogTemp, Warning, TEXT("=== Biome System Example Complete ==="));
    }

private:
    static FString GetBiomeName(EBiomeType BiomeType)
    {
        switch (BiomeType)
        {
            case EBiomeType::Meadows: return TEXT("Meadows");
            case EBiomeType::BlackForest: return TEXT("BlackForest");
            case EBiomeType::Swamp: return TEXT("Swamp");
            default: return TEXT("Unknown");
        }
    }
    
    static void TestBiomeTransitions(const FBiomeSystem& BiomeSystem)
    {
        // Test biome transitions along a line
        const float StartX = -1000.0f;
        const float EndX = 1000.0f;
        const float Y = 0.0f;
        const int32 NumSamples = 20;
        
        EBiomeType LastDominantBiome = EBiomeType::Meadows;
        int32 TransitionCount = 0;
        
        for (int32 i = 0; i < NumSamples; ++i)
        {
            float X = FMath::Lerp(StartX, EndX, static_cast<float>(i) / (NumSamples - 1));
            FBiomeEvaluation BiomeEval = BiomeSystem.EvaluateBiome(X, Y);
            
            if (i > 0 && BiomeEval.BiomeWeights.DominantBiome != LastDominantBiome)
            {
                TransitionCount++;
                UE_LOG(LogTemp, Warning, TEXT("Biome transition at X=%.1f: %s -> %s"), 
                    X, 
                    *GetBiomeName(LastDominantBiome), 
                    *GetBiomeName(BiomeEval.BiomeWeights.DominantBiome));
            }
            
            LastDominantBiome = BiomeEval.BiomeWeights.DominantBiome;
        }
        
        UE_LOG(LogTemp, Warning, TEXT("Total biome transitions detected: %d"), TransitionCount);
    }
};