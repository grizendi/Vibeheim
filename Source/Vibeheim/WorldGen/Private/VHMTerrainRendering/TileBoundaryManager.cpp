#include "VHMTerrainRendering/TileBoundaryManager.h"
#include "Services/HeightfieldService.h"
#include "Engine/Engine.h"
#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogTileBoundaryManager, Log, All);

UVHMTileBoundaryManager::UVHMTileBoundaryManager()
{
    HeightfieldService = nullptr;
    VHMSettings = FVHMSettings();
    DefaultTileSize = 64.0f;
    DefaultHeightScale = 1.0f;
    BoundaryOperationCount = 0;
}

bool UVHMTileBoundaryManager::Initialize(UHeightfieldService* InHeightfieldService, const FVHMSettings& InVHMSettings)
{
    if (!InHeightfieldService)
    {
        UE_LOG(LogTileBoundaryManager, Error, TEXT("Initialize - HeightfieldService is null"));
        return false;
    }

    HeightfieldService = InHeightfieldService;
    VHMSettings = InVHMSettings;

    UE_LOG(LogTileBoundaryManager, Log, TEXT("TileBoundaryManager initialized with texture resolution %d, boundary stitching %s"), 
           VHMSettings.HeightTextureResolution, VHMSettings.bEnableBoundaryStitching ? TEXT("enabled") : TEXT("disabled"));
    return true;
}

