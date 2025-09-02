#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "VHMTypes.h"
#include "Data/WorldGenTypes.h"
#include "ITileBoundaryManager.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UTileBoundaryManager : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for managing seamless tile boundary handling in VHM terrain rendering
 * Handles mesh stitching, height data sampling, and normal calculation across tile boundaries
 */
class VIBEHEIM_API ITileBoundaryManager
{
    GENERATED_BODY()

public:
    /**
     * Extract boundary data from a tile's heightfield data
     * @param TileCoord Tile coordinate to extract boundary data from
     * @param HeightData Height data for the tile
     * @param TextureResolution Resolution of the height texture
     * @param OutBoundaryData Output boundary data structure
     * @return True if boundary data was extracted successfully
     */
    virtual bool ExtractTileBoundaryData(const FTileCoord& TileCoord, 
                                       const TArray<float>& HeightData, 
                                       int32 TextureResolution,
                                       FTileBoundaryData& OutBoundaryData) = 0;

    /**
     * Update boundary data when a tile's heightfield is modified
     * @param TileCoord Tile coordinate that was modified
     * @param Modifications Array of modifications applied to the tile
     * @param TextureResolution Resolution of the height texture
     * @return True if boundary data was updated successfully
     */
    virtual bool UpdateTileBoundaryData(const FTileCoord& TileCoord,
                                      const TArray<FHeightfieldModification>& Modifications,
                                      int32 TextureResolution) = 0;

    /**
     * Stitch mesh boundaries between adjacent tiles
     * @param TileCoord Primary tile coordinate
     * @param AdjacentTileCoord Adjacent tile coordinate
     * @param OutStitchedHeightData Output height data with stitched boundaries
     * @param TextureResolution Resolution of the height texture
     * @return True if mesh stitching was successful
     */
    virtual bool StitchTileBoundaries(const FTileCoord& TileCoord,
                                    const FTileCoord& AdjacentTileCoord,
                                    TArray<float>& OutStitchedHeightData,
                                    int32 TextureResolution) = 0;

    /**
     * Sample height data at tile edges for seamless transitions
     * @param TileCoord Tile coordinate to sample from
     * @param EdgeDirection Edge direction (0=North, 1=East, 2=South, 3=West)
     * @param SampleCount Number of samples along the edge
     * @param OutHeightSamples Output height samples along the edge
     * @return True if height sampling was successful
     */
    virtual bool SampleTileEdgeHeights(const FTileCoord& TileCoord,
                                     uint8 EdgeDirection,
                                     int32 SampleCount,
                                     TArray<float>& OutHeightSamples) = 0;

    /**
     * Calculate normal vectors across tile boundaries
     * @param TileCoord Primary tile coordinate
     * @param AdjacentTileCoord Adjacent tile coordinate
     * @param EdgeDirection Edge direction between tiles
     * @param OutBoundaryNormals Output normal vectors along the boundary
     * @param TextureResolution Resolution of the height texture
     * @return True if normal calculation was successful
     */
    virtual bool CalculateBoundaryNormals(const FTileCoord& TileCoord,
                                        const FTileCoord& AdjacentTileCoord,
                                        uint8 EdgeDirection,
                                        TArray<FVector>& OutBoundaryNormals,
                                        int32 TextureResolution) = 0;

    /**
     * Create texture coordinate mapping for consistent material application across boundaries
     * @param TileCoord Primary tile coordinate
     * @param AdjacentTileCoord Adjacent tile coordinate
     * @param EdgeDirection Edge direction between tiles
     * @param OutBoundaryUVs Output UV coordinates along the boundary
     * @param TextureResolution Resolution of the height texture
     * @return True if UV mapping was successful
     */
    virtual bool CreateBoundaryUVMapping(const FTileCoord& TileCoord,
                                       const FTileCoord& AdjacentTileCoord,
                                       uint8 EdgeDirection,
                                       TArray<FVector2D>& OutBoundaryUVs,
                                       int32 TextureResolution) = 0;

    /**
     * Get adjacent tile coordinates for a given tile
     * @param TileCoord Tile coordinate to get adjacent tiles for
     * @param OutAdjacentTiles Output array of adjacent tile coordinates
     */
    virtual void GetAdjacentTiles(const FTileCoord& TileCoord, TArray<FTileCoord>& OutAdjacentTiles) = 0;

    /**
     * Check if two tiles are adjacent
     * @param TileA First tile coordinate
     * @param TileB Second tile coordinate
     * @return True if tiles are adjacent
     */
    virtual bool AreTilesAdjacent(const FTileCoord& TileA, const FTileCoord& TileB) = 0;

    /**
     * Get the edge direction between two adjacent tiles
     * @param FromTile Source tile coordinate
     * @param ToTile Target tile coordinate
     * @return Edge direction (0=North, 1=East, 2=South, 3=West) or 255 if not adjacent
     */
    virtual uint8 GetEdgeDirection(const FTileCoord& FromTile, const FTileCoord& ToTile) = 0;

    /**
     * Notify that adjacent tiles have been modified and boundaries need updating
     * @param ModifiedTiles Array of tile coordinates that were modified
     */
    virtual void NotifyAdjacentTilesModified(const TArray<FTileCoord>& ModifiedTiles) = 0;

    /**
     * Get boundary data for a specific tile
     * @param TileCoord Tile coordinate to get boundary data for
     * @param OutBoundaryData Output boundary data structure
     * @return True if boundary data exists and was retrieved
     */
    virtual bool GetTileBoundaryData(const FTileCoord& TileCoord, FTileBoundaryData& OutBoundaryData) = 0;

    /**
     * Clear boundary data for a specific tile
     * @param TileCoord Tile coordinate to clear boundary data for
     */
    virtual void ClearTileBoundaryData(const FTileCoord& TileCoord) = 0;

    /**
     * Clear all boundary data
     */
    virtual void ClearAllBoundaryData() = 0;
};
