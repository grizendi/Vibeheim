#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NoiseSystem.generated.h"

/**
 * Noise type enumeration for different noise algorithms
 */
UENUM(BlueprintType)
enum class ENoiseType : uint8
{
	Perlin = 0,
	Simplex = 1,
	Ridge = 2,
	Billow = 3,
	Voronoi = 4,
	MAX UMETA(Hidden)
};

/**
 * Domain warp settings for noise distortion
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FDomainWarpSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Domain Warp")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Domain Warp")
	float Strength = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Domain Warp")
	float Scale = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Domain Warp")
	int32 Octaves = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Domain Warp")
	FVector2D Offset = FVector2D::ZeroVector;
};

/**
 * Advanced noise settings with multiple octaves and domain warping
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FAdvancedNoiseSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	ENoiseType NoiseType = ENoiseType::Perlin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	float Scale = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	float Amplitude = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	int32 Octaves = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	float Persistence = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	float Lacunarity = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	FVector2D Offset = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Domain Warp")
	FDomainWarpSettings DomainWarp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	float RidgeSharpness = 1.0f; // For ridge noise

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	float BillowBias = 0.0f; // For billow noise
};

/**
 * Deterministic noise system with domain warp capabilities
 * Provides various noise algorithms for terrain generation
 */
UCLASS(BlueprintType)
class VIBEHEIM_API UNoiseSystem : public UObject
{
	GENERATED_BODY()

public:
	UNoiseSystem();

	/**
	 * Initialize the noise system with a seed
	 */
	UFUNCTION(BlueprintCallable, Category = "Noise")
	void Initialize(int32 InSeed);

	/**
	 * Generate noise value at a specific position
	 */
	UFUNCTION(BlueprintCallable, Category = "Noise")
	float GenerateNoise(FVector2D Position, const FAdvancedNoiseSettings& Settings) const;

	/**
	 * Generate noise with simple settings (backward compatibility)
	 */
	UFUNCTION(BlueprintCallable, Category = "Noise")
	float GenerateSimpleNoise(FVector2D Position, float Scale, float Amplitude) const;
	float GenerateSimpleNoise(FVector2D Position, float Scale, float Amplitude, int32 NoiseTypeIndex) const;
	
	/**
	 * Generate octave noise (multiple layers)
	 */
	UFUNCTION(BlueprintCallable, Category = "Noise")
	float GenerateOctaveNoise(FVector2D Position, const FAdvancedNoiseSettings& Settings) const;

	/**
	 * Apply domain warping to a position
	 */
	UFUNCTION(BlueprintCallable, Category = "Noise")
	FVector2D ApplyDomainWarp(FVector2D Position, const FDomainWarpSettings& WarpSettings) const;

	/**
	 * Generate a 2D noise field for a given area
	 */
	UFUNCTION(BlueprintCallable, Category = "Noise")
	TArray<float> GenerateNoiseField(FVector2D StartPosition, int32 Width, int32 Height, float SampleSpacing, const FAdvancedNoiseSettings& Settings) const;

	/**
	 * Get the current seed
	 */
	UFUNCTION(BlueprintCallable, Category = "Noise")
	int32 GetSeed() const { return Seed; }

private:
	UPROPERTY()
	int32 Seed;

	/**
	 * Generate Perlin noise
	 */
	float GeneratePerlinNoise(FVector2D Position, float Scale) const;

	/**
	 * Generate Simplex noise
	 */
	float GenerateSimplexNoise(FVector2D Position, float Scale) const;

	/**
	 * Generate Ridge noise (absolute Perlin)
	 */
	float GenerateRidgeNoise(FVector2D Position, float Scale, float Sharpness) const;

	/**
	 * Generate Billow noise (absolute Perlin with bias)
	 */
	float GenerateBillowNoise(FVector2D Position, float Scale, float Bias) const;

	/**
	 * Generate Voronoi noise
	 */
	float GenerateVoronoiNoise(FVector2D Position, float Scale) const;

	/**
	 * Hash function for deterministic random values
	 */
	uint32 Hash(int32 X, int32 Y, uint32 NoiseType = 0) const;

	/**
	 * Hash function for floating point coordinates
	 */
	uint32 HashFloat(float X, float Y, uint32 NoiseType = 0) const;

	/**
	 * Smooth interpolation function
	 */
	float SmoothStep(float t) const;

	/**
	 * Linear interpolation
	 */
	float Lerp(float a, float b, float t) const;

	/**
	 * Gradient function for Perlin noise
	 */
	float Gradient(uint32 Hash, float X, float Y) const;

	/**
	 * Find closest Voronoi cell
	 */
	FVector2D FindClosestVoronoiPoint(FVector2D Position, float Scale) const;
};
