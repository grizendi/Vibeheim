#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ITileBoundaryManager.h"
#include "VHMTypes.h"
#include "Data/WorldGenTypes.h"
#include "TileBoundaryManager.generated.h"

// Forward declarations
class UHeightfieldService;

/**
 * Concrete implementation of ITileBoundaryManager
 * Manages seamless tile boundary handling for VHM terrain rendering
 */
UCLASS(BlueprintType)
class VIBEHEIM_API UVHMTileBoundaryManager : public UObject, public ITileBoundaryManager
{
    GENERATED_BODY()

public:
    UVHMTileBoundaryManager();

    /**
     * Initialize the boundary manager with heightfield service
     * @param InHeightfieldService Reference to heightfield service for data access
     * @param InVHMSettings VHM settings containing boundary configuration
     * @return True if initialization succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "Boundary")
    bool Initialize(UHeightfieldService* InHeightfieldService, const FVHMSettings& InVHMSettings);

    // ITileBoundaryManager interface
    virtual bool ExtractTileBoundaryData(const FTileCoord& TileCoord, 
                                       const TArray<float>& HeightData, 
                                       int32 TextureResolution,
                                       FTileBoundaryData& OutBoundaryData) override;

    virtual bool UpdateTileBoundaryData(const FTileCoord& TileCoord,
                                      const TArray<FHeightfieldModification>& Modifications,
                                      int32 TextureResolution) override;

    virtual bool StitchTileBoundaries(const FTileCoord& TileCoord,
                                    const FTileCoord& AdjacentTileCoord,
                                    TArray<float>& OutStitchedHeightData,
                                    int32 TextureResolution) override;

    virtual bool SampleTileEdgeHeights(const FTileCoord& TileCoord,
                                     uint8 EdgeDirection,
                                     int32 SampleCount,
                                     TArray<float>& OutHeightSamples) override;

    virtual bool CalculateBoundaryNormals(const FTileCoord& TileCoord,
                                        const FTileCoord& AdjacentTileCoord,
                                        uint8 EdgeDirection,
                                        TArray<FVector>& OutBoundaryNormals,
                                        int32 TextureResolution) override;

    virtual bool CreateBoundaryUVMapping(const FTileCoord& TileCoord,
                                       const FTileCoord& AdjacentTileCoord,
                                       uint8 EdgeDirection,
                                       TArray<FVector2D>& OutBoundaryUVs,
                                       int32 TextureResolution) override;

    virtual void GetAdjacentTiles(const FTileCoord& TileCoord, TArray<FTileCoord>& OutAdjacentTiles) override;

    virtual bool AreTilesAdjacent(const FTileCoord& TileA, const FTileCoord& TileB) override;

    virtual uint8 GetEdgeDirection(const FTileCoord& FromTile, const FTileCoord& ToTile) override;

    virtual void NotifyAdjacentTilesModified(const TArray<FTileCoord>& ModifiedTiles) override;

    virtual bool GetTileBoundaryData(const FTileCoord& TileCoord, FTileBoundaryData& OutBoundaryData) override;

    virtual void ClearTileBoundaryData(const FTileCoord& TileCoord) override;

    virtual void ClearAllBoundaryData() override;

    /**
     * Get statistics about boundary operations
     * @return Number of boundary stitching operations performed
     */
    UFUNCTION(BlueprintCallable, Category = "Boundary")
    int32 GetBoundaryOperationCount() const { return BoundaryOperationCount; }

protected:
    /**
     * Extract edge heights from heightfield data
     * @param HeightData Source height data
     * @param EdgeDirection Edge direction to extract
     * @param TextureResolution Resolution of the height texture
     * @param OutEdgeHeights Output edge height values
     * @return True if extraction succeeded
     */
    bool ExtractEdgeHeights(const TArray<float>& HeightData, 
                          uint8 EdgeDirection, 
                          int32 TextureResolution,
                          TArray<float>& OutEdgeHeights);

    /**
     * Calculate normal vectors from height data
     * @param HeightData Source height data
     * @param TextureResolution Resolution of the height texture
     * @param HeightScale Scale factor for height differences
     * @param OutNormals Output normal vectors
     * @return True if calculation succeeded
     */
    bool CalculateNormalsFromHeights(const TArray<float>& HeightData,
                                   int32 TextureResolution,
                                   float HeightScale,
                                   TArray<FVector>& OutNormals);

    /**
     * Generate UV coordinates for a tile edge
     * @param EdgeDirection Edge direction
     * @param SampleCount Number of samples along the edge
     * @param TileSize Size of the tile in world units
     * @param OutUVs Output UV coordinates
     * @return True if generation succeeded
     */
    bool GenerateEdgeUVs(uint8 EdgeDirection,
                       int32 SampleCount,
                       float TileSize,
                       TArray<FVector2D>& OutUVs);

    /**
     * Blend height values between two tiles at their boundary
     * @param PrimaryHeights Height values from primary tile
     * @param AdjacentHeights Height values from adjacent tile
     * @param BlendFactor Blend factor (0.0 = primary, 1.0 = adjacent)
     * @param OutBlendedHeights Output blended height values
     * @return True if blending succeeded
     */
    bool BlendBoundaryHeights(const TArray<float>& PrimaryHeights,
                            const TArray<float>& AdjacentHeights,
                            float BlendFactor,
                            TArray<float>& OutBlendedHeights);

    /**
     * Get height data for a specific tile from the heightfield service
     * @param TileCoord Tile coordinate to get data for
     * @param OutHeightData Output height data
     * @return True if height data was retrieved
     */
    bool GetTileHeightData(const FTileCoord& TileCoord, TArray<float>& OutHeightData);

    /**
     * Validate that height data size matches expected resolution
     * @param HeightData Height data to validate
     * @param ExpectedResolution Expected texture resolution
     * @return True if data size is valid
     */
    bool ValidateHeightDataSize(const TArray<float>& HeightData, int32 ExpectedResolution);

    /**
     * Convert 2D texture coordinates to 1D array index
     * @param X X coordinate in texture
     * @param Y Y coordinate in texture
     * @param TextureResolution Resolution of the texture
     * @return 1D array index
     */
    int32 GetHeightDataIndex(int32 X, int32 Y, int32 TextureResolution);

private:
    // Service references
    UPROPERTY()
    UHeightfieldService* HeightfieldService;

    // Boundary data storage
    UPROPERTY()
    TMap<FTileCoord, FTileBoundaryData> TileBoundaryDataMap;

    // VHM settings
    UPROPERTY()
    FVHMSettings VHMSettings;

    // Configuration
    float DefaultTileSize = 64.0f;
    float DefaultHeightScale = 1.0f;

    // Performance tracking
    int32 BoundaryOperationCount = 0;

    // Constants for edge directions
    static constexpr uint8 EDGE_NORTH = 0;
    static constexpr uint8 EDGE_EAST = 1;
    static constexpr uint8 EDGE_SOUTH = 2;
    static constexpr uint8 EDGE_WEST = 3;
    static constexpr uint8 EDGE_INVALID = 255;
};