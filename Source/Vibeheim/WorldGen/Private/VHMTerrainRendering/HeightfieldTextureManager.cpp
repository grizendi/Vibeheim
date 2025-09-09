#include "VHMTerrainRendering/HeightfieldTextureManager.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TextureResource.h"
#include "RenderUtils.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformTime.h"
#include "Engine/Texture.h"
#include "Engine/Engine.h"
#include "PixelFormat.h"
#include "Serialization/BulkData.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeightfieldTextureManager, Log, All);

UVHMHeightfieldTextureManager::UVHMHeightfieldTextureManager()
{
    // Initialize with default settings
    VHMSettings = FVHMSettings();
    HeightTextureFormat = VHMSettings.bUseHighPrecisionHeightTextures ? PF_R32_FLOAT : PF_R16F;
    
    // Don't create any graphics resources in constructor - wait for explicit Initialize() call
}

bool UVHMHeightfieldTextureManager::Initialize(const FVHMSettings& Settings)
{
    VHMSettings = Settings;
    
    // Set texture format based on settings
    HeightTextureFormat = VHMSettings.bUseHighPrecisionHeightTextures ? PF_R32_FLOAT : PF_R16F;
    
    const TCHAR* PrecisionText = (HeightTextureFormat == PF_R32_FLOAT) ? TEXT("high") : TEXT("standard");
    UE_LOG(LogHeightfieldTextureManager, Log, TEXT("HeightfieldTextureManager initialized with %s precision textures"), PrecisionText);
    
    return true;
}

