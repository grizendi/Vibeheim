#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/Texture2D.h"
#include "PixelFormat.h"
#include "IHeightfieldTextureManager.h"
#include "VHMTypes.h"
#include "Data/WorldGenTypes.h"
#include "HeightfieldTextureManager.generated.h"

/**
 * Concrete implementation of IHeightfieldTextureManager
 * Handles conversion of heightfield data to GPU textures for VHM rendering
 * Supports both R16F and R32F texture formats for optimal GPU performance
 */
UCLASS(BlueprintType)
class VIBEHEIM_API UVHMHeightfieldTextureManager : public UObject, public IHeightfieldTextureManager
{
    GENERATED_BODY()

public:
    UVHMHeightfieldTextureManager();

    // IHeightfieldTextureManager interface
    virtual UTexture2D* CreateHeightTexture(const FTileCoord& TileCoord, const TArray<float>& HeightData) override;
    virtual bool UpdateHeightTexture(const FTileCoord& TileCoord, const TArray<FHeightfieldModification>& Modifications) override;
    virtual UTexture2D* CreateNormalTexture(const FTileCoord& TileCoord, const TArray<float>& HeightData) override;
    virtual UTexture2D* GetHeightTexture(const FTileCoord& TileCoord) override;
    virtual void RemoveTexture(const FTileCoord& TileCoord) override;
    virtual void OptimizeTextureMemory() override;
    virtual float GetTextureMemoryUsageMB() const override;

    /**
     * Initialize the texture manager with VHM settings
     * @param Settings VHM configuration settings
     * @return True if initialization succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "VHM")
    bool Initialize(const FVHMSettings& Settings);

    /**
     * Get texture format being used for height textures
     * @return Current texture format (R16F or R32F)
     */
    UFUNCTION(BlueprintCallable, Category = "VHM")
    EPixelFormat GetHeightTextureFormat() const { return HeightTextureFormat; }

    /**
     * Get number of active textures
     * @return Count of currently managed textures
     */
    UFUNCTION(BlueprintCallable, Category = "VHM")
    int32 GetActiveTextureCount() const { return HeightTextures.Num(); }

protected:
    /**
     * Create texture with specified format and data
     * @param TileCoord Tile coordinate for naming
     * @param TextureData Raw texture data
     * @param Format Pixel format to use
     * @param bIsNormalMap Whether this is a normal map texture
     * @return Created texture or nullptr on failure
     */
    UTexture2D* CreateTextureInternal(const FTileCoord& TileCoord, const TArray<uint8>& TextureData, EPixelFormat Format, bool bIsNormalMap = false);

    /**
     * Convert float height data to texture format
     * @param HeightData Input height values
     * @param OutTextureData Output texture data in target format
     * @param Format Target pixel format
     * @return True if conversion succeeded
     */
    bool ConvertHeightDataToTexture(const TArray<float>& HeightData, TArray<uint8>& OutTextureData, EPixelFormat Format);

    /**
     * Generate normal map data from height data
     * @param HeightData Input height values
     * @param OutNormalData Output normal map data (RGB8)
     * @param TextureResolution Resolution of the texture (width/height)
     * @param HeightScale Scale factor for height differences
     * @return True if generation succeeded
     */
    bool GenerateNormalMapData(const TArray<float>& HeightData, TArray<uint8>& OutNormalData, int32 TextureResolution, float HeightScale = 1.0f);

    /**
     * Apply heightfield modifications to existing texture data
     * @param TileCoord Tile coordinate
     * @param Modifications Array of modifications to apply
     * @param InOutHeightData Height data to modify
     * @return True if modifications were applied successfully
     */
    bool ApplyModificationsToHeightData(const FTileCoord& TileCoord, const TArray<FHeightfieldModification>& Modifications, TArray<float>& InOutHeightData);

    /**
     * Update texture with new data
     * @param Texture Texture to update
     * @param NewTextureData New texture data
     * @param Format Pixel format of the data
     * @return True if update succeeded
     */
    bool UpdateTextureData(UTexture2D* Texture, const TArray<uint8>& NewTextureData, EPixelFormat Format);

    /**
     * Calculate memory usage for a single texture
     * @param Format Pixel format
     * @param Resolution Texture resolution (width/height)
     * @return Memory usage in bytes
     */
    int32 CalculateTextureMemoryUsage(EPixelFormat Format, int32 Resolution) const;

private:
    // VHM settings
    UPROPERTY()
    FVHMSettings VHMSettings;

    // Texture storage
    UPROPERTY()
    TMap<FTileCoord, TObjectPtr<UTexture2D>> HeightTextures;

    UPROPERTY()
    TMap<FTileCoord, TObjectPtr<UTexture2D>> NormalTextures;

    // Cached height data for modifications (not serialized)
    TMap<FTileCoord, TArray<float>> CachedHeightData;

    // Texture format settings
    EPixelFormat HeightTextureFormat = PF_R16F;
    EPixelFormat NormalTextureFormat = PF_B8G8R8A8;

    // Performance tracking
    mutable float CachedMemoryUsageMB = 0.0f;
    mutable bool bMemoryUsageDirty = true;

    // Constants
    static constexpr int32 DefaultTextureResolution = 64;
    static constexpr float DefaultHeightScale = 1.0f;
};