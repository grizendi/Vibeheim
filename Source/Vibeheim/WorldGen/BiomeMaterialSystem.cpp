#include "BiomeMaterialSystem.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

void FBiomeMaterialSystem::Initialize()
{
    // Set default fallback material from VoxelPluginLegacy
    FallbackMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/VoxelFree/Examples/Shared/VoxelExamples_SimpleColorMaterial.VoxelExamples_SimpleColorMaterial")));
    
    // Clear any existing cache
    MaterialInstanceCache.Empty();
}

FBiomeMaterialBlend FBiomeMaterialSystem::CreateMaterialBlend(const FBiomeEvaluation& BiomeEvaluation) const
{
    FBiomeMaterialBlend MaterialBlend;
    
    // Set primary material from dominant biome
    MaterialBlend.PrimaryMaterial = BiomeEvaluation.BlendedMaterial;
    MaterialBlend.BlendedColor = BiomeEvaluation.BlendedBiomeColor;
    MaterialBlend.BlendedRoughness = BiomeEvaluation.BlendedRoughness;
    MaterialBlend.BlendedMetallic = BiomeEvaluation.BlendedMetallic;
    
    // For now, we use the blended result directly
    // Future enhancement could support multi-material blending
    MaterialBlend.BlendFactor = 0.0f; // Use primary material fully
    
    return MaterialBlend;
}

UMaterialInstanceDynamic* FBiomeMaterialSystem::CreateBlendedMaterialInstance(const FBiomeMaterialBlend& MaterialBlend, UMaterialInterface* BaseMaterial) const
{
    // Use provided base material or fallback
    UMaterialInterface* SourceMaterial = BaseMaterial;
    if (!SourceMaterial)
    {
        if (MaterialBlend.PrimaryMaterial.IsValid())
        {
            SourceMaterial = MaterialBlend.PrimaryMaterial.LoadSynchronous();
        }
        else
        {
            SourceMaterial = GetFallbackMaterial();
        }
    }
    
    if (!SourceMaterial)
    {
        UE_LOG(LogTemp, Warning, TEXT("BiomeMaterialSystem: No valid source material available for blending"));
        return nullptr;
    }
    
    // Check cache first
    FString CacheKey = GenerateMaterialCacheKey(MaterialBlend);
    if (UMaterialInstanceDynamic** CachedInstance = MaterialInstanceCache.Find(CacheKey))
    {
        if (IsValid(*CachedInstance))
        {
            return *CachedInstance;
        }
        else
        {
            // Remove invalid cached instance
            MaterialInstanceCache.Remove(CacheKey);
        }
    }
    
    // Create new dynamic material instance
    UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(SourceMaterial, nullptr);
    if (DynamicMaterial)
    {
        // Apply material parameters
        ApplyMaterialParameters(DynamicMaterial, MaterialBlend);
        
        // Cache the instance
        MaterialInstanceCache.Add(CacheKey, DynamicMaterial);
    }
    
    return DynamicMaterial;
}

void FBiomeMaterialSystem::ApplyMaterialParameters(UMaterialInstanceDynamic* MaterialInstance, const FBiomeMaterialBlend& MaterialBlend) const
{
    if (!MaterialInstance)
    {
        return;
    }
    
    // Apply color parameters
    MaterialInstance->SetVectorParameterValue(TEXT("BiomeColor"), MaterialBlend.BlendedColor);
    MaterialInstance->SetVectorParameterValue(TEXT("BaseColor"), MaterialBlend.BlendedColor);
    
    // Apply material property parameters
    MaterialInstance->SetScalarParameterValue(TEXT("Roughness"), MaterialBlend.BlendedRoughness);
    MaterialInstance->SetScalarParameterValue(TEXT("Metallic"), MaterialBlend.BlendedMetallic);
    
    // Apply blend factor if secondary material blending is supported
    MaterialInstance->SetScalarParameterValue(TEXT("BlendFactor"), MaterialBlend.BlendFactor);
}

UMaterialInterface* FBiomeMaterialSystem::GetFallbackMaterial() const
{
    if (FallbackMaterial.IsValid())
    {
        return FallbackMaterial.LoadSynchronous();
    }
    return nullptr;
}

void FBiomeMaterialSystem::SetFallbackMaterial(UMaterialInterface* Material)
{
    FallbackMaterial = Material;
}