UTexture2D* UVHMHeightfieldTextureManager::CreateHeightTexture(const FTileCoord& TileCoord, const TArray<float>& HeightData)
{
    // Safety check: Don't create textures if rendering system isn't ready
    if (!GEngine)
    {
        UE_LOG(LogHeightfieldTextureManager, Warning, TEXT("CreateHeightTexture: GEngine is null for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return nullptr;
    }
    
    if (!IsInGameThread())
    {
        UE_LOG(LogHeightfieldTextureManager, Warning, TEXT("CreateHeightTexture: Not in game thread for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return nullptr;
    }
    
    // Note: Don't check GEngine->GetWorld() as it might return null in PIE
    // Instead, get world from the object context
    
    if (HeightData.Num() == 0)
    {
        UE_LOG(LogHeightfieldTextureManager, Warning, TEXT("CreateHeightTexture: Empty height data for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return nullptr;
    }

    // Validate data size matches expected resolution
    const int32 ExpectedSize = VHMSettings.HeightTextureResolution * VHMSettings.HeightTextureResolution;
    if (HeightData.Num() != ExpectedSize)
    {
        UE_LOG(LogHeightfieldTextureManager, Warning, TEXT("CreateHeightTexture: Height data size mismatch for tile (%d, %d). Expected %d, got %d"), 
               TileCoord.X, TileCoord.Y, ExpectedSize, HeightData.Num());
        return nullptr;
    }

    // Convert height data to texture format
    TArray<uint8> TextureData;
    if (!ConvertHeightDataToTexture(HeightData, TextureData, HeightTextureFormat))
    {
        UE_LOG(LogHeightfieldTextureManager, Error, TEXT("CreateHeightTexture: Failed to convert height data for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return nullptr;
    }

    // Create the texture
    UTexture2D* HeightTexture = CreateTextureInternal(TileCoord, TextureData, HeightTextureFormat, false);
    if (!HeightTexture)
    {
        UE_LOG(LogHeightfieldTextureManager, Error, TEXT("CreateHeightTexture: Failed to create texture for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return nullptr;
    }

    // Store texture and cache height data for future modifications
    HeightTextures.Add(TileCoord, HeightTexture);
    CachedHeightData.Add(TileCoord, HeightData);
    bMemoryUsageDirty = true;

    UE_LOG(LogHeightfieldTextureManager, Verbose, TEXT("Created height texture for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
    return HeightTexture;
}

bool UVHMHeightfieldTextureManager::UpdateHeightTexture(const FTileCoord& TileCoord, const TArray<FHeightfieldModification>& Modifications)
{
    if (Modifications.Num() == 0)
    {
        return true; // Nothing to update
    }

    // Get existing texture and cached height data
    TObjectPtr<UTexture2D>* ExistingTexture = HeightTextures.Find(TileCoord);
    TArray<float>* CachedData = CachedHeightData.Find(TileCoord);

    if (!ExistingTexture || !ExistingTexture->Get() || !CachedData)
    {
        UE_LOG(LogHeightfieldTextureManager, Warning, TEXT("UpdateHeightTexture: No existing texture found for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    // Apply modifications to cached height data
    TArray<float> ModifiedHeightData = *CachedData;
    if (!ApplyModificationsToHeightData(TileCoord, Modifications, ModifiedHeightData))
    {
        UE_LOG(LogHeightfieldTextureManager, Error, TEXT("UpdateHeightTexture: Failed to apply modifications for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    // Convert modified data to texture format
    TArray<uint8> NewTextureData;
    if (!ConvertHeightDataToTexture(ModifiedHeightData, NewTextureData, HeightTextureFormat))
    {
        UE_LOG(LogHeightfieldTextureManager, Error, TEXT("UpdateHeightTexture: Failed to convert modified height data for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    // Update the texture
    if (!UpdateTextureData(ExistingTexture->Get(), NewTextureData, HeightTextureFormat))
    {
        UE_LOG(LogHeightfieldTextureManager, Error, TEXT("UpdateHeightTexture: Failed to update texture data for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    // Update cached data
    *CachedData = ModifiedHeightData;

    UE_LOG(LogHeightfieldTextureManager, Verbose, TEXT("Updated height texture for tile (%d, %d) with %d modifications"), 
           TileCoord.X, TileCoord.Y, Modifications.Num());
    return true;
}

UTexture2D* UVHMHeightfieldTextureManager::CreateNormalTexture(const FTileCoord& TileCoord, const TArray<float>& HeightData)
{
    if (HeightData.Num() == 0)
    {
        UE_LOG(LogHeightfieldTextureManager, Warning, TEXT("CreateNormalTexture: Empty height data for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return nullptr;
    }

    // Generate normal map data from height data
    TArray<uint8> NormalData;
    if (!GenerateNormalMapData(HeightData, NormalData, VHMSettings.HeightTextureResolution, DefaultHeightScale))
    {
        UE_LOG(LogHeightfieldTextureManager, Error, TEXT("CreateNormalTexture: Failed to generate normal data for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return nullptr;
    }

    // Create the normal texture
    UTexture2D* NormalTexture = CreateTextureInternal(TileCoord, NormalData, NormalTextureFormat, true);
    if (!NormalTexture)
    {
        UE_LOG(LogHeightfieldTextureManager, Error, TEXT("CreateNormalTexture: Failed to create normal texture for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return nullptr;
    }

    // Store normal texture
    NormalTextures.Add(TileCoord, NormalTexture);
    bMemoryUsageDirty = true;

    UE_LOG(LogHeightfieldTextureManager, Verbose, TEXT("Created normal texture for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
    return NormalTexture;
}

UTexture2D* UVHMHeightfieldTextureManager::GetHeightTexture(const FTileCoord& TileCoord)
{
    if (TObjectPtr<UTexture2D>* FoundTexture = HeightTextures.Find(TileCoord))
    {
        return FoundTexture->Get();
    }
    return nullptr;
}

void UVHMHeightfieldTextureManager::RemoveTexture(const FTileCoord& TileCoord)
{
    bool bRemovedAny = false;

    // Remove height texture
    if (HeightTextures.Remove(TileCoord) > 0)
    {
        bRemovedAny = true;
        UE_LOG(LogHeightfieldTextureManager, Verbose, TEXT("Removed height texture for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
    }

    // Remove normal texture
    if (NormalTextures.Remove(TileCoord) > 0)
    {
        bRemovedAny = true;
        UE_LOG(LogHeightfieldTextureManager, Verbose, TEXT("Removed normal texture for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
    }

    // Remove cached height data
    if (CachedHeightData.Remove(TileCoord) > 0)
    {
        bRemovedAny = true;
    }

    if (bRemovedAny)
    {
        bMemoryUsageDirty = true;
    }
}

void UVHMHeightfieldTextureManager::OptimizeTextureMemory()
{
    int32 RemovedCount = 0;
    TArray<FTileCoord> TilesToRemove;

    // Find textures that haven't been accessed recently (simple cleanup for now)
    // In a more sophisticated implementation, this could use LRU or distance-based cleanup
    const double CurrentTime = FPlatformTime::Seconds();
    const double CleanupThreshold = 30.0; // Remove textures older than 30 seconds

    // For now, just log the optimization attempt
    UE_LOG(LogHeightfieldTextureManager, Log, TEXT("OptimizeTextureMemory: Managing %d height textures and %d normal textures"), 
           HeightTextures.Num(), NormalTextures.Num());

    // Force garbage collection to clean up unreferenced textures
    if (GEngine)
    {
        GEngine->ForceGarbageCollection(true);
    }

    bMemoryUsageDirty = true;
    
    UE_LOG(LogHeightfieldTextureManager, Verbose, TEXT("Texture memory optimization completed"));
}

float UVHMHeightfieldTextureManager::GetTextureMemoryUsageMB() const
{
    if (!bMemoryUsageDirty)
    {
        return CachedMemoryUsageMB;
    }

    float TotalMemoryBytes = 0.0f;

    // Calculate height texture memory
    for (const auto& TexturePair : HeightTextures)
    {
        TotalMemoryBytes += CalculateTextureMemoryUsage(HeightTextureFormat, VHMSettings.HeightTextureResolution);
    }

    // Calculate normal texture memory
    for (const auto& TexturePair : NormalTextures)
    {
        TotalMemoryBytes += CalculateTextureMemoryUsage(NormalTextureFormat, VHMSettings.HeightTextureResolution);
    }

    // Add cached height data memory
    for (const auto& CachedDataPair : CachedHeightData)
    {
        TotalMemoryBytes += CachedDataPair.Value.Num() * sizeof(float);
    }

    CachedMemoryUsageMB = TotalMemoryBytes / (1024.0f * 1024.0f);
    bMemoryUsageDirty = false;

    return CachedMemoryUsageMB;
}

UTexture2D* UVHMHeightfieldTextureManager::CreateTextureInternal(const FTileCoord& TileCoord, const TArray<uint8>& TextureData, EPixelFormat Format, bool bIsNormalMap)
{
    if (TextureData.Num() == 0)
    {
        UE_LOG(LogHeightfieldTextureManager, Error, TEXT("CreateTextureInternal: Empty texture data"));
        return nullptr;
    }

    // Create texture object
    UTexture2D* NewTexture = UTexture2D::CreateTransient(VHMSettings.HeightTextureResolution, VHMSettings.HeightTextureResolution, Format);
    if (!NewTexture)
    {
        UE_LOG(LogHeightfieldTextureManager, Error, TEXT("CreateTextureInternal: Failed to create transient texture"));
        return nullptr;
    }

    // Set texture properties
    NewTexture->SRGB = false; // Height and normal data should not use sRGB
    NewTexture->CompressionSettings = bIsNormalMap ? TC_Normalmap : TC_HDR;
    NewTexture->MipGenSettings = TMGS_NoMipmaps; // VHM typically doesn't need mipmaps for height data
    NewTexture->Filter = TF_Bilinear;
    NewTexture->AddressX = TA_Clamp;
    NewTexture->AddressY = TA_Clamp;

    // Lock texture for writing
    FTexture2DMipMap& Mip = NewTexture->GetPlatformData()->Mips[0];
    void* TextureDataPtr = Mip.BulkData.Lock(LOCK_READ_WRITE);
    
    if (!TextureDataPtr)
    {
        UE_LOG(LogHeightfieldTextureManager, Error, TEXT("CreateTextureInternal: Failed to lock texture data"));
        return nullptr;
    }

    // Copy data to texture
    FMemory::Memcpy(TextureDataPtr, TextureData.GetData(), TextureData.Num());
    Mip.BulkData.Unlock();

    // Update texture resource
    NewTexture->UpdateResource();

    return NewTexture;
}

bool UVHMHeightfieldTextureManager::ConvertHeightDataToTexture(const TArray<float>& HeightData, TArray<uint8>& OutTextureData, EPixelFormat Format)
{
    const int32 PixelCount = HeightData.Num();
    
    if (Format == PF_R16F)
    {
        // Convert to 16-bit float format
        OutTextureData.SetNum(PixelCount * sizeof(FFloat16));
        FFloat16* PixelData = reinterpret_cast<FFloat16*>(OutTextureData.GetData());
        
        for (int32 i = 0; i < PixelCount; ++i)
        {
            PixelData[i] = FFloat16(HeightData[i]);
        }
    }
    else if (Format == PF_R32_FLOAT)
    {
        // Convert to 32-bit float format
        OutTextureData.SetNum(PixelCount * sizeof(float));
        float* PixelData = reinterpret_cast<float*>(OutTextureData.GetData());
        
        for (int32 i = 0; i < PixelCount; ++i)
        {
            PixelData[i] = HeightData[i];
        }
    }
    else
    {
        UE_LOG(LogHeightfieldTextureManager, Error, TEXT("ConvertHeightDataToTexture: Unsupported pixel format"));
        return false;
    }

    return true;
}

bool UVHMHeightfieldTextureManager::GenerateNormalMapData(const TArray<float>& HeightData, TArray<uint8>& OutNormalData, int32 TextureResolution, float HeightScale)
{
    const int32 PixelCount = TextureResolution * TextureResolution;
    if (HeightData.Num() != PixelCount)
    {
        UE_LOG(LogHeightfieldTextureManager, Error, TEXT("GenerateNormalMapData: Height data size mismatch"));
        return false;
    }

    // Allocate normal map data (RGBA8)
    OutNormalData.SetNum(PixelCount * 4);
    uint8* NormalPixels = OutNormalData.GetData();

    // Generate normals using finite differences
    for (int32 Y = 0; Y < TextureResolution; ++Y)
    {
        for (int32 X = 0; X < TextureResolution; ++X)
        {
            const int32 Index = Y * TextureResolution + X;
            
            // Sample neighboring heights (with clamping at edges)
            const int32 LeftX = FMath::Max(0, X - 1);
            const int32 RightX = FMath::Min(TextureResolution - 1, X + 1);
            const int32 TopY = FMath::Max(0, Y - 1);
            const int32 BottomY = FMath::Min(TextureResolution - 1, Y + 1);
            
            const float LeftHeight = HeightData[Y * TextureResolution + LeftX];
            const float RightHeight = HeightData[Y * TextureResolution + RightX];
            const float TopHeight = HeightData[TopY * TextureResolution + X];
            const float BottomHeight = HeightData[BottomY * TextureResolution + X];
            
            // Calculate gradients
            const float DX = (RightHeight - LeftHeight) * HeightScale;
            const float DY = (BottomHeight - TopHeight) * HeightScale;
            
            // Calculate normal vector
            FVector Normal = FVector(-DX, -DY, 1.0f).GetSafeNormal();
            
            // Convert to texture format (0-255 range)
            const int32 PixelIndex = Index * 4;
            NormalPixels[PixelIndex + 0] = FMath::Clamp(FMath::RoundToInt((Normal.X + 1.0f) * 127.5f), 0, 255); // R
            NormalPixels[PixelIndex + 1] = FMath::Clamp(FMath::RoundToInt((Normal.Y + 1.0f) * 127.5f), 0, 255); // G
            NormalPixels[PixelIndex + 2] = FMath::Clamp(FMath::RoundToInt((Normal.Z + 1.0f) * 127.5f), 0, 255); // B
            NormalPixels[PixelIndex + 3] = 255; // A (full alpha)
        }
    }

    return true;
}

bool UVHMHeightfieldTextureManager::ApplyModificationsToHeightData(const FTileCoord& TileCoord, const TArray<FHeightfieldModification>& Modifications, TArray<float>& InOutHeightData)
{
    const int32 TextureResolution = VHMSettings.HeightTextureResolution;
    const float TileSize = 64.0f; // Standard tile size in meters
    const FVector TileWorldPos = TileCoord.ToWorldPosition(TileSize);
    
    for (const FHeightfieldModification& Modification : Modifications)
    {
        // Check if modification affects this tile
        if (Modification.AffectedTile != TileCoord)
        {
            continue;
        }

        // Convert modification center to tile-local coordinates
        const FVector2D LocalCenter = FVector2D(
            Modification.Center.X - TileWorldPos.X + (TileSize * 0.5f),
            Modification.Center.Y - TileWorldPos.Y + (TileSize * 0.5f)
        );

        // Convert to texture coordinates
        const FVector2D TextureCenter = LocalCenter * (TextureResolution / TileSize);
        const float TextureRadius = Modification.Radius * (TextureResolution / TileSize);

        // Apply modification to affected pixels
        for (int32 Y = 0; Y < TextureResolution; ++Y)
        {
            for (int32 X = 0; X < TextureResolution; ++X)
            {
                const FVector2D PixelPos(X, Y);
                const float Distance = FVector2D::Distance(PixelPos, TextureCenter);
                
                if (Distance <= TextureRadius)
                {
                    const int32 Index = Y * TextureResolution + X;
                    const float Falloff = 1.0f - (Distance / TextureRadius);
                    const float EffectiveStrength = Modification.Strength * Falloff;
                    
                    switch (Modification.Operation)
                    {
                        case EHeightfieldOperation::Add:
                            InOutHeightData[Index] += EffectiveStrength;
                            break;
                            
                        case EHeightfieldOperation::Subtract:
                            InOutHeightData[Index] -= EffectiveStrength;
                            break;
                            
                        case EHeightfieldOperation::Flatten:
                            {
                                const float TargetHeight = Modification.bFlattenUsesTarget ? Modification.FlattenTargetZ : InOutHeightData[Index];
                                InOutHeightData[Index] = FMath::Lerp(InOutHeightData[Index], TargetHeight, EffectiveStrength);
                            }
                            break;
                            
                        case EHeightfieldOperation::Smooth:
                            {
                                // Simple smoothing - average with neighbors
                                float AverageHeight = InOutHeightData[Index];
                                int32 SampleCount = 1;
                                
                                for (int32 DY = -1; DY <= 1; ++DY)
                                {
                                    for (int32 DX = -1; DX <= 1; ++DX)
                                    {
                                        const int32 SampleX = X + DX;
                                        const int32 SampleY = Y + DY;
                                        
                                        if (SampleX >= 0 && SampleX < TextureResolution && 
                                            SampleY >= 0 && SampleY < TextureResolution)
                                        {
                                            const int32 SampleIndex = SampleY * TextureResolution + SampleX;
                                            AverageHeight += InOutHeightData[SampleIndex];
                                            SampleCount++;
                                        }
                                    }
                                }
                                
                                AverageHeight /= SampleCount;
                                InOutHeightData[Index] = FMath::Lerp(InOutHeightData[Index], AverageHeight, EffectiveStrength);
                            }
                            break;
                    }
                }
            }
        }
    }

    return true;
}

bool UVHMHeightfieldTextureManager::UpdateTextureData(UTexture2D* Texture, const TArray<uint8>& NewTextureData, EPixelFormat Format)
{
    if (!Texture || NewTextureData.Num() == 0)
    {
        return false;
    }

    // Lock texture for writing
    FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
    void* TextureDataPtr = Mip.BulkData.Lock(LOCK_READ_WRITE);
    
    if (!TextureDataPtr)
    {
        UE_LOG(LogHeightfieldTextureManager, Error, TEXT("UpdateTextureData: Failed to lock texture data"));
        return false;
    }

    // Copy new data to texture
    FMemory::Memcpy(TextureDataPtr, NewTextureData.GetData(), NewTextureData.Num());
    Mip.BulkData.Unlock();

    // Update texture resource
    Texture->UpdateResource();

    return true;
}

int32 UVHMHeightfieldTextureManager::CalculateTextureMemoryUsage(EPixelFormat Format, int32 Resolution) const
{
    const int32 PixelCount = Resolution * Resolution;
    
    switch (Format)
    {
        case PF_R16F:
            return PixelCount * sizeof(FFloat16);
        case PF_R32_FLOAT:
            return PixelCount * sizeof(float);
        case PF_B8G8R8A8:
            return PixelCount * 4; // 4 bytes per pixel (RGBA)
        default:
            return PixelCount * 4; // Default assumption
    }
}
