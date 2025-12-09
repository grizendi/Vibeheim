#include "Data/WorldGenTerrainResource.h"

#include "Engine/Texture2D.h"
#include "Logging/LogMacros.h"
#include "Math/Float16.h"
#include "PixelFormat.h"
#include "Rendering/Texture2DResource.h"
#include "TextureResource.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorldGenTerrainResource, Log, All);

namespace
{
FIntPoint ToTileKey(const FTileCoord& Tile)
{
    return FIntPoint(Tile.X, Tile.Y);
}

FVector2D GetTileOrigin(const UWorldGenTerrainResource& Resource, const FTileCoord& Tile)
{
    return Resource.WorldOrigin + FVector2D(Tile.X * Resource.TileSizeMeters, Tile.Y * Resource.TileSizeMeters);
}

FIntPoint ComputePixelCoords(const UWorldGenTerrainResource& Resource, const FVector& WorldPos, const FTileCoord& Tile, int32 Width, int32 Height)
{
    if (Width <= 0 || Height <= 0)
    {
        return FIntPoint::ZeroValue;
    }

    const FVector2D TileOrigin = GetTileOrigin(Resource, Tile);
    const FVector2D Local = FVector2D(WorldPos.X, WorldPos.Y) - TileOrigin;
    const float U = (Resource.TileSizeMeters > KINDA_SMALL_NUMBER) ? Local.X / Resource.TileSizeMeters : 0.0f;
    const float V = (Resource.TileSizeMeters > KINDA_SMALL_NUMBER) ? Local.Y / Resource.TileSizeMeters : 0.0f;

    const int32 PixelX = FMath::Clamp(FMath::FloorToInt(U * Width), 0, Width - 1);
    const int32 PixelY = FMath::Clamp(FMath::FloorToInt(V * Height), 0, Height - 1);
    return FIntPoint(PixelX, PixelY);
}

float SampleHeightFromPlatformData(UTexture2D* Texture, const FIntPoint& Pixel, float SeaLevelFallback)
{
    if (!Texture)
    {
        return SeaLevelFallback;
    }

    FTexturePlatformData* PlatformData = Texture->GetPlatformData();
    if (!PlatformData || PlatformData->Mips.Num() == 0)
    {
        return SeaLevelFallback;
    }

    const FTexture2DMipMap& Mip = PlatformData->Mips[0];
    if (Mip.BulkData.GetBulkDataSize() <= 0 || Mip.SizeX <= 0 || Mip.SizeY <= 0)
    {
        return SeaLevelFallback;
    }

    const int32 ClampedX = FMath::Clamp(Pixel.X, 0, Mip.SizeX - 1);
    const int32 ClampedY = FMath::Clamp(Pixel.Y, 0, Mip.SizeY - 1);
    const int32 PixelIndex = ClampedY * Mip.SizeX + ClampedX;

    float HeightValue = SeaLevelFallback;
    const EPixelFormat PixelFormat = PlatformData->PixelFormat;
    const uint8* RawData = static_cast<const uint8*>(Mip.BulkData.LockReadOnly());
    if (PixelFormat == PF_R32_FLOAT)
    {
        const float* FloatData = reinterpret_cast<const float*>(RawData);
        HeightValue = FloatData[PixelIndex];
    }
    else if (PixelFormat == PF_R16F)
    {
        const FFloat16* FloatData = reinterpret_cast<const FFloat16*>(RawData);
        HeightValue = FloatData[PixelIndex].GetFloat();
    }
    else
    {
        // Unsupported format for direct sampling
        HeightValue = SeaLevelFallback;
    }
    Mip.BulkData.Unlock();
    return HeightValue;
}

float SampleHeightFromSource(UTexture2D* Texture, const FIntPoint& Pixel, float SeaLevelFallback)
{
    if (!Texture)
    {
        return SeaLevelFallback;
    }

    const int32 SourceWidth = Texture->Source.GetSizeX();
    const int32 SourceHeight = Texture->Source.GetSizeY();
    if (SourceWidth <= 0 || SourceHeight <= 0)
    {
        return SeaLevelFallback;
    }

    const ETextureSourceFormat SourceFormat = Texture->Source.GetFormat();
    const uint8* RawData = Texture->Source.LockMipReadOnly(0);
    if (!RawData)
    {
        return SeaLevelFallback;
    }

    const int32 ClampedX = FMath::Clamp(Pixel.X, 0, SourceWidth - 1);
    const int32 ClampedY = FMath::Clamp(Pixel.Y, 0, SourceHeight - 1);
    const int32 PixelIndex = ClampedY * SourceWidth + ClampedX;

    float HeightValue = SeaLevelFallback;
    switch (SourceFormat)
    {
        case TSF_R32F:
        {
            const float* FloatData = reinterpret_cast<const float*>(RawData);
            HeightValue = FloatData[PixelIndex];
            break;
        }
        case TSF_RGBA16F:
        {
            const FFloat16* FloatData = reinterpret_cast<const FFloat16*>(RawData);
            HeightValue = FloatData[PixelIndex * 4].GetFloat(); // read first channel
            break;
        }
        case TSF_G16:
        {
            const uint16* UIntData = reinterpret_cast<const uint16*>(RawData);
            HeightValue = static_cast<float>(UIntData[PixelIndex]);
            break;
        }
        default:
            HeightValue = SeaLevelFallback;
            break;
    }
    Texture->Source.UnlockMip(0);
    return HeightValue;
}
} // namespace

