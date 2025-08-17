#include "WorldGenSettings.h"
#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorldGenSettings, Log, All);

// Initialize singleton instance
UWorldGenSettings* UWorldGenSettings::Instance = nullptr;

UWorldGenSettings::UWorldGenSettings()
{
	// Initialize with default settings
	Settings = FWorldGenConfig();
	ApplyLockedValues();
}

UWorldGenSettings* UWorldGenSettings::GetWorldGenSettings()
{
	if (!Instance)
	{
		Instance = NewObject<UWorldGenSettings>();
		Instance->AddToRoot(); // Prevent garbage collection
		
		// Try to load settings from default location
		Instance->LoadFromJSON();
	}
	return Instance;
}

bool UWorldGenSettings::LoadFromJSON(const FString& ConfigPath)
{
	const FString FullPath = FPaths::ProjectDir() / ConfigPath;
	
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FullPath))
	{
		UE_LOG(LogWorldGenSettings, Warning, TEXT("Could not load WorldGen settings from %s. Using defaults."), *FullPath);
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogWorldGenSettings, Error, TEXT("Failed to parse JSON from %s"), *FullPath);
		return false;
	}

	if (!ParseJSONObject(JsonObject))
	{
		UE_LOG(LogWorldGenSettings, Error, TEXT("Failed to parse WorldGen settings from JSON"));
		return false;
	}

	// Apply locked values and validate
	ApplyLockedValues();
	TArray<FString> ValidationErrors;
	ValidateSettings(ValidationErrors);

	UE_LOG(LogWorldGenSettings, Log, TEXT("Successfully loaded WorldGen settings from %s"), *FullPath);
	return true;
}