bool UVHMTileBoundaryManager::ExtractTileBoundaryData(const FTileCoord& TileCoord, 
                                                    const TArray<float>& HeightData, 
                                                    int32 TextureResolution,
                                                    FTileBoundaryData& OutBoundaryData)
{
    if (!ValidateHeightDataSize(HeightData, TextureResolution))
    {
        UE_LOG(LogTileBoundaryManager, Error, TEXT("ExtractTileBoundaryData - Invalid height data size for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    // Initialize boundary data
    OutBoundaryData.TileCoord = TileCoord;
    OutBoundaryData.BoundaryEdges.SetNum(4);
    OutBoundaryData.LastUpdateTime = FPlatformTime::Seconds();
    OutBoundaryData.bIsValid = false;

    // Extract edge data for each direction
    for (int32 EdgeIndex = 0; EdgeIndex < 4; ++EdgeIndex)
    {
        FTileBoundaryEdge& Edge = OutBoundaryData.BoundaryEdges[EdgeIndex];
        Edge.EdgeDirection = EdgeIndex;

        // Extract edge heights
        if (!ExtractEdgeHeights(HeightData, EdgeIndex, TextureResolution, Edge.EdgeHeights))
        {
            UE_LOG(LogTileBoundaryManager, Error, TEXT("ExtractTileBoundaryData - Failed to extract edge heights for direction %d"), EdgeIndex);
            return false;
        }

        // Calculate edge normals
        TArray<FVector> AllNormals;
        if (CalculateNormalsFromHeights(HeightData, TextureResolution, DefaultHeightScale, AllNormals))
        {
            // Extract normals for this edge
            Edge.EdgeNormals.Reset();
            for (int32 i = 0; i < Edge.EdgeHeights.Num(); ++i)
            {
                int32 NormalIndex = 0;
                switch (EdgeIndex)
                {
                case EDGE_NORTH: // Top edge
                    NormalIndex = GetHeightDataIndex(i, TextureResolution - 1, TextureResolution);
                    break;
                case EDGE_EAST: // Right edge
                    NormalIndex = GetHeightDataIndex(TextureResolution - 1, i, TextureResolution);
                    break;
                case EDGE_SOUTH: // Bottom edge
                    NormalIndex = GetHeightDataIndex(i, 0, TextureResolution);
                    break;
                case EDGE_WEST: // Left edge
                    NormalIndex = GetHeightDataIndex(0, i, TextureResolution);
                    break;
                }
                
                if (AllNormals.IsValidIndex(NormalIndex))
                {
                    Edge.EdgeNormals.Add(AllNormals[NormalIndex]);
                }
                else
                {
                    Edge.EdgeNormals.Add(FVector::UpVector);
                }
            }
        }
        else
        {
            // Fallback to up vectors
            Edge.EdgeNormals.Init(FVector::UpVector, Edge.EdgeHeights.Num());
        }

        // Generate edge UVs
        if (!GenerateEdgeUVs(EdgeIndex, Edge.EdgeHeights.Num(), DefaultTileSize, Edge.EdgeUVs))
        {
            UE_LOG(LogTileBoundaryManager, Warning, TEXT("ExtractTileBoundaryData - Failed to generate UVs for edge %d"), EdgeIndex);
            // Continue with empty UVs
        }

        // Set adjacent tile coordinate
        switch (EdgeIndex)
        {
        case EDGE_NORTH:
            Edge.AdjacentTile = FTileCoord(TileCoord.X, TileCoord.Y + 1);
            break;
        case EDGE_EAST:
            Edge.AdjacentTile = FTileCoord(TileCoord.X + 1, TileCoord.Y);
            break;
        case EDGE_SOUTH:
            Edge.AdjacentTile = FTileCoord(TileCoord.X, TileCoord.Y - 1);
            break;
        case EDGE_WEST:
            Edge.AdjacentTile = FTileCoord(TileCoord.X - 1, TileCoord.Y);
            break;
        }
    }

    OutBoundaryData.bIsValid = true;

    // Store boundary data
    TileBoundaryDataMap.Add(TileCoord, OutBoundaryData);

    UE_LOG(LogTileBoundaryManager, Verbose, TEXT("Extracted boundary data for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
    return true;
}

bool UVHMTileBoundaryManager::UpdateTileBoundaryData(const FTileCoord& TileCoord,
                                                   const TArray<FHeightfieldModification>& Modifications,
                                                   int32 TextureResolution)
{
    if (Modifications.Num() == 0)
    {
        return true; // Nothing to update
    }

    // Get current height data for the tile
    TArray<float> CurrentHeightData;
    if (!GetTileHeightData(TileCoord, CurrentHeightData))
    {
        UE_LOG(LogTileBoundaryManager, Warning, TEXT("UpdateTileBoundaryData - Could not get height data for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    // Re-extract boundary data with updated height information
    FTileBoundaryData UpdatedBoundaryData;
    if (!ExtractTileBoundaryData(TileCoord, CurrentHeightData, TextureResolution, UpdatedBoundaryData))
    {
        UE_LOG(LogTileBoundaryManager, Error, TEXT("UpdateTileBoundaryData - Failed to extract updated boundary data for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    // Notify adjacent tiles that this tile has been modified
    TArray<FTileCoord> AdjacentTiles;
    GetAdjacentTiles(TileCoord, AdjacentTiles);
    NotifyAdjacentTilesModified(AdjacentTiles);

    BoundaryOperationCount++;
    UE_LOG(LogTileBoundaryManager, Verbose, TEXT("Updated boundary data for tile (%d, %d) with %d modifications"), TileCoord.X, TileCoord.Y, Modifications.Num());
    return true;
}

bool UVHMTileBoundaryManager::StitchTileBoundaries(const FTileCoord& TileCoord,
                                                 const FTileCoord& AdjacentTileCoord,
                                                 TArray<float>& OutStitchedHeightData,
                                                 int32 TextureResolution)
{
    if (!AreTilesAdjacent(TileCoord, AdjacentTileCoord))
    {
        UE_LOG(LogTileBoundaryManager, Warning, TEXT("StitchTileBoundaries - Tiles (%d, %d) and (%d, %d) are not adjacent"), 
               TileCoord.X, TileCoord.Y, AdjacentTileCoord.X, AdjacentTileCoord.Y);
        return false;
    }

    // Get height data for both tiles
    TArray<float> PrimaryHeightData, AdjacentHeightData;
    if (!GetTileHeightData(TileCoord, PrimaryHeightData) || !GetTileHeightData(AdjacentTileCoord, AdjacentHeightData))
    {
        UE_LOG(LogTileBoundaryManager, Error, TEXT("StitchTileBoundaries - Could not get height data for tiles"));
        return false;
    }

    // Start with primary tile's height data
    OutStitchedHeightData = PrimaryHeightData;

    // Get edge direction between tiles
    uint8 EdgeDirection = GetEdgeDirection(TileCoord, AdjacentTileCoord);
    if (EdgeDirection == EDGE_INVALID)
    {
        UE_LOG(LogTileBoundaryManager, Error, TEXT("StitchTileBoundaries - Invalid edge direction between tiles"));
        return false;
    }

    // Extract edge heights from both tiles
    TArray<float> PrimaryEdgeHeights, AdjacentEdgeHeights;
    if (!ExtractEdgeHeights(PrimaryHeightData, EdgeDirection, TextureResolution, PrimaryEdgeHeights))
    {
        UE_LOG(LogTileBoundaryManager, Error, TEXT("StitchTileBoundaries - Failed to extract primary edge heights"));
        return false;
    }

    // Get the opposite edge from adjacent tile
    uint8 OppositeEdge = (EdgeDirection + 2) % 4; // North<->South, East<->West
    if (!ExtractEdgeHeights(AdjacentHeightData, OppositeEdge, TextureResolution, AdjacentEdgeHeights))
    {
        UE_LOG(LogTileBoundaryManager, Error, TEXT("StitchTileBoundaries - Failed to extract adjacent edge heights"));
        return false;
    }

    // Blend the edge heights
    TArray<float> BlendedEdgeHeights;
    if (!BlendBoundaryHeights(PrimaryEdgeHeights, AdjacentEdgeHeights, VHMSettings.BoundaryBlendFactor, BlendedEdgeHeights))
    {
        UE_LOG(LogTileBoundaryManager, Error, TEXT("StitchTileBoundaries - Failed to blend boundary heights"));
        return false;
    }

    // Apply blended heights back to the primary tile's edge
    for (int32 i = 0; i < BlendedEdgeHeights.Num() && i < TextureResolution; ++i)
    {
        int32 HeightIndex = 0;
        switch (EdgeDirection)
        {
        case EDGE_NORTH: // Top edge
            HeightIndex = GetHeightDataIndex(i, TextureResolution - 1, TextureResolution);
            break;
        case EDGE_EAST: // Right edge
            HeightIndex = GetHeightDataIndex(TextureResolution - 1, i, TextureResolution);
            break;
        case EDGE_SOUTH: // Bottom edge
            HeightIndex = GetHeightDataIndex(i, 0, TextureResolution);
            break;
        case EDGE_WEST: // Left edge
            HeightIndex = GetHeightDataIndex(0, i, TextureResolution);
            break;
        }

        if (OutStitchedHeightData.IsValidIndex(HeightIndex))
        {
            OutStitchedHeightData[HeightIndex] = BlendedEdgeHeights[i];
        }
    }

    BoundaryOperationCount++;
    UE_LOG(LogTileBoundaryManager, Verbose, TEXT("Stitched boundaries between tiles (%d, %d) and (%d, %d)"), 
           TileCoord.X, TileCoord.Y, AdjacentTileCoord.X, AdjacentTileCoord.Y);
    return true;
}

bool UVHMTileBoundaryManager::SampleTileEdgeHeights(const FTileCoord& TileCoord,
                                                  uint8 EdgeDirection,
                                                  int32 SampleCount,
                                                  TArray<float>& OutHeightSamples)
{
    if (EdgeDirection > EDGE_WEST)
    {
        UE_LOG(LogTileBoundaryManager, Error, TEXT("SampleTileEdgeHeights - Invalid edge direction %d"), EdgeDirection);
        return false;
    }

    // Get height data for the tile
    TArray<float> HeightData;
    if (!GetTileHeightData(TileCoord, HeightData))
    {
        UE_LOG(LogTileBoundaryManager, Error, TEXT("SampleTileEdgeHeights - Could not get height data for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    // Extract edge heights
    if (!ExtractEdgeHeights(HeightData, EdgeDirection, VHMSettings.HeightTextureResolution, OutHeightSamples))
    {
        UE_LOG(LogTileBoundaryManager, Error, TEXT("SampleTileEdgeHeights - Failed to extract edge heights"));
        return false;
    }

    // Resample if different sample count requested
    if (SampleCount != OutHeightSamples.Num() && SampleCount > 0)
    {
        TArray<float> ResampledHeights;
        ResampledHeights.Reserve(SampleCount);

        for (int32 i = 0; i < SampleCount; ++i)
        {
            float t = (float)i / (float)(SampleCount - 1);
            int32 SourceIndex = FMath::RoundToInt(t * (OutHeightSamples.Num() - 1));
            SourceIndex = FMath::Clamp(SourceIndex, 0, OutHeightSamples.Num() - 1);
            ResampledHeights.Add(OutHeightSamples[SourceIndex]);
        }

        OutHeightSamples = MoveTemp(ResampledHeights);
    }

    UE_LOG(LogTileBoundaryManager, Verbose, TEXT("Sampled %d edge heights for tile (%d, %d) edge %d"), 
           OutHeightSamples.Num(), TileCoord.X, TileCoord.Y, EdgeDirection);
    return true;
}

bool UVHMTileBoundaryManager::CalculateBoundaryNormals(const FTileCoord& TileCoord,
                                                     const FTileCoord& AdjacentTileCoord,
                                                     uint8 EdgeDirection,
                                                     TArray<FVector>& OutBoundaryNormals,
                                                     int32 TextureResolution)
{
    if (!AreTilesAdjacent(TileCoord, AdjacentTileCoord))
    {
        UE_LOG(LogTileBoundaryManager, Warning, TEXT("CalculateBoundaryNormals - Tiles are not adjacent"));
        return false;
    }

    // Get height data for both tiles
    TArray<float> PrimaryHeightData, AdjacentHeightData;
    if (!GetTileHeightData(TileCoord, PrimaryHeightData) || !GetTileHeightData(AdjacentTileCoord, AdjacentHeightData))
    {
        UE_LOG(LogTileBoundaryManager, Error, TEXT("CalculateBoundaryNormals - Could not get height data"));
        return false;
    }

    // Create combined height data for normal calculation
    TArray<float> CombinedHeightData;
    CombinedHeightData.SetNum(TextureResolution * TextureResolution * 2); // Space for both tiles

    // Copy primary tile data
    for (int32 i = 0; i < PrimaryHeightData.Num(); ++i)
    {
        CombinedHeightData[i] = PrimaryHeightData[i];
    }

    // Calculate normals using finite differences across tile boundary
    OutBoundaryNormals.Reset();
    OutBoundaryNormals.Reserve(TextureResolution);

    for (int32 i = 0; i < TextureResolution; ++i)
    {
        FVector Normal = FVector::UpVector;

        // Get height values for normal calculation
        float CenterHeight = 0.0f;
        float LeftHeight = 0.0f;
        float RightHeight = 0.0f;
        float UpHeight = 0.0f;
        float DownHeight = 0.0f;

        // Calculate based on edge direction
        switch (EdgeDirection)
        {
        case EDGE_NORTH: // Top edge
            {
                int32 CenterX = i;
                int32 CenterY = TextureResolution - 1;
                CenterHeight = PrimaryHeightData[GetHeightDataIndex(CenterX, CenterY, TextureResolution)];
                
                // Use adjacent tile for "up" direction
                if (AdjacentHeightData.IsValidIndex(GetHeightDataIndex(CenterX, 0, TextureResolution)))
                {
                    UpHeight = AdjacentHeightData[GetHeightDataIndex(CenterX, 0, TextureResolution)];
                }
                else
                {
                    UpHeight = CenterHeight;
                }
                
                // Use primary tile for other directions
                DownHeight = (CenterY > 0) ? PrimaryHeightData[GetHeightDataIndex(CenterX, CenterY - 1, TextureResolution)] : CenterHeight;
                LeftHeight = (CenterX > 0) ? PrimaryHeightData[GetHeightDataIndex(CenterX - 1, CenterY, TextureResolution)] : CenterHeight;
                RightHeight = (CenterX < TextureResolution - 1) ? PrimaryHeightData[GetHeightDataIndex(CenterX + 1, CenterY, TextureResolution)] : CenterHeight;
            }
            break;
        case EDGE_EAST: // Right edge
            {
                int32 CenterX = TextureResolution - 1;
                int32 CenterY = i;
                CenterHeight = PrimaryHeightData[GetHeightDataIndex(CenterX, CenterY, TextureResolution)];
                
                // Use adjacent tile for "right" direction
                if (AdjacentHeightData.IsValidIndex(GetHeightDataIndex(0, CenterY, TextureResolution)))
                {
                    RightHeight = AdjacentHeightData[GetHeightDataIndex(0, CenterY, TextureResolution)];
                }
                else
                {
                    RightHeight = CenterHeight;
                }
                
                // Use primary tile for other directions
                LeftHeight = (CenterX > 0) ? PrimaryHeightData[GetHeightDataIndex(CenterX - 1, CenterY, TextureResolution)] : CenterHeight;
                UpHeight = (CenterY < TextureResolution - 1) ? PrimaryHeightData[GetHeightDataIndex(CenterX, CenterY + 1, TextureResolution)] : CenterHeight;
                DownHeight = (CenterY > 0) ? PrimaryHeightData[GetHeightDataIndex(CenterX, CenterY - 1, TextureResolution)] : CenterHeight;
            }
            break;
        case EDGE_SOUTH: // Bottom edge
            {
                int32 CenterX = i;
                int32 CenterY = 0;
                CenterHeight = PrimaryHeightData[GetHeightDataIndex(CenterX, CenterY, TextureResolution)];
                
                // Use adjacent tile for "down" direction
                if (AdjacentHeightData.IsValidIndex(GetHeightDataIndex(CenterX, TextureResolution - 1, TextureResolution)))
                {
                    DownHeight = AdjacentHeightData[GetHeightDataIndex(CenterX, TextureResolution - 1, TextureResolution)];
                }
                else
                {
                    DownHeight = CenterHeight;
                }
                
                // Use primary tile for other directions
                UpHeight = (CenterY < TextureResolution - 1) ? PrimaryHeightData[GetHeightDataIndex(CenterX, CenterY + 1, TextureResolution)] : CenterHeight;
                LeftHeight = (CenterX > 0) ? PrimaryHeightData[GetHeightDataIndex(CenterX - 1, CenterY, TextureResolution)] : CenterHeight;
                RightHeight = (CenterX < TextureResolution - 1) ? PrimaryHeightData[GetHeightDataIndex(CenterX + 1, CenterY, TextureResolution)] : CenterHeight;
            }
            break;
        case EDGE_WEST: // Left edge
            {
                int32 CenterX = 0;
                int32 CenterY = i;
                CenterHeight = PrimaryHeightData[GetHeightDataIndex(CenterX, CenterY, TextureResolution)];
                
                // Use adjacent tile for "left" direction
                if (AdjacentHeightData.IsValidIndex(GetHeightDataIndex(TextureResolution - 1, CenterY, TextureResolution)))
                {
                    LeftHeight = AdjacentHeightData[GetHeightDataIndex(TextureResolution - 1, CenterY, TextureResolution)];
                }
                else
                {
                    LeftHeight = CenterHeight;
                }
                
                // Use primary tile for other directions
                RightHeight = (CenterX < TextureResolution - 1) ? PrimaryHeightData[GetHeightDataIndex(CenterX + 1, CenterY, TextureResolution)] : CenterHeight;
                UpHeight = (CenterY < TextureResolution - 1) ? PrimaryHeightData[GetHeightDataIndex(CenterX, CenterY + 1, TextureResolution)] : CenterHeight;
                DownHeight = (CenterY > 0) ? PrimaryHeightData[GetHeightDataIndex(CenterX, CenterY - 1, TextureResolution)] : CenterHeight;
            }
            break;
        }

        // Calculate normal using finite differences
        FVector Tangent1 = FVector(2.0f, 0.0f, (RightHeight - LeftHeight) * DefaultHeightScale);
        FVector Tangent2 = FVector(0.0f, 2.0f, (UpHeight - DownHeight) * DefaultHeightScale);
        Normal = FVector::CrossProduct(Tangent1, Tangent2).GetSafeNormal();

        OutBoundaryNormals.Add(Normal);
    }

    UE_LOG(LogTileBoundaryManager, Verbose, TEXT("Calculated %d boundary normals between tiles (%d, %d) and (%d, %d)"), 
           OutBoundaryNormals.Num(), TileCoord.X, TileCoord.Y, AdjacentTileCoord.X, AdjacentTileCoord.Y);
    return true;
}

bool UVHMTileBoundaryManager::CreateBoundaryUVMapping(const FTileCoord& TileCoord,
                                                    const FTileCoord& AdjacentTileCoord,
                                                    uint8 EdgeDirection,
                                                    TArray<FVector2D>& OutBoundaryUVs,
                                                    int32 TextureResolution)
{
    if (!AreTilesAdjacent(TileCoord, AdjacentTileCoord))
    {
        UE_LOG(LogTileBoundaryManager, Warning, TEXT("CreateBoundaryUVMapping - Tiles are not adjacent"));
        return false;
    }

    // Generate UV coordinates for the boundary edge
    if (!GenerateEdgeUVs(EdgeDirection, TextureResolution, DefaultTileSize, OutBoundaryUVs))
    {
        UE_LOG(LogTileBoundaryManager, Error, TEXT("CreateBoundaryUVMapping - Failed to generate edge UVs"));
        return false;
    }

    // Adjust UVs for seamless material transitions
    // This ensures that materials blend correctly across tile boundaries
    for (FVector2D& UV : OutBoundaryUVs)
    {
        // Normalize UV coordinates to tile space
        UV.X = FMath::Fmod(UV.X, 1.0f);
        UV.Y = FMath::Fmod(UV.Y, 1.0f);
        
        // Ensure UVs are positive
        if (UV.X < 0.0f) UV.X += 1.0f;
        if (UV.Y < 0.0f) UV.Y += 1.0f;
    }

    UE_LOG(LogTileBoundaryManager, Verbose, TEXT("Created %d boundary UV mappings between tiles (%d, %d) and (%d, %d)"), 
           OutBoundaryUVs.Num(), TileCoord.X, TileCoord.Y, AdjacentTileCoord.X, AdjacentTileCoord.Y);
    return true;
}

void UVHMTileBoundaryManager::GetAdjacentTiles(const FTileCoord& TileCoord, TArray<FTileCoord>& OutAdjacentTiles)
{
    OutAdjacentTiles.Reset();
    OutAdjacentTiles.Reserve(4);

    // Add all four adjacent tiles (North, East, South, West)
    OutAdjacentTiles.Add(FTileCoord(TileCoord.X, TileCoord.Y + 1)); // North
    OutAdjacentTiles.Add(FTileCoord(TileCoord.X + 1, TileCoord.Y)); // East
    OutAdjacentTiles.Add(FTileCoord(TileCoord.X, TileCoord.Y - 1)); // South
    OutAdjacentTiles.Add(FTileCoord(TileCoord.X - 1, TileCoord.Y)); // West
}

bool UVHMTileBoundaryManager::AreTilesAdjacent(const FTileCoord& TileA, const FTileCoord& TileB)
{
    int32 DeltaX = FMath::Abs(TileA.X - TileB.X);
    int32 DeltaY = FMath::Abs(TileA.Y - TileB.Y);

    // Tiles are adjacent if they differ by 1 in exactly one dimension
    return (DeltaX == 1 && DeltaY == 0) || (DeltaX == 0 && DeltaY == 1);
}

uint8 UVHMTileBoundaryManager::GetEdgeDirection(const FTileCoord& FromTile, const FTileCoord& ToTile)
{
    if (!AreTilesAdjacent(FromTile, ToTile))
    {
        return EDGE_INVALID;
    }

    int32 DeltaX = ToTile.X - FromTile.X;
    int32 DeltaY = ToTile.Y - FromTile.Y;

    if (DeltaY == 1) return EDGE_NORTH;  // ToTile is north of FromTile
    if (DeltaX == 1) return EDGE_EAST;   // ToTile is east of FromTile
    if (DeltaY == -1) return EDGE_SOUTH; // ToTile is south of FromTile
    if (DeltaX == -1) return EDGE_WEST;  // ToTile is west of FromTile

    return EDGE_INVALID;
}

void UVHMTileBoundaryManager::NotifyAdjacentTilesModified(const TArray<FTileCoord>& ModifiedTiles)
{
    for (const FTileCoord& ModifiedTile : ModifiedTiles)
    {
        // Get all tiles adjacent to the modified tile
        TArray<FTileCoord> AdjacentTiles;
        GetAdjacentTiles(ModifiedTile, AdjacentTiles);

        // Mark boundary data as needing update for adjacent tiles
        for (const FTileCoord& AdjacentTile : AdjacentTiles)
        {
            if (FTileBoundaryData* BoundaryData = TileBoundaryDataMap.Find(AdjacentTile))
            {
                BoundaryData->bIsValid = false; // Mark for re-extraction
                BoundaryData->LastUpdateTime = FPlatformTime::Seconds();
            }
        }
    }

    UE_LOG(LogTileBoundaryManager, Verbose, TEXT("Notified boundary updates for %d modified tiles"), ModifiedTiles.Num());
}

bool UVHMTileBoundaryManager::GetTileBoundaryData(const FTileCoord& TileCoord, FTileBoundaryData& OutBoundaryData)
{
    if (const FTileBoundaryData* BoundaryData = TileBoundaryDataMap.Find(TileCoord))
    {
        OutBoundaryData = *BoundaryData;
        return BoundaryData->bIsValid;
    }

    return false;
}

void UVHMTileBoundaryManager::ClearTileBoundaryData(const FTileCoord& TileCoord)
{
    TileBoundaryDataMap.Remove(TileCoord);
    UE_LOG(LogTileBoundaryManager, Verbose, TEXT("Cleared boundary data for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
}

void UVHMTileBoundaryManager::ClearAllBoundaryData()
{
    int32 ClearedCount = TileBoundaryDataMap.Num();
    TileBoundaryDataMap.Empty();
    BoundaryOperationCount = 0;
    UE_LOG(LogTileBoundaryManager, Log, TEXT("Cleared all boundary data (%d tiles)"), ClearedCount);
}

// Protected helper methods

bool UVHMTileBoundaryManager::ExtractEdgeHeights(const TArray<float>& HeightData, 
                                                uint8 EdgeDirection, 
                                                int32 TextureResolution,
                                                TArray<float>& OutEdgeHeights)
{
    if (!ValidateHeightDataSize(HeightData, TextureResolution))
    {
        return false;
    }

    OutEdgeHeights.Reset();
    OutEdgeHeights.Reserve(TextureResolution);

    switch (EdgeDirection)
    {
    case EDGE_NORTH: // Top edge (Y = TextureResolution - 1)
        for (int32 X = 0; X < TextureResolution; ++X)
        {
            int32 Index = GetHeightDataIndex(X, TextureResolution - 1, TextureResolution);
            OutEdgeHeights.Add(HeightData[Index]);
        }
        break;
    case EDGE_EAST: // Right edge (X = TextureResolution - 1)
        for (int32 Y = 0; Y < TextureResolution; ++Y)
        {
            int32 Index = GetHeightDataIndex(TextureResolution - 1, Y, TextureResolution);
            OutEdgeHeights.Add(HeightData[Index]);
        }
        break;
    case EDGE_SOUTH: // Bottom edge (Y = 0)
        for (int32 X = 0; X < TextureResolution; ++X)
        {
            int32 Index = GetHeightDataIndex(X, 0, TextureResolution);
            OutEdgeHeights.Add(HeightData[Index]);
        }
        break;
    case EDGE_WEST: // Left edge (X = 0)
        for (int32 Y = 0; Y < TextureResolution; ++Y)
        {
            int32 Index = GetHeightDataIndex(0, Y, TextureResolution);
            OutEdgeHeights.Add(HeightData[Index]);
        }
        break;
    default:
        UE_LOG(LogTileBoundaryManager, Error, TEXT("ExtractEdgeHeights - Invalid edge direction %d"), EdgeDirection);
        return false;
    }

    return true;
}

bool UVHMTileBoundaryManager::CalculateNormalsFromHeights(const TArray<float>& HeightData,
                                                        int32 TextureResolution,
                                                        float HeightScale,
                                                        TArray<FVector>& OutNormals)
{
    if (!ValidateHeightDataSize(HeightData, TextureResolution))
    {
        return false;
    }

    OutNormals.Reset();
    OutNormals.Reserve(HeightData.Num());

    for (int32 Y = 0; Y < TextureResolution; ++Y)
    {
        for (int32 X = 0; X < TextureResolution; ++X)
        {
            // Get neighboring height values
            float CenterHeight = HeightData[GetHeightDataIndex(X, Y, TextureResolution)];
            
            float LeftHeight = (X > 0) ? HeightData[GetHeightDataIndex(X - 1, Y, TextureResolution)] : CenterHeight;
            float RightHeight = (X < TextureResolution - 1) ? HeightData[GetHeightDataIndex(X + 1, Y, TextureResolution)] : CenterHeight;
            float DownHeight = (Y > 0) ? HeightData[GetHeightDataIndex(X, Y - 1, TextureResolution)] : CenterHeight;
            float UpHeight = (Y < TextureResolution - 1) ? HeightData[GetHeightDataIndex(X, Y + 1, TextureResolution)] : CenterHeight;

            // Calculate normal using finite differences
            FVector Tangent1 = FVector(2.0f, 0.0f, (RightHeight - LeftHeight) * HeightScale);
            FVector Tangent2 = FVector(0.0f, 2.0f, (UpHeight - DownHeight) * HeightScale);
            FVector Normal = FVector::CrossProduct(Tangent1, Tangent2).GetSafeNormal();

            OutNormals.Add(Normal);
        }
    }

    return true;
}

bool UVHMTileBoundaryManager::GenerateEdgeUVs(uint8 EdgeDirection,
                                            int32 SampleCount,
                                            float TileSize,
                                            TArray<FVector2D>& OutUVs)
{
    if (SampleCount <= 0)
    {
        return false;
    }

    OutUVs.Reset();
    OutUVs.Reserve(SampleCount);

    for (int32 i = 0; i < SampleCount; ++i)
    {
        float t = (float)i / (float)(SampleCount - 1);
        FVector2D UV;

        switch (EdgeDirection)
        {
        case EDGE_NORTH: // Top edge
            UV = FVector2D(t, 1.0f);
            break;
        case EDGE_EAST: // Right edge
            UV = FVector2D(1.0f, t);
            break;
        case EDGE_SOUTH: // Bottom edge
            UV = FVector2D(t, 0.0f);
            break;
        case EDGE_WEST: // Left edge
            UV = FVector2D(0.0f, t);
            break;
        default:
            UV = FVector2D(0.5f, 0.5f);
            break;
        }

        OutUVs.Add(UV);
    }

    return true;
}

bool UVHMTileBoundaryManager::BlendBoundaryHeights(const TArray<float>& PrimaryHeights,
                                                 const TArray<float>& AdjacentHeights,
                                                 float BlendFactor,
                                                 TArray<float>& OutBlendedHeights)
{
    if (PrimaryHeights.Num() != AdjacentHeights.Num())
    {
        UE_LOG(LogTileBoundaryManager, Error, TEXT("BlendBoundaryHeights - Height array size mismatch"));
        return false;
    }

    OutBlendedHeights.Reset();
    OutBlendedHeights.Reserve(PrimaryHeights.Num());

    BlendFactor = FMath::Clamp(BlendFactor, 0.0f, 1.0f);

    for (int32 i = 0; i < PrimaryHeights.Num(); ++i)
    {
        float BlendedHeight = FMath::Lerp(PrimaryHeights[i], AdjacentHeights[i], BlendFactor);
        OutBlendedHeights.Add(BlendedHeight);
    }

    return true;
}

bool UVHMTileBoundaryManager::GetTileHeightData(const FTileCoord& TileCoord, TArray<float>& OutHeightData)
{
    if (!HeightfieldService)
    {
        UE_LOG(LogTileBoundaryManager, Error, TEXT("GetTileHeightData - HeightfieldService is null"));
        return false;
    }

    FHeightfieldData HeightfieldData;
    if (!HeightfieldService->GetCachedHeightfield(TileCoord, HeightfieldData))
    {
        UE_LOG(LogTileBoundaryManager, Warning, TEXT("GetTileHeightData - No heightfield data for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    OutHeightData = HeightfieldData.HeightData;
    return true;
}

bool UVHMTileBoundaryManager::ValidateHeightDataSize(const TArray<float>& HeightData, int32 ExpectedResolution)
{
    int32 ExpectedSize = ExpectedResolution * ExpectedResolution;
    if (HeightData.Num() != ExpectedSize)
    {
        UE_LOG(LogTileBoundaryManager, Error, TEXT("ValidateHeightDataSize - Size mismatch. Expected %d, got %d"), ExpectedSize, HeightData.Num());
        return false;
    }
    return true;
}

int32 UVHMTileBoundaryManager::GetHeightDataIndex(int32 X, int32 Y, int32 TextureResolution)
{
    return Y * TextureResolution + X;
}
