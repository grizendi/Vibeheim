#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Data/EnhancedBiomeData.h"
#include "BiomeSystem.h"
#include "BiomeMaterialSystem.generated.h"

/**
 * Material blending parameters for biome transitions
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FBiomeMaterialBlend
{
    GENERATED_BODY()

    /** Primary material (dominant biome) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Blend")
    TSoftObjectPtr<UMaterialInterface> PrimaryMaterial;

    /** Secondary material (for blending) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Blend")
    TSoftObjectPtr<UMaterialInterface> SecondaryMaterial;

    /** Blend factor between materials (0.0 = primary, 1.0 = secondary) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Blend", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BlendFactor = 0.0f;

    /** Blended color values */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Blend")
    FLinearColor BlendedColor = FLinearColor::White;

    /** Blended roughness value */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Blend", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BlendedRoughness = 0.5f;

    /** Blended metallic value */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Blend", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BlendedMetallic = 0.0f;

    FBiomeMaterialBlend()
    {
        BlendFactor = 0.0f;
        BlendedColor = FLinearColor::White;
        BlendedRoughness = 0.5f;
        BlendedMetallic = 0.0f;
    }
};

/**
 * System for managing biome-specific materials and smooth transitions
 */
class VIBEHEIM_API FBiomeMaterialSystem
{
public:
    /**
     * Initialize the material system
     */
    void Initialize();

    /**
     * Create a material blend from biome evaluation result
     * @param BiomeEvaluation The biome evaluation containing blend information
     * @return Material blend parameters
     */
    FBiomeMaterialBlend CreateMaterialBlend(const FBiomeEvaluation& BiomeEvaluation) const;

    /**
     * Create a dynamic material instance for biome blending
     * @param MaterialBlend The material blend parameters
     * @param BaseMaterial Optional base material to use for blending
     * @return Dynamic material instance (null if creation fails)
     */
    UMaterialInstanceDynamic* CreateBlendedMaterialInstance(const FBiomeMaterialBlend& MaterialBlend, UMaterialInterface* BaseMaterial = nullptr) const;

    /**
     * Apply material parameters to a dynamic material instance
     * @param MaterialInstance The dynamic material instance to modify
     * @param MaterialBlend The blend parameters to apply
     */
    void ApplyMaterialParameters(UMaterialInstanceDynamic* MaterialInstance, const FBiomeMaterialBlend& MaterialBlend) const;

    /**
     * Get fallback material for biome rendering when no specific material is available
     * @return Fallback material interface
     */
    UMaterialInterface* GetFallbackMaterial() const;

    /**
     * Set fallback material for biome rendering
     * @param Material The material to use as fallback
     */
    void SetFallbackMaterial(UMaterialInterface* Material);

    /**
     * Calculate smooth material transition between two biomes
     * @param BiomeA First biome type
     * @param BiomeB Second biome type
     * @param BlendFactor Blend factor (0.0 = BiomeA, 1.0 = BiomeB)
     * @param BiomeSystem Reference to biome system for data access
     * @return Material blend parameters
     */
    FBiomeMaterialBlend CalculateBiomeTransition(EBiomeType BiomeA, EBiomeType BiomeB, float BlendFactor, const FBiomeSystem& BiomeSystem) const;

    /**
     * Validate that all biome materials are properly configured
     * @param BiomeSystem Reference to biome system for validation
     * @return True if all materials are valid
     */
    bool ValidateBiomeMaterials(const FBiomeSystem& BiomeSystem) const;

private:
    /** Fallback material when biome-specific materials are not available */
    UPROPERTY()
    TSoftObjectPtr<UMaterialInterface> FallbackMaterial;

    /** Cache of created dynamic material instances */
    mutable TMap<FString, UMaterialInstanceDynamic*> MaterialInstanceCache;

    /**
     * Generate cache key for material blend
     * @param MaterialBlend The material blend to generate key for
     * @return Cache key string
     */
    FString GenerateMaterialCacheKey(const FBiomeMaterialBlend& MaterialBlend) const;

    /**
     * Blend two colors with specified factor
     * @param ColorA First color
     * @param ColorB Second color
     * @param BlendFactor Blend factor (0.0 = ColorA, 1.0 = ColorB)
     * @return Blended color
     */
    FLinearColor BlendColors(const FLinearColor& ColorA, const FLinearColor& ColorB, float BlendFactor) const;

    /**
     * Blend two scalar values with specified factor
     * @param ValueA First value
     * @param ValueB Second value
     * @param BlendFactor Blend factor (0.0 = ValueA, 1.0 = ValueB)
     * @return Blended value
     */
    float BlendScalarValues(float ValueA, float ValueB, float BlendFactor) const;
};