FBiomeMaterialBlend FBiomeMaterialSystem::CalculateBiomeTransition(EBiomeType BiomeA, EBiomeType BiomeB, float BlendFactor, const FBiomeSystem& BiomeSystem) const
{
    FBiomeMaterialBlend MaterialBlend;
    
    const FEnhancedBiomeData& BiomeDataA = BiomeSystem.GetEnhancedBiomeData(BiomeA);
    const FEnhancedBiomeData& BiomeDataB = BiomeSystem.GetEnhancedBiomeData(BiomeB);
    
    // Set materials
    MaterialBlend.PrimaryMaterial = BiomeDataA.BiomeMaterial;
    MaterialBlend.SecondaryMaterial = BiomeDataB.BiomeMaterial;
    MaterialBlend.BlendFactor = BlendFactor;
    
    // Blend colors
    MaterialBlend.BlendedColor = BlendColors(BiomeDataA.BiomeColor, BiomeDataB.BiomeColor, BlendFactor);
    
    // Blend material properties
    MaterialBlend.BlendedRoughness = BlendScalarValues(BiomeDataA.MaterialRoughness, BiomeDataB.MaterialRoughness, BlendFactor);
    MaterialBlend.BlendedMetallic = BlendScalarValues(BiomeDataA.MaterialMetallic, BiomeDataB.MaterialMetallic, BlendFactor);
    
    return MaterialBlend;
}

bool FBiomeMaterialSystem::ValidateBiomeMaterials(const FBiomeSystem& BiomeSystem) const
{
    bool bAllValid = true;
    
    const TArray<FEnhancedBiomeData>& AllBiomes = BiomeSystem.GetAllEnhancedBiomeData();
    
    for (int32 i = 0; i < AllBiomes.Num(); ++i)
    {
        const FEnhancedBiomeData& BiomeData = AllBiomes[i];
        
        // Check if biome has either a material or a valid color
        bool bHasMaterial = BiomeData.BiomeMaterial.IsValid();
        bool bHasValidColor = !BiomeData.BiomeColor.Equals(FLinearColor::Black, 0.01f);
        
        if (!bHasMaterial && !bHasValidColor)
        {
            UE_LOG(LogTemp, Warning, TEXT("BiomeMaterialSystem: Biome '%s' has no material and no valid color"), *BiomeData.BiomeName);
            bAllValid = false;
        }
        
        // Validate material parameter ranges
        if (BiomeData.MaterialRoughness < 0.0f || BiomeData.MaterialRoughness > 1.0f)
        {
            UE_LOG(LogTemp, Warning, TEXT("BiomeMaterialSystem: Biome '%s' has invalid roughness value: %f"), *BiomeData.BiomeName, BiomeData.MaterialRoughness);
            bAllValid = false;
        }
        
        if (BiomeData.MaterialMetallic < 0.0f || BiomeData.MaterialMetallic > 1.0f)
        {
            UE_LOG(LogTemp, Warning, TEXT("BiomeMaterialSystem: Biome '%s' has invalid metallic value: %f"), *BiomeData.BiomeName, BiomeData.MaterialMetallic);
            bAllValid = false;
        }
    }
    
    return bAllValid;
}

FString FBiomeMaterialSystem::GenerateMaterialCacheKey(const FBiomeMaterialBlend& MaterialBlend) const
{
    FString PrimaryPath = MaterialBlend.PrimaryMaterial.IsValid() ? MaterialBlend.PrimaryMaterial.GetAssetName() : TEXT("None");
    FString SecondaryPath = MaterialBlend.SecondaryMaterial.IsValid() ? MaterialBlend.SecondaryMaterial.GetAssetName() : TEXT("None");
    
    return FString::Printf(TEXT("%s_%s_%.3f_%.3f_%.3f_%.3f"), 
        *PrimaryPath, 
        *SecondaryPath, 
        MaterialBlend.BlendFactor,
        MaterialBlend.BlendedRoughness,
        MaterialBlend.BlendedMetallic,
        MaterialBlend.BlendedColor.R + MaterialBlend.BlendedColor.G + MaterialBlend.BlendedColor.B
    );
}

FLinearColor FBiomeMaterialSystem::BlendColors(const FLinearColor& ColorA, const FLinearColor& ColorB, float BlendFactor) const
{
    BlendFactor = FMath::Clamp(BlendFactor, 0.0f, 1.0f);
    return FLinearColor::LerpUsingHSV(ColorA, ColorB, BlendFactor);
}

float FBiomeMaterialSystem::BlendScalarValues(float ValueA, float ValueB, float BlendFactor) const
{
    BlendFactor = FMath::Clamp(BlendFactor, 0.0f, 1.0f);
    return FMath::Lerp(ValueA, ValueB, BlendFactor);
}