FTileCoord UWorldGenTerrainResource::WorldPosToTile(const FVector& WorldPos) const
{
    if (TileSizeMeters <= KINDA_SMALL_NUMBER)
    {
        UE_LOG(LogWorldGenTerrainResource, Warning,
               TEXT("WorldPosToTile - Invalid TileSizeMeters (%.3f). Returning origin tile."),
               TileSizeMeters);
        return FTileCoord();
    }

    const FVector2D Local = FVector2D(WorldPos.X, WorldPos.Y) - WorldOrigin;
    const float InvTileSize = 1.0f / TileSizeMeters;
    return FTileCoord(FMath::FloorToInt(Local.X * InvTileSize),
                      FMath::FloorToInt(Local.Y * InvTileSize));
}

bool UWorldGenTerrainResource::HasTileData(const FTileCoord& Tile) const
{
    const FIntPoint TileKey = ToTileKey(Tile);
    return HeightTextures.Contains(TileKey) || BiomeCache.Contains(TileKey);
}

float UWorldGenTerrainResource::GetHeightAtWorldPosition(const FVector& WorldPos) const
{
    const FTileCoord TileCoord = WorldPosToTile(WorldPos);
    const FIntPoint TileKey = ToTileKey(TileCoord);

    const TSoftObjectPtr<UTexture2D>* TexturePtr = HeightTextures.Find(TileKey);
    if (!TexturePtr)
    {
        UE_LOG(LogWorldGenTerrainResource, Verbose,
               TEXT("GetHeightAtWorldPosition - No height texture for tile (%d,%d)"),
               TileCoord.X, TileCoord.Y);
        return SeaLevel;
    }

    UTexture2D* HeightTexture = TexturePtr->Get();
    if (!HeightTexture)
    {
        HeightTexture = TexturePtr->LoadSynchronous();
    }
    if (!HeightTexture)
    {
        UE_LOG(LogWorldGenTerrainResource, Warning,
               TEXT("GetHeightAtWorldPosition - Failed to load height texture for tile (%d,%d)"),
               TileCoord.X, TileCoord.Y);
        return SeaLevel;
    }

    const bool bHasSource =
        HeightTexture->Source.GetSizeX() > 0 && HeightTexture->Source.GetSizeY() > 0;
    int32 Width = bHasSource ? HeightTexture->Source.GetSizeX() : HeightTexture->GetSizeX();
    int32 Height = bHasSource ? HeightTexture->Source.GetSizeY() : HeightTexture->GetSizeY();
    if (Width <= 0 || Height <= 0)
    {
        UE_LOG(LogWorldGenTerrainResource, Warning,
               TEXT("GetHeightAtWorldPosition - Invalid texture dimensions for tile (%d,%d); "
                    "returning SeaLevel"),
               TileCoord.X, TileCoord.Y);
        return SeaLevel;
    }

    const FIntPoint PixelCoords = ComputePixelCoords(*this, WorldPos, TileCoord, Width, Height);

    const FTexturePlatformData* PlatformData = HeightTexture->GetPlatformData();
    const bool bSupportedPlatformFormat = PlatformData &&
                                          (PlatformData->PixelFormat == PF_R32_FLOAT ||
                                           PlatformData->PixelFormat == PF_R16F);

    // Prefer source data when available (transient editor textures/tests), otherwise use platform data.
    float HeightValue = SeaLevel;
    if (bHasSource)
    {
        HeightValue = SampleHeightFromSource(HeightTexture, PixelCoords, SeaLevel);
    }

    // Fall back to platform data when source is missing or produced a fallback height.
    if ((!bHasSource || FMath::IsNearlyEqual(HeightValue, SeaLevel)) && bSupportedPlatformFormat)
    {
        HeightValue = SampleHeightFromPlatformData(HeightTexture, PixelCoords, SeaLevel);
    }

    return HeightValue;
}

FBiomeResult UWorldGenTerrainResource::GetBiomeAtWorldPosition(const FVector& WorldPos) const
{
    const FTileCoord TileCoord = WorldPosToTile(WorldPos);
    const FIntPoint TileKey = ToTileKey(TileCoord);
    if (const FBiomeResult* CachedBiome = BiomeCache.Find(TileKey))
    {
        return *CachedBiome;
    }

    UE_LOG(LogWorldGenTerrainResource, Verbose,
           TEXT("GetBiomeAtWorldPosition - No biome cache for tile (%d,%d)"),
           TileCoord.X, TileCoord.Y);
    return FBiomeResult();
}
