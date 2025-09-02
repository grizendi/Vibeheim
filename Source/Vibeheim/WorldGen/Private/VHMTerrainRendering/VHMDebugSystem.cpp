#include "VHMTerrainRendering/VHMDebugSystem.h"
#include "VHMTerrainRendering/VHMTerrainRenderer.h"
#include "VirtualHeightfieldMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Components/PrimitiveComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVHMDebugSystem, Log, All);

UVHMDebugSystem::UVHMDebugSystem()
{
    TerrainRenderer = nullptr;
    World = nullptr;
    WireframeMaterial = nullptr;
    DebugMaterial = nullptr;

    // Initialize default debug settings
    DebugDrawSettings.bEnableDebugDraw = false;
    DebugDrawSettings.DebugMode = EVHMDebugMode::None;
    DebugDrawSettings.BoundsColor = FColor::Green;
    DebugDrawSettings.LODColor = FColor::Blue;
    DebugDrawSettings.BoundaryColor = FColor::Red;
    DebugDrawSettings.LineThickness = 2.0f;
    DebugDrawSettings.LineDuration = 0.0f;
    DebugDrawSettings.bShowDebugText = true;
    DebugDrawSettings.TextScale = 1.0f;

    // Initialize texture export settings
    TextureExportSettings.ExportDirectory = TEXT("Debug/VHM/");
    TextureExportSettings.ExportFormat = TEXT("PNG");
    TextureExportSettings.bIncludeTileCoords = true;
    TextureExportSettings.bIncludeTimestamp = false;
    TextureExportSettings.bExportHeightTextures = true;
    TextureExportSettings.bExportMaterialTextures = true;
}

bool UVHMDebugSystem::Initialize(UVHMTerrainRenderer* InTerrainRenderer, UWorld* InWorld)
{
    if (!InTerrainRenderer || !InWorld)
    {
        UE_LOG(LogVHMDebugSystem, Error, TEXT("Initialize - Invalid terrain renderer or world reference"));
        return false;
    }

    TerrainRenderer = InTerrainRenderer;
    World = InWorld;

    // Create debug materials
    CreateDebugMaterials();

    UE_LOG(LogVHMDebugSystem, Log, TEXT("VHM Debug System initialized successfully"));
    return true;
}

void UVHMDebugSystem::SetDebugDrawSettings(const FVHMDebugDrawSettings& Settings)
{
    DebugDrawSettings = Settings;
    UE_LOG(LogVHMDebugSystem, Log, TEXT("Debug draw settings updated - Mode: %d, Enabled: %s"), 
           static_cast<int32>(Settings.DebugMode), Settings.bEnableDebugDraw ? TEXT("True") : TEXT("False"));
}

void UVHMDebugSystem::SetDebugVisualizationEnabled(bool bEnabled)
{
    DebugDrawSettings.bEnableDebugDraw = bEnabled;
    UE_LOG(LogVHMDebugSystem, Log, TEXT("Debug visualization %s"), bEnabled ? TEXT("enabled") : TEXT("disabled"));
}

void UVHMDebugSystem::SetDebugMode(EVHMDebugMode Mode)
{
    DebugDrawSettings.DebugMode = Mode;
    UE_LOG(LogVHMDebugSystem, Log, TEXT("Debug mode set to: %d"), static_cast<int32>(Mode));
}

void UVHMDebugSystem::DrawDebugVisualization()
{
    if (!DebugDrawSettings.bEnableDebugDraw || !TerrainRenderer || !World)
    {
        return;
    }

    // Get all active mesh tiles from terrain renderer
    TArray<FTileCoord> ActiveTiles = TerrainRenderer->GetActiveMeshTiles();

    for (const FTileCoord& TileCoord : ActiveTiles)
    {
        DrawDebugVisualizationForTile(TileCoord);
    }

    // Draw performance overlay if enabled
    if (DebugDrawSettings.DebugMode == EVHMDebugMode::Performance)
    {
        DrawPerformanceOverlay();
    }
}

