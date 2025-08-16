#include "WorldGenSettings.h"
#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

bool FWorldGenSettings::LoadFromJSON(const FString& FilePath)
{
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to load WorldGen settings from: %s"), *FilePath);
        return false;
    }

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse WorldGen settings JSON from: %s"), *FilePath);
        return false;
    }

    // Load basic settings
    if (JsonObject->HasField(TEXT("Seed")))
    {
        Seed = static_cast<int64>(JsonObject->GetNumberField(TEXT("Seed")));
    }
    
    if (JsonObject->HasField(TEXT("WorldGenVersion")))
    {
        WorldGenVersion = static_cast<int32>(JsonObject->GetNumberField(TEXT("WorldGenVersion")));
    }
    
    if (JsonObject->HasField(TEXT("PluginSHA")))
    {
        PluginSHA = JsonObject->GetStringField(TEXT("PluginSHA"));
    }

    // Load voxel settings
    if (JsonObject->HasField(TEXT("VoxelSizeCm")))
    {
        VoxelSizeCm = static_cast<float>(JsonObject->GetNumberField(TEXT("VoxelSizeCm")));
    }
    
    if (JsonObject->HasField(TEXT("ChunkSize")))
    {
        ChunkSize = static_cast<int32>(JsonObject->GetNumberField(TEXT("ChunkSize")));
    }

    // Load streaming settings
    if (JsonObject->HasField(TEXT("MaxLOD")))
    {
        MaxLOD = static_cast<int32>(JsonObject->GetNumberField(TEXT("MaxLOD")));
    }
    
    if (JsonObject->HasField(TEXT("LOD0Radius")))
    {
        LOD0Radius = static_cast<int32>(JsonObject->GetNumberField(TEXT("LOD0Radius")));
    }
    
    if (JsonObject->HasField(TEXT("LOD1Radius")))
    {
        LOD1Radius = static_cast<int32>(JsonObject->GetNumberField(TEXT("LOD1Radius")));
    }
    
    if (JsonObject->HasField(TEXT("LOD2Radius")))
    {
        LOD2Radius = static_cast<int32>(JsonObject->GetNumberField(TEXT("LOD2Radius")));
    }
    
    if (JsonObject->HasField(TEXT("bCollisionUpToLOD1")))
    {
        bCollisionUpToLOD1 = JsonObject->GetBoolField(TEXT("bCollisionUpToLOD1"));
    }

    // Load biome settings
    if (JsonObject->HasField(TEXT("BiomeBlendMeters")))
    {
        BiomeBlendMeters = static_cast<float>(JsonObject->GetNumberField(TEXT("BiomeBlendMeters")));
    }

    // Load persistence settings
    if (JsonObject->HasField(TEXT("SaveFlushMs")))
    {
        SaveFlushMs = static_cast<int32>(JsonObject->GetNumberField(TEXT("SaveFlushMs")));
    }

    // Load biome noise parameters
    if (JsonObject->HasField(TEXT("MeadowsScale")))
    {
        MeadowsScale = static_cast<float>(JsonObject->GetNumberField(TEXT("MeadowsScale")));
    }
    
    if (JsonObject->HasField(TEXT("BlackForestScale")))
    {
        BlackForestScale = static_cast<float>(JsonObject->GetNumberField(TEXT("BlackForestScale")));
    }
    
    if (JsonObject->HasField(TEXT("SwampScale")))
    {
        SwampScale = static_cast<float>(JsonObject->GetNumberField(TEXT("SwampScale")));
    }

    // Load material and generator settings
    if (JsonObject->HasField(TEXT("VoxelMaterial")))
    {
        FString MaterialPath = JsonObject->GetStringField(TEXT("VoxelMaterial"));
        if (!MaterialPath.IsEmpty())
        {
            VoxelMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(MaterialPath));
        }
    }
    
    if (JsonObject->HasField(TEXT("GeneratorClass")))
    {
        FString GeneratorPath = JsonObject->GetStringField(TEXT("GeneratorClass"));
        if (!GeneratorPath.IsEmpty())
        {
            GeneratorClass = TSoftClassPtr<UVoxelGenerator>(FSoftClassPath(GeneratorPath));
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Successfully loaded WorldGen settings from: %s"), *FilePath);
    return true;
}

bool FWorldGenSettings::SaveToJSON(const FString& FilePath) const
{
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

    // Save basic settings
    JsonObject->SetNumberField(TEXT("Seed"), static_cast<double>(Seed));
    JsonObject->SetNumberField(TEXT("WorldGenVersion"), WorldGenVersion);
    JsonObject->SetStringField(TEXT("PluginSHA"), PluginSHA);

    // Save voxel settings
    JsonObject->SetNumberField(TEXT("VoxelSizeCm"), VoxelSizeCm);
    JsonObject->SetNumberField(TEXT("ChunkSize"), ChunkSize);

    // Save streaming settings
    JsonObject->SetNumberField(TEXT("MaxLOD"), MaxLOD);
    JsonObject->SetNumberField(TEXT("LOD0Radius"), LOD0Radius);
    JsonObject->SetNumberField(TEXT("LOD1Radius"), LOD1Radius);
    JsonObject->SetNumberField(TEXT("LOD2Radius"), LOD2Radius);
    JsonObject->SetBoolField(TEXT("bCollisionUpToLOD1"), bCollisionUpToLOD1);

    // Save biome settings
    JsonObject->SetNumberField(TEXT("BiomeBlendMeters"), BiomeBlendMeters);

    // Save persistence settings
    JsonObject->SetNumberField(TEXT("SaveFlushMs"), SaveFlushMs);

    // Save biome noise parameters
    JsonObject->SetNumberField(TEXT("MeadowsScale"), MeadowsScale);
    JsonObject->SetNumberField(TEXT("BlackForestScale"), BlackForestScale);
    JsonObject->SetNumberField(TEXT("SwampScale"), SwampScale);

    // Save material and generator settings
    if (VoxelMaterial.IsValid())
    {
        JsonObject->SetStringField(TEXT("VoxelMaterial"), VoxelMaterial.GetLongPackageName());
    }
    
    if (GeneratorClass.IsValid())
    {
        JsonObject->SetStringField(TEXT("GeneratorClass"), GeneratorClass.GetLongPackageName());
    }

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    if (!FFileHelper::SaveStringToFile(OutputString, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to save WorldGen settings to: %s"), *FilePath);
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("Successfully saved WorldGen settings to: %s"), *FilePath);
    return true;
}

bool FWorldGenSettings::ValidateSettings() const
{
    bool bIsValid = true;

    // Validate voxel settings
    if (VoxelSizeCm < 1.0f || VoxelSizeCm > 200.0f)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid VoxelSizeCm: %f (must be between 1.0 and 200.0)"), VoxelSizeCm);
        bIsValid = false;
    }

    if (ChunkSize < 8 || ChunkSize > 128)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid ChunkSize: %d (must be between 8 and 128)"), ChunkSize);
        bIsValid = false;
    }

    // Validate LOD settings
    if (MaxLOD < 1 || MaxLOD > 5)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid MaxLOD: %d (must be between 1 and 5)"), MaxLOD);
        bIsValid = false;
    }

    if (LOD0Radius < 1 || LOD0Radius > 10)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid LOD0Radius: %d (must be between 1 and 10)"), LOD0Radius);
        bIsValid = false;
    }

    if (LOD1Radius < 1 || LOD1Radius > 15)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid LOD1Radius: %d (must be between 1 and 15)"), LOD1Radius);
        bIsValid = false;
    }

    if (LOD2Radius < 1 || LOD2Radius > 20)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid LOD2Radius: %d (must be between 1 and 20)"), LOD2Radius);
        bIsValid = false;
    }

    // Validate LOD radius ordering
    if (LOD0Radius >= LOD1Radius)
    {
        UE_LOG(LogTemp, Error, TEXT("LOD0Radius (%d) must be less than LOD1Radius (%d)"), LOD0Radius, LOD1Radius);
        bIsValid = false;
    }

    if (LOD1Radius >= LOD2Radius)
    {
        UE_LOG(LogTemp, Error, TEXT("LOD1Radius (%d) must be less than LOD2Radius (%d)"), LOD1Radius, LOD2Radius);
        bIsValid = false;
    }

    // Validate biome settings
    if (BiomeBlendMeters < 1.0f || BiomeBlendMeters > 100.0f)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid BiomeBlendMeters: %f (must be between 1.0 and 100.0)"), BiomeBlendMeters);
        bIsValid = false;
    }

    // Validate persistence settings
    if (SaveFlushMs < 1000 || SaveFlushMs > 10000)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid SaveFlushMs: %d (must be between 1000 and 10000)"), SaveFlushMs);
        bIsValid = false;
    }

    // Validate noise scales
    if (MeadowsScale < 0.0001f || MeadowsScale > 0.01f)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid MeadowsScale: %f (must be between 0.0001 and 0.01)"), MeadowsScale);
        bIsValid = false;
    }

    if (BlackForestScale < 0.0001f || BlackForestScale > 0.01f)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid BlackForestScale: %f (must be between 0.0001 and 0.01)"), BlackForestScale);
        bIsValid = false;
    }

    if (SwampScale < 0.0001f || SwampScale > 0.01f)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid SwampScale: %f (must be between 0.0001 and 0.01)"), SwampScale);
        bIsValid = false;
    }

    return bIsValid;
}