bool UWorldGenSettings::SaveToJSON(const FString& ConfigPath) const
{
	const FString FullPath = FPaths::ProjectDir() / ConfigPath;
	
	TSharedPtr<FJsonObject> JsonObject = CreateJSONObject();
	if (!JsonObject.IsValid())
	{
		UE_LOG(LogWorldGenSettings, Error, TEXT("Failed to create JSON object for settings"));
		return false;
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	if (!FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
	{
		UE_LOG(LogWorldGenSettings, Error, TEXT("Failed to serialize settings to JSON"));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(OutputString, *FullPath))
	{
		UE_LOG(LogWorldGenSettings, Error, TEXT("Failed to save settings to file %s"), *FullPath);
		return false;
	}

	UE_LOG(LogWorldGenSettings, Log, TEXT("Successfully saved WorldGen settings to %s"), *FullPath);
	return true;
}

bool UWorldGenSettings::ParseJSONObject(const TSharedPtr<FJsonObject>& JsonObject)
{
	if (!JsonObject.IsValid())
	{
		return false;
	}

	// Parse core generation settings
	if (JsonObject->HasField(TEXT("Seed")))
	{
		Settings.Seed = static_cast<uint64>(JsonObject->GetNumberField(TEXT("Seed")));
	}
	
	if (JsonObject->HasField(TEXT("WorldGenVersion")))
	{
		Settings.WorldGenVersion = static_cast<int32>(JsonObject->GetNumberField(TEXT("WorldGenVersion")));
	}

	// Parse coordinate system (these will be locked to specific values)
	if (JsonObject->HasField(TEXT("TileSizeMeters")))
	{
		Settings.TileSizeMeters = static_cast<float>(JsonObject->GetNumberField(TEXT("TileSizeMeters")));
	}
	
	if (JsonObject->HasField(TEXT("SampleSpacingMeters")))
	{
		Settings.SampleSpacingMeters = static_cast<float>(JsonObject->GetNumberField(TEXT("SampleSpacingMeters")));
	}
	
	if (JsonObject->HasField(TEXT("MaxTerrainHeight")))
	{
		Settings.MaxTerrainHeight = static_cast<float>(JsonObject->GetNumberField(TEXT("MaxTerrainHeight")));
	}
	
	if (JsonObject->HasField(TEXT("SeaLevel")))
	{
		Settings.SeaLevel = static_cast<float>(JsonObject->GetNumberField(TEXT("SeaLevel")));
	}

	// Parse streaming settings
	if (JsonObject->HasField(TEXT("GenerateRadius")))
	{
		Settings.GenerateRadius = static_cast<int32>(JsonObject->GetNumberField(TEXT("GenerateRadius")));
	}
	
	if (JsonObject->HasField(TEXT("LoadRadius")))
	{
		Settings.LoadRadius = static_cast<int32>(JsonObject->GetNumberField(TEXT("LoadRadius")));
	}
	
	if (JsonObject->HasField(TEXT("ActiveRadius")))
	{
		Settings.ActiveRadius = static_cast<int32>(JsonObject->GetNumberField(TEXT("ActiveRadius")));
	}

	// Parse heightfield settings
	if (JsonObject->HasField(TEXT("HeightfieldScale")))
	{
		Settings.HeightfieldScale = static_cast<float>(JsonObject->GetNumberField(TEXT("HeightfieldScale")));
	}
	
	if (JsonObject->HasField(TEXT("HeightfieldResolution")))
	{
		Settings.HeightfieldResolution = static_cast<int32>(JsonObject->GetNumberField(TEXT("HeightfieldResolution")));
	}

	// Parse World Partition settings
	if (JsonObject->HasField(TEXT("CellSize")))
	{
		Settings.CellSize = static_cast<int32>(JsonObject->GetNumberField(TEXT("CellSize")));
	}

	// Parse PCG settings
	if (JsonObject->HasField(TEXT("VegetationDensity")))
	{
		Settings.VegetationDensity = static_cast<float>(JsonObject->GetNumberField(TEXT("VegetationDensity")));
	}
	
	if (JsonObject->HasField(TEXT("POIDensity")))
	{
		Settings.POIDensity = static_cast<float>(JsonObject->GetNumberField(TEXT("POIDensity")));
	}
	
	if (JsonObject->HasField(TEXT("MaxHISMInstances")))
	{
		Settings.MaxHISMInstances = static_cast<int32>(JsonObject->GetNumberField(TEXT("MaxHISMInstances")));
	}

	// Parse biome settings
	if (JsonObject->HasField(TEXT("BiomeScale")))
	{
		Settings.BiomeScale = static_cast<float>(JsonObject->GetNumberField(TEXT("BiomeScale")));
	}
	
	if (JsonObject->HasField(TEXT("BiomeBlendDistance")))
	{
		Settings.BiomeBlendDistance = static_cast<float>(JsonObject->GetNumberField(TEXT("BiomeBlendDistance")));
	}

	// Parse RVT settings
	if (JsonObject->HasField(TEXT("RVTResolution")))
	{
		Settings.RVTResolution = static_cast<int32>(JsonObject->GetNumberField(TEXT("RVTResolution")));
	}
	
	if (JsonObject->HasField(TEXT("RVTTileSize")))
	{
		Settings.RVTTileSize = static_cast<int32>(JsonObject->GetNumberField(TEXT("RVTTileSize")));
	}

	// Parse performance targets
	if (JsonObject->HasField(TEXT("TileGenTargetMs")))
	{
		Settings.TileGenTargetMs = static_cast<float>(JsonObject->GetNumberField(TEXT("TileGenTargetMs")));
	}
	
	if (JsonObject->HasField(TEXT("PCGTargetMsPerTile")))
	{
		Settings.PCGTargetMsPerTile = static_cast<float>(JsonObject->GetNumberField(TEXT("PCGTargetMsPerTile")));
	}

	// Parse nested performance targets object if it exists
	if (JsonObject->HasField(TEXT("PerfTargets")))
	{
		const TSharedPtr<FJsonObject> PerfTargets = JsonObject->GetObjectField(TEXT("PerfTargets"));
		if (PerfTargets.IsValid())
		{
			if (PerfTargets->HasField(TEXT("TileGenMs")))
			{
				Settings.TileGenTargetMs = static_cast<float>(PerfTargets->GetNumberField(TEXT("TileGenMs")));
			}
			
			if (PerfTargets->HasField(TEXT("PCGMsPerTile")))
			{
				Settings.PCGTargetMsPerTile = static_cast<float>(PerfTargets->GetNumberField(TEXT("PCGMsPerTile")));
			}
		}
	}

	return true;
}

TSharedPtr<FJsonObject> UWorldGenSettings::CreateJSONObject() const
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

	// Core generation settings
	JsonObject->SetNumberField(TEXT("Seed"), static_cast<double>(Settings.Seed));
	JsonObject->SetNumberField(TEXT("WorldGenVersion"), Settings.WorldGenVersion);

	// Coordinate system (locked values)
	JsonObject->SetNumberField(TEXT("TileSizeMeters"), Settings.TileSizeMeters);
	JsonObject->SetNumberField(TEXT("SampleSpacingMeters"), Settings.SampleSpacingMeters);
	JsonObject->SetNumberField(TEXT("MaxTerrainHeight"), Settings.MaxTerrainHeight);
	JsonObject->SetNumberField(TEXT("SeaLevel"), Settings.SeaLevel);

	// Streaming settings
	JsonObject->SetNumberField(TEXT("GenerateRadius"), Settings.GenerateRadius);
	JsonObject->SetNumberField(TEXT("LoadRadius"), Settings.LoadRadius);
	JsonObject->SetNumberField(TEXT("ActiveRadius"), Settings.ActiveRadius);

	// Heightfield settings
	JsonObject->SetNumberField(TEXT("HeightfieldScale"), Settings.HeightfieldScale);
	JsonObject->SetNumberField(TEXT("HeightfieldResolution"), Settings.HeightfieldResolution);

	// World Partition settings
	JsonObject->SetNumberField(TEXT("CellSize"), Settings.CellSize);

	// PCG settings
	JsonObject->SetNumberField(TEXT("VegetationDensity"), Settings.VegetationDensity);
	JsonObject->SetNumberField(TEXT("POIDensity"), Settings.POIDensity);
	JsonObject->SetNumberField(TEXT("MaxHISMInstances"), Settings.MaxHISMInstances);

	// Biome settings
	JsonObject->SetNumberField(TEXT("BiomeScale"), Settings.BiomeScale);
	JsonObject->SetNumberField(TEXT("BiomeBlendDistance"), Settings.BiomeBlendDistance);

	// RVT settings
	JsonObject->SetNumberField(TEXT("RVTResolution"), Settings.RVTResolution);
	JsonObject->SetNumberField(TEXT("RVTTileSize"), Settings.RVTTileSize);

	// Performance targets (nested object for compatibility)
	TSharedPtr<FJsonObject> PerfTargets = MakeShareable(new FJsonObject);
	PerfTargets->SetNumberField(TEXT("TileGenMs"), Settings.TileGenTargetMs);
	PerfTargets->SetNumberField(TEXT("PCGMsPerTile"), Settings.PCGTargetMsPerTile);
	JsonObject->SetObjectField(TEXT("PerfTargets"), PerfTargets);

	return JsonObject;
}

bool UWorldGenSettings::ValidateSettings(TArray<FString>& OutValidationErrors)
{
	OutValidationErrors.Empty();
	
	bool bAllValid = true;
	
	// Apply locked values first
	ApplyLockedValues();
	
	// Validate each category
	bAllValid &= ValidateHeightfieldSettings(OutValidationErrors);
	bAllValid &= ValidateStreamingSettings(OutValidationErrors);
	bAllValid &= ValidatePCGSettings(OutValidationErrors);
	bAllValid &= ValidatePerformanceSettings(OutValidationErrors);

	// Log validation results
	if (OutValidationErrors.Num() > 0)
	{
		UE_LOG(LogWorldGenSettings, Warning, TEXT("WorldGen settings validation found %d issues:"), OutValidationErrors.Num());
		for (const FString& Error : OutValidationErrors)
		{
			UE_LOG(LogWorldGenSettings, Warning, TEXT("  - %s"), *Error);
		}
	}
	else
	{
		UE_LOG(LogWorldGenSettings, Log, TEXT("WorldGen settings validation passed"));
	}

	return bAllValid;
}

void UWorldGenSettings::ResetToDefaults()
{
	Settings = FWorldGenConfig();
	ApplyLockedValues();
	UE_LOG(LogWorldGenSettings, Log, TEXT("WorldGen settings reset to defaults"));
}

void UWorldGenSettings::ApplyLockedValues()
{
	// These values are locked per the coordinate system specification
	Settings.TileSizeMeters = 64.0f;
	Settings.SampleSpacingMeters = 1.0f;
	Settings.MaxTerrainHeight = 120.0f;
}

bool UWorldGenSettings::ValidateHeightfieldSettings(TArray<FString>& OutErrors)
{
	bool bValid = true;

	// Validate heightfield resolution (must be power of 2, reasonable range)
	if (Settings.HeightfieldResolution < 64 || Settings.HeightfieldResolution > 4096)
	{
		ClampSettingValue(Settings.HeightfieldResolution, 64, 4096, TEXT("HeightfieldResolution"), OutErrors);
		bValid = false;
	}
	
	// Check if it's a power of 2
	if ((Settings.HeightfieldResolution & (Settings.HeightfieldResolution - 1)) != 0)
	{
		// Find nearest power of 2
		int32 NearestPowerOf2 = 1;
		while (NearestPowerOf2 < Settings.HeightfieldResolution)
		{
			NearestPowerOf2 <<= 1;
		}
		
		// Choose the closer power of 2
		int32 LowerPowerOf2 = NearestPowerOf2 >> 1;
		if (FMath::Abs(Settings.HeightfieldResolution - LowerPowerOf2) < FMath::Abs(Settings.HeightfieldResolution - NearestPowerOf2))
		{
			NearestPowerOf2 = LowerPowerOf2;
		}
		
		OutErrors.Add(FString::Printf(TEXT("HeightfieldResolution must be power of 2. Changed from %d to %d"), 
			Settings.HeightfieldResolution, NearestPowerOf2));
		Settings.HeightfieldResolution = NearestPowerOf2;
		bValid = false;
	}

	// Validate heightfield scale
	ClampSettingValue(Settings.HeightfieldScale, 1.0f, 1000.0f, TEXT("HeightfieldScale"), OutErrors);

	return bValid;
}

bool UWorldGenSettings::ValidateStreamingSettings(TArray<FString>& OutErrors)
{
	bool bValid = true;

	// Validate streaming radii relationships (Generate >= Load >= Active)
	ClampSettingValue(Settings.GenerateRadius, 1, 20, TEXT("GenerateRadius"), OutErrors);
	ClampSettingValue(Settings.LoadRadius, 1, 15, TEXT("LoadRadius"), OutErrors);
	ClampSettingValue(Settings.ActiveRadius, 1, 10, TEXT("ActiveRadius"), OutErrors);

	// Ensure proper radius relationships
	if (Settings.LoadRadius > Settings.GenerateRadius)
	{
		OutErrors.Add(FString::Printf(TEXT("LoadRadius (%d) cannot be greater than GenerateRadius (%d). Setting LoadRadius to %d"), 
			Settings.LoadRadius, Settings.GenerateRadius, Settings.GenerateRadius));
		Settings.LoadRadius = Settings.GenerateRadius;
		bValid = false;
	}

	if (Settings.ActiveRadius > Settings.LoadRadius)
	{
		OutErrors.Add(FString::Printf(TEXT("ActiveRadius (%d) cannot be greater than LoadRadius (%d). Setting ActiveRadius to %d"), 
			Settings.ActiveRadius, Settings.LoadRadius, Settings.LoadRadius));
		Settings.ActiveRadius = Settings.LoadRadius;
		bValid = false;
	}

	// Validate cell size (should be reasonable for World Partition)
	ClampSettingValue(Settings.CellSize, 1600, 25600, TEXT("CellSize"), OutErrors); // 16m to 256m

	return bValid;
}

bool UWorldGenSettings::ValidatePCGSettings(TArray<FString>& OutErrors)
{
	bool bValid = true;

	// Validate PCG density values
	ClampSettingValue(Settings.VegetationDensity, 0.0f, 10.0f, TEXT("VegetationDensity"), OutErrors);
	ClampSettingValue(Settings.POIDensity, 0.0f, 1.0f, TEXT("POIDensity"), OutErrors);
	
	// Validate HISM instance limits
	ClampSettingValue(Settings.MaxHISMInstances, 100, 100000, TEXT("MaxHISMInstances"), OutErrors);

	// Validate biome settings
	ClampSettingValue(Settings.BiomeScale, 0.0001f, 0.01f, TEXT("BiomeScale"), OutErrors);
	ClampSettingValue(Settings.BiomeBlendDistance, 10.0f, 2000.0f, TEXT("BiomeBlendDistance"), OutErrors);

	// Validate RVT settings
	if (Settings.RVTResolution < 512 || Settings.RVTResolution > 8192)
	{
		ClampSettingValue(Settings.RVTResolution, 512, 8192, TEXT("RVTResolution"), OutErrors);
		bValid = false;
	}
	
	// RVT tile size should be power of 2
	if ((Settings.RVTTileSize & (Settings.RVTTileSize - 1)) != 0)
	{
		int32 NearestPowerOf2 = 1;
		while (NearestPowerOf2 < Settings.RVTTileSize)
		{
			NearestPowerOf2 <<= 1;
		}
		
		int32 LowerPowerOf2 = NearestPowerOf2 >> 1;
		if (FMath::Abs(Settings.RVTTileSize - LowerPowerOf2) < FMath::Abs(Settings.RVTTileSize - NearestPowerOf2))
		{
			NearestPowerOf2 = LowerPowerOf2;
		}
		
		OutErrors.Add(FString::Printf(TEXT("RVTTileSize must be power of 2. Changed from %d to %d"), 
			Settings.RVTTileSize, NearestPowerOf2));
		Settings.RVTTileSize = NearestPowerOf2;
		bValid = false;
	}

	ClampSettingValue(Settings.RVTTileSize, 64, 512, TEXT("RVTTileSize"), OutErrors);

	return bValid;
}

bool UWorldGenSettings::ValidatePerformanceSettings(TArray<FString>& OutErrors)
{
	bool bValid = true;

	// Validate performance targets (reasonable ranges for frame time budgets)
	ClampSettingValue(Settings.TileGenTargetMs, 0.1f, 10.0f, TEXT("TileGenTargetMs"), OutErrors);
	ClampSettingValue(Settings.PCGTargetMsPerTile, 0.1f, 5.0f, TEXT("PCGTargetMsPerTile"), OutErrors);

	return bValid;
}

void UWorldGenSettings::ClampSettingValue(float& Value, float MinValue, float MaxValue, const FString& SettingName, TArray<FString>& OutErrors)
{
	if (Value < MinValue || Value > MaxValue)
	{
		float OriginalValue = Value;
		Value = FMath::Clamp(Value, MinValue, MaxValue);
		OutErrors.Add(FString::Printf(TEXT("%s value %.2f is out of range [%.2f, %.2f]. Clamped to %.2f"), 
			*SettingName, OriginalValue, MinValue, MaxValue, Value));
	}
}

void UWorldGenSettings::ClampSettingValue(int32& Value, int32 MinValue, int32 MaxValue, const FString& SettingName, TArray<FString>& OutErrors)
{
	if (Value < MinValue || Value > MaxValue)
	{
		int32 OriginalValue = Value;
		Value = FMath::Clamp(Value, MinValue, MaxValue);
		OutErrors.Add(FString::Printf(TEXT("%s value %d is out of range [%d, %d]. Clamped to %d"), 
			*SettingName, OriginalValue, MinValue, MaxValue, Value));
	}
}