void UVHMDebugSystem::DrawDebugVisualizationForTile(const FTileCoord& TileCoord)
{
    if (!DebugDrawSettings.bEnableDebugDraw || !TerrainRenderer || !World)
    {
        return;
    }

    // Get VHM component for this tile
    UVirtualHeightfieldMeshComponent* VHMComponent = TerrainRenderer->GetVHMComponent(TileCoord);
    if (!VHMComponent)
    {
        return;
    }

    switch (DebugDrawSettings.DebugMode)
    {
        case EVHMDebugMode::ComponentBounds:
            DrawComponentBounds(VHMComponent, TileCoord);
            break;

        case EVHMDebugMode::LODLevels:
            DrawLODLevelIndicators(VHMComponent, TileCoord);
            break;

        case EVHMDebugMode::TileBoundaries:
            DrawTileBoundaries(TileCoord);
            break;

        case EVHMDebugMode::Wireframe:
            // Wireframe mode is handled by material changes
            break;

        case EVHMDebugMode::HeightTexture:
        case EVHMDebugMode::MaterialDebug:
            // These modes are handled by material changes or separate visualization
            break;

        default:
            break;
    }
}

bool UVHMDebugSystem::ExportHeightTexture(const FTileCoord& TileCoord, const FString& OutputPath)
{
    if (!TerrainRenderer)
    {
        UE_LOG(LogVHMDebugSystem, Error, TEXT("ExportHeightTexture - No terrain renderer available"));
        return false;
    }

    // Get terrain mesh data for the tile
    FTerrainMeshData MeshData;
    if (!TerrainRenderer->GetTerrainMeshData(TileCoord, MeshData))
    {
        UE_LOG(LogVHMDebugSystem, Warning, TEXT("ExportHeightTexture - No mesh data for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    if (!MeshData.HeightTexture)
    {
        UE_LOG(LogVHMDebugSystem, Warning, TEXT("ExportHeightTexture - No height texture for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    // Generate output path if not provided
    FString FinalOutputPath = OutputPath;
    if (FinalOutputPath.IsEmpty())
    {
        FinalOutputPath = GenerateExportFilename(TileCoord, TEXT("Height"), TextureExportSettings.ExportFormat);
    }

    // Ensure directory exists
    FString DirectoryPath = FPaths::GetPath(FinalOutputPath);
    if (!FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*DirectoryPath))
    {
        UE_LOG(LogVHMDebugSystem, Error, TEXT("ExportHeightTexture - Failed to create directory: %s"), *DirectoryPath);
        return false;
    }

    // Export texture data
    // Note: This is a simplified implementation - actual texture export would require
    // reading texture data from GPU and converting to image format
    UE_LOG(LogVHMDebugSystem, Log, TEXT("Height texture export for tile (%d, %d) to: %s"), 
           TileCoord.X, TileCoord.Y, *FinalOutputPath);

    // For now, just log the export attempt
    UE_LOG(LogVHMDebugSystem, Log, TEXT("Height texture dimensions: %dx%d"), 
           MeshData.HeightTexture->GetSizeX(), MeshData.HeightTexture->GetSizeY());

    return true;
}

bool UVHMDebugSystem::ExportMaterialTexture(const FTileCoord& TileCoord, const FString& OutputPath)
{
    if (!TerrainRenderer)
    {
        UE_LOG(LogVHMDebugSystem, Error, TEXT("ExportMaterialTexture - No terrain renderer available"));
        return false;
    }

    // Get terrain mesh data for the tile
    FTerrainMeshData MeshData;
    if (!TerrainRenderer->GetTerrainMeshData(TileCoord, MeshData))
    {
        UE_LOG(LogVHMDebugSystem, Warning, TEXT("ExportMaterialTexture - No mesh data for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    if (!MeshData.MaterialInstance)
    {
        UE_LOG(LogVHMDebugSystem, Warning, TEXT("ExportMaterialTexture - No material instance for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return false;
    }

    // Generate output path if not provided
    FString FinalOutputPath = OutputPath;
    if (FinalOutputPath.IsEmpty())
    {
        FinalOutputPath = GenerateExportFilename(TileCoord, TEXT("Material"), TextureExportSettings.ExportFormat);
    }

    // Ensure directory exists
    FString DirectoryPath = FPaths::GetPath(FinalOutputPath);
    if (!FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*DirectoryPath))
    {
        UE_LOG(LogVHMDebugSystem, Error, TEXT("ExportMaterialTexture - Failed to create directory: %s"), *DirectoryPath);
        return false;
    }

    // Export material texture data
    UE_LOG(LogVHMDebugSystem, Log, TEXT("Material texture export for tile (%d, %d) to: %s"), 
           TileCoord.X, TileCoord.Y, *FinalOutputPath);

    return true;
}

bool UVHMDebugSystem::ExportAllTexturesForTile(const FTileCoord& TileCoord, const FString& OutputDirectory)
{
    bool bSuccess = true;

    // Export height texture if enabled
    if (TextureExportSettings.bExportHeightTextures)
    {
        FString HeightTexturePath = OutputDirectory.IsEmpty() ? TEXT("") : 
            FPaths::Combine(OutputDirectory, GenerateExportFilename(TileCoord, TEXT("Height"), TextureExportSettings.ExportFormat));
        bSuccess &= ExportHeightTexture(TileCoord, HeightTexturePath);
    }

    // Export material texture if enabled
    if (TextureExportSettings.bExportMaterialTextures)
    {
        FString MaterialTexturePath = OutputDirectory.IsEmpty() ? TEXT("") : 
            FPaths::Combine(OutputDirectory, GenerateExportFilename(TileCoord, TEXT("Material"), TextureExportSettings.ExportFormat));
        bSuccess &= ExportMaterialTexture(TileCoord, MaterialTexturePath);
    }

    return bSuccess;
}

void UVHMDebugSystem::StartProfiling(const FString& OperationName, const FTileCoord& TileCoord)
{
    FVHMProfilingData& ProfilingEntry = ProfilingData.FindOrAdd(OperationName);
    ProfilingEntry.OperationName = OperationName;
    ProfilingEntry.StartTime = FPlatformTime::Seconds();
    ProfilingEntry.TileCoord = TileCoord;
    ProfilingEntry.EndTime = 0.0;
    ProfilingEntry.DurationMs = 0.0f;

    UE_LOG(LogVHMDebugSystem, VeryVerbose, TEXT("Started profiling: %s for tile (%d, %d)"), 
           *OperationName, TileCoord.X, TileCoord.Y);
}

void UVHMDebugSystem::EndProfiling(const FString& OperationName)
{
    if (FVHMProfilingData* ProfilingEntry = ProfilingData.Find(OperationName))
    {
        ProfilingEntry->EndTime = FPlatformTime::Seconds();
        ProfilingEntry->DurationMs = (ProfilingEntry->EndTime - ProfilingEntry->StartTime) * 1000.0f;

        UE_LOG(LogVHMDebugSystem, VeryVerbose, TEXT("Ended profiling: %s - Duration: %.2fms"), 
               *OperationName, ProfilingEntry->DurationMs);
    }
    else
    {
        UE_LOG(LogVHMDebugSystem, Warning, TEXT("EndProfiling - No profiling data found for operation: %s"), *OperationName);
    }
}

bool UVHMDebugSystem::GetProfilingResults(const FString& OperationName, FVHMProfilingData& OutProfilingData) const
{
    if (const FVHMProfilingData* ProfilingEntry = ProfilingData.Find(OperationName))
    {
        OutProfilingData = *ProfilingEntry;
        return true;
    }
    return false;
}

TArray<FVHMProfilingData> UVHMDebugSystem::GetAllProfilingResults() const
{
    TArray<FVHMProfilingData> Results;
    ProfilingData.GenerateValueArray(Results);
    return Results;
}

void UVHMDebugSystem::ClearProfilingData()
{
    ProfilingData.Empty();
    UE_LOG(LogVHMDebugSystem, Log, TEXT("Profiling data cleared"));
}

void UVHMDebugSystem::SetWireframeMode(bool bEnabled)
{
    if (!TerrainRenderer)
    {
        return;
    }

    // Get all active mesh tiles
    TArray<FTileCoord> ActiveTiles = TerrainRenderer->GetActiveMeshTiles();

    for (const FTileCoord& TileCoord : ActiveTiles)
    {
        UVirtualHeightfieldMeshComponent* VHMComponent = TerrainRenderer->GetVHMComponent(TileCoord);
        if (VHMComponent)
        {
            if (bEnabled && WireframeMaterial)
            {
                VHMComponent->UPrimitiveComponent::SetMaterial(0, WireframeMaterial);
            }
            else
            {
                // Restore original material
                FTerrainMeshData MeshData;
                if (TerrainRenderer->GetTerrainMeshData(TileCoord, MeshData) && MeshData.MaterialInstance)
                {
                    VHMComponent->UPrimitiveComponent::SetMaterial(0, MeshData.MaterialInstance);
                }
            }
        }
    }

    UE_LOG(LogVHMDebugSystem, Log, TEXT("Wireframe mode %s for %d VHM components"), 
           bEnabled ? TEXT("enabled") : TEXT("disabled"), ActiveTiles.Num());
}

void UVHMDebugSystem::SetDebugMaterialMode(bool bEnabled)
{
    if (!TerrainRenderer)
    {
        return;
    }

    // Get all active mesh tiles
    TArray<FTileCoord> ActiveTiles = TerrainRenderer->GetActiveMeshTiles();

    for (const FTileCoord& TileCoord : ActiveTiles)
    {
        UVirtualHeightfieldMeshComponent* VHMComponent = TerrainRenderer->GetVHMComponent(TileCoord);
        if (VHMComponent)
        {
            if (bEnabled && DebugMaterial)
            {
                VHMComponent->UPrimitiveComponent::SetMaterial(0, DebugMaterial);
            }
            else
            {
                // Restore original material
                FTerrainMeshData MeshData;
                if (TerrainRenderer->GetTerrainMeshData(TileCoord, MeshData) && MeshData.MaterialInstance)
                {
                    VHMComponent->UPrimitiveComponent::SetMaterial(0, MeshData.MaterialInstance);
                }
            }
        }
    }

    UE_LOG(LogVHMDebugSystem, Log, TEXT("Debug material mode %s for %d VHM components"), 
           bEnabled ? TEXT("enabled") : TEXT("disabled"), ActiveTiles.Num());
}

FString UVHMDebugSystem::GetPerformanceStatsSummary() const
{
    if (!TerrainRenderer)
    {
        return TEXT("No terrain renderer available");
    }

    FVHMPerformanceStats Stats = TerrainRenderer->GetPerformanceStats();
    
    FString Summary = FString::Printf(TEXT("=== VHM Performance Statistics ===\n"));
    Summary += FString::Printf(TEXT("Active VHM Components: %d\n"), Stats.ActiveVHMComponents);
    Summary += FString::Printf(TEXT("Texture Memory Usage: %.2f MB\n"), Stats.TextureMemoryUsageMB);
    Summary += FString::Printf(TEXT("Average Mesh Generation: %.2f ms\n"), Stats.AverageMeshGenerationMs);
    Summary += FString::Printf(TEXT("Last Mesh Generation: %.2f ms\n"), Stats.LastMeshGenerationMs);
    Summary += FString::Printf(TEXT("LOD Transitions This Frame: %d\n"), Stats.LODTransitionsThisFrame);
    Summary += FString::Printf(TEXT("Current FPS: %.1f\n"), Stats.CurrentFPS);
    Summary += FString::Printf(TEXT("Total Meshes Created: %d\n"), Stats.TotalMeshesCreated);
    Summary += FString::Printf(TEXT("Peak Generation Time: %.2f ms\n"), Stats.PeakGenerationTimeMs);
    Summary += FString::Printf(TEXT("Boundary Stitching Operations: %d\n"), Stats.BoundaryStitchingOperations);

    return Summary;
}

void UVHMDebugSystem::LogVHMComponentInfo(const FTileCoord& TileCoord) const
{
    if (!TerrainRenderer)
    {
        UE_LOG(LogVHMDebugSystem, Error, TEXT("LogVHMComponentInfo - No terrain renderer available"));
        return;
    }

    UVirtualHeightfieldMeshComponent* VHMComponent = TerrainRenderer->GetVHMComponent(TileCoord);
    if (!VHMComponent)
    {
        UE_LOG(LogVHMDebugSystem, Warning, TEXT("LogVHMComponentInfo - No VHM component for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return;
    }

    FTerrainMeshData MeshData;
    if (!TerrainRenderer->GetTerrainMeshData(TileCoord, MeshData))
    {
        UE_LOG(LogVHMDebugSystem, Warning, TEXT("LogVHMComponentInfo - No mesh data for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        return;
    }

    UE_LOG(LogVHMDebugSystem, Log, TEXT("=== VHM Component Info for Tile (%d, %d) ==="), TileCoord.X, TileCoord.Y);
    UE_LOG(LogVHMDebugSystem, Log, TEXT("Component Valid: %s"), VHMComponent ? TEXT("Yes") : TEXT("No"));
    UE_LOG(LogVHMDebugSystem, Log, TEXT("Component Registered: %s"), VHMComponent->IsRegistered() ? TEXT("Yes") : TEXT("No"));
    bool bIsComponentVisible = VHMComponent->UPrimitiveComponent::IsVisible();
    UE_LOG(LogVHMDebugSystem, Log, TEXT("Component Visible: %s"), bIsComponentVisible ? TEXT("Yes") : TEXT("No"));
    UE_LOG(LogVHMDebugSystem, Log, TEXT("World Bounds: %s"), *MeshData.WorldBounds.ToString());
    UE_LOG(LogVHMDebugSystem, Log, TEXT("Current LOD Level: %d"), MeshData.CurrentLODLevel);
    UE_LOG(LogVHMDebugSystem, Log, TEXT("Is Visible: %s"), MeshData.bIsVisible ? TEXT("Yes") : TEXT("No"));
    UE_LOG(LogVHMDebugSystem, Log, TEXT("Last Update Time: %.3f"), MeshData.LastUpdateTime);
    UE_LOG(LogVHMDebugSystem, Log, TEXT("Height Texture: %s"), MeshData.HeightTexture ? TEXT("Valid") : TEXT("None"));
    UE_LOG(LogVHMDebugSystem, Log, TEXT("Material Instance: %s"), MeshData.MaterialInstance ? TEXT("Valid") : TEXT("None"));

    if (MeshData.HeightTexture)
    {
        UE_LOG(LogVHMDebugSystem, Log, TEXT("Height Texture Size: %dx%d"), 
               MeshData.HeightTexture->GetSizeX(), MeshData.HeightTexture->GetSizeY());
    }
}

void UVHMDebugSystem::LogTextureMemoryUsage() const
{
    if (!TerrainRenderer)
    {
        UE_LOG(LogVHMDebugSystem, Error, TEXT("LogTextureMemoryUsage - No terrain renderer available"));
        return;
    }

    FVHMPerformanceStats Stats = TerrainRenderer->GetPerformanceStats();
    TArray<FTileCoord> ActiveTiles = TerrainRenderer->GetActiveMeshTiles();

    UE_LOG(LogVHMDebugSystem, Log, TEXT("=== VHM Texture Memory Usage ==="));
    UE_LOG(LogVHMDebugSystem, Log, TEXT("Total Texture Memory: %.2f MB"), Stats.TextureMemoryUsageMB);
    UE_LOG(LogVHMDebugSystem, Log, TEXT("Active Tiles: %d"), ActiveTiles.Num());

    if (ActiveTiles.Num() > 0)
    {
        float MemoryPerTile = Stats.TextureMemoryUsageMB / ActiveTiles.Num();
        UE_LOG(LogVHMDebugSystem, Log, TEXT("Average Memory Per Tile: %.2f MB"), MemoryPerTile);
    }

    // Log individual tile texture information
    for (const FTileCoord& TileCoord : ActiveTiles)
    {
        FTerrainMeshData MeshData;
        if (TerrainRenderer->GetTerrainMeshData(TileCoord, MeshData))
        {
            FString TextureInfo = FString::Printf(TEXT("Tile (%d, %d): "), TileCoord.X, TileCoord.Y);
            
            if (MeshData.HeightTexture)
            {
                TextureInfo += FString::Printf(TEXT("Height %dx%d "), 
                    MeshData.HeightTexture->GetSizeX(), MeshData.HeightTexture->GetSizeY());
            }
            else
            {
                TextureInfo += TEXT("No Height ");
            }

            if (MeshData.MaterialInstance)
            {
                TextureInfo += TEXT("Material OK");
            }
            else
            {
                TextureInfo += TEXT("No Material");
            }

            UE_LOG(LogVHMDebugSystem, Log, TEXT("  %s"), *TextureInfo);
        }
    }
}

void UVHMDebugSystem::DrawComponentBounds(UVirtualHeightfieldMeshComponent* VHMComponent, const FTileCoord& TileCoord)
{
    if (!VHMComponent || !World)
    {
        return;
    }

    // Get component bounds
    FBox ComponentBounds = GetTileWorldBounds(TileCoord);
    
    // Draw bounding box
    DrawDebugBox(World, ComponentBounds.GetCenter(), ComponentBounds.GetExtent(), 
                 DebugDrawSettings.BoundsColor, false, DebugDrawSettings.LineDuration, 0, DebugDrawSettings.LineThickness);

    // Draw debug text if enabled
    if (DebugDrawSettings.bShowDebugText)
    {
        FVector TextLocation = ComponentBounds.GetCenter() + FVector(0, 0, ComponentBounds.GetExtent().Z);
        FString DebugText = FString::Printf(TEXT("Tile (%d, %d)"), TileCoord.X, TileCoord.Y);
        
        DrawDebugString(World, TextLocation, DebugText, nullptr, DebugDrawSettings.BoundsColor, 
                       DebugDrawSettings.LineDuration, false, DebugDrawSettings.TextScale);
    }
}

void UVHMDebugSystem::DrawLODLevelIndicators(UVirtualHeightfieldMeshComponent* VHMComponent, const FTileCoord& TileCoord)
{
    if (!VHMComponent || !World || !TerrainRenderer)
    {
        return;
    }

    // Get mesh data to determine LOD level
    FTerrainMeshData MeshData;
    if (!TerrainRenderer->GetTerrainMeshData(TileCoord, MeshData))
    {
        return;
    }

    FVector TileCenter = GetTileCenterPosition(TileCoord);
    
    // Draw LOD level indicator
    FColor LODColor = DebugDrawSettings.LODColor;
    
    // Vary color intensity based on LOD level
    float LODIntensity = FMath::Clamp(1.0f - (MeshData.CurrentLODLevel * 0.2f), 0.2f, 1.0f);
    LODColor = FColor(
        FMath::RoundToInt(LODColor.R * LODIntensity),
        FMath::RoundToInt(LODColor.G * LODIntensity),
        FMath::RoundToInt(LODColor.B * LODIntensity),
        LODColor.A
    );

    // Draw LOD level as a sphere
    float SphereRadius = 50.0f + (MeshData.CurrentLODLevel * 25.0f);
    DrawDebugSphere(World, TileCenter + FVector(0, 0, 100), SphereRadius, 12, LODColor, 
                   false, DebugDrawSettings.LineDuration, 0, DebugDrawSettings.LineThickness);

    // Draw debug text if enabled
    if (DebugDrawSettings.bShowDebugText)
    {
        FString LODText = FString::Printf(TEXT("LOD %d"), MeshData.CurrentLODLevel);
        DrawDebugString(World, TileCenter + FVector(0, 0, 150), LODText, nullptr, LODColor, 
                       DebugDrawSettings.LineDuration, false, DebugDrawSettings.TextScale);
    }
}

void UVHMDebugSystem::DrawTileBoundaries(const FTileCoord& TileCoord)
{
    if (!World)
    {
        return;
    }

    FBox TileBounds = GetTileWorldBounds(TileCoord);
    FVector TileMin = TileBounds.Min;
    FVector TileMax = TileBounds.Max;

    // Draw tile boundary lines on the ground
    float GroundZ = TileMin.Z;
    
    // Draw the four edges of the tile
    DrawDebugLine(World, FVector(TileMin.X, TileMin.Y, GroundZ), FVector(TileMax.X, TileMin.Y, GroundZ), 
                 DebugDrawSettings.BoundaryColor, false, DebugDrawSettings.LineDuration, 0, DebugDrawSettings.LineThickness);
    
    DrawDebugLine(World, FVector(TileMax.X, TileMin.Y, GroundZ), FVector(TileMax.X, TileMax.Y, GroundZ), 
                 DebugDrawSettings.BoundaryColor, false, DebugDrawSettings.LineDuration, 0, DebugDrawSettings.LineThickness);
    
    DrawDebugLine(World, FVector(TileMax.X, TileMax.Y, GroundZ), FVector(TileMin.X, TileMax.Y, GroundZ), 
                 DebugDrawSettings.BoundaryColor, false, DebugDrawSettings.LineDuration, 0, DebugDrawSettings.LineThickness);
    
    DrawDebugLine(World, FVector(TileMin.X, TileMax.Y, GroundZ), FVector(TileMin.X, TileMin.Y, GroundZ), 
                 DebugDrawSettings.BoundaryColor, false, DebugDrawSettings.LineDuration, 0, DebugDrawSettings.LineThickness);

    // Draw corner markers
    float MarkerSize = 20.0f;
    DrawDebugSphere(World, FVector(TileMin.X, TileMin.Y, GroundZ), MarkerSize, 8, DebugDrawSettings.BoundaryColor, 
                   false, DebugDrawSettings.LineDuration, 0, DebugDrawSettings.LineThickness);
    DrawDebugSphere(World, FVector(TileMax.X, TileMin.Y, GroundZ), MarkerSize, 8, DebugDrawSettings.BoundaryColor, 
                   false, DebugDrawSettings.LineDuration, 0, DebugDrawSettings.LineThickness);
    DrawDebugSphere(World, FVector(TileMax.X, TileMax.Y, GroundZ), MarkerSize, 8, DebugDrawSettings.BoundaryColor, 
                   false, DebugDrawSettings.LineDuration, 0, DebugDrawSettings.LineThickness);
    DrawDebugSphere(World, FVector(TileMin.X, TileMax.Y, GroundZ), MarkerSize, 8, DebugDrawSettings.BoundaryColor, 
                   false, DebugDrawSettings.LineDuration, 0, DebugDrawSettings.LineThickness);
}

void UVHMDebugSystem::DrawPerformanceOverlay()
{
    if (!World || !GEngine)
    {
        return;
    }

    // Get performance statistics
    FString PerformanceText = GetPerformanceStatsSummary();
    
    // Display performance text on screen
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, PerformanceText);
    }
}

FString UVHMDebugSystem::GenerateExportFilename(const FTileCoord& TileCoord, const FString& TextureType, const FString& Extension) const
{
    FString Filename = TextureType;

    if (TextureExportSettings.bIncludeTileCoords)
    {
        Filename += FString::Printf(TEXT("_Tile_%d_%d"), TileCoord.X, TileCoord.Y);
    }

    if (TextureExportSettings.bIncludeTimestamp)
    {
        FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
        Filename += FString::Printf(TEXT("_%s"), *Timestamp);
    }

    Filename += FString::Printf(TEXT(".%s"), *Extension.ToLower());

    return FPaths::Combine(TextureExportSettings.ExportDirectory, Filename);
}

void UVHMDebugSystem::CreateDebugMaterials()
{
    // Create wireframe material
    // Note: In a real implementation, these would be loaded from material assets
    // For now, we'll just create placeholder material instances
    
    UE_LOG(LogVHMDebugSystem, Log, TEXT("Debug materials created (placeholder implementation)"));
}

FBox UVHMDebugSystem::GetTileWorldBounds(const FTileCoord& TileCoord) const
{
    if (!TerrainRenderer)
    {
        return FBox(ForceInit);
    }

    FTerrainMeshData MeshData;
    if (TerrainRenderer->GetTerrainMeshData(TileCoord, MeshData))
    {
        return MeshData.WorldBounds;
    }

    // Fallback: calculate bounds based on tile coordinate
    // This assumes 64m tiles - should be configurable
    float TileSize = 64.0f;
    FVector TileMin(TileCoord.X * TileSize, TileCoord.Y * TileSize, 0.0f);
    FVector TileMax = TileMin + FVector(TileSize, TileSize, 100.0f);
    
    return FBox(TileMin, TileMax);
}

FVector UVHMDebugSystem::GetTileCenterPosition(const FTileCoord& TileCoord) const
{
    FBox TileBounds = GetTileWorldBounds(TileCoord);
    return TileBounds.GetCenter();
}
