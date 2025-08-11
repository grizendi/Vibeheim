#include "WorldGenConfigManager.h"
#include "VoxelPluginAdapter.h"
#include "WorldGenModule.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"

UWorldGenConfigManager::UWorldGenConfigManager()
{
    InitializeDefaults();
}

bool UWorldGenConfigManager::LoadDefaultConfiguration()
{
    return LoadConfiguration(GetDefaultConfigPath());
}

bool UWorldGenConfigManager::LoadConfiguration(const FString& FilePath)
{
    // Check if file exists
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath))
    {
        UE_LOG(LogWorldGen, Warning, TEXT("Configuration file does not exist: %s. Using default settings."), *FilePath);
        InitializeDefaults();
        return SaveConfiguration(FilePath); // Create default config file
    }

    FWorldGenSettings NewSettings;
    if (!NewSettings.LoadFromJSON(FilePath))
    {
        UE_LOG(LogWorldGen, Error, TEXT("Failed to load configuration from: %s"), *FilePath);
        return false;
    }

    if (!NewSettings.ValidateSettings())
    {
        UE_LOG(LogWorldGen, Error, TEXT("Configuration validation failed for: %s"), *FilePath);
        return false;
    }

    Settings = NewSettings;
    bIsValid = true;
    
    UE_LOG(LogWorldGen, Log, TEXT("Successfully loaded and validated configuration from: %s"), *FilePath);
    return true;
}

bool UWorldGenConfigManager::SaveDefaultConfiguration() const
{
    return SaveConfiguration(GetDefaultConfigPath());
}

bool UWorldGenConfigManager::SaveConfiguration(const FString& FilePath) const
{
    if (!bIsValid)
    {
        UE_LOG(LogWorldGen, Error, TEXT("Cannot save invalid configuration to: %s"), *FilePath);
        return false;
    }

    return Settings.SaveToJSON(FilePath);
}

bool UWorldGenConfigManager::UpdateSettings(const FWorldGenSettings& NewSettings)
{
    if (!NewSettings.ValidateSettings())
    {
        UE_LOG(LogWorldGen, Error, TEXT("Cannot update to invalid settings"));
        return false;
    }

    Settings = NewSettings;
    bIsValid = true;
    
    UE_LOG(LogWorldGen, Log, TEXT("Successfully updated world generation settings"));
    return true;
}

void UWorldGenConfigManager::ResetToDefaults()
{
    InitializeDefaults();
    UE_LOG(LogWorldGen, Log, TEXT("Reset world generation settings to defaults"));
}

FString UWorldGenConfigManager::GetDefaultConfigPath()
{
    return FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("WorldGenSettings.json"));
}

void UWorldGenConfigManager::InitializeDefaults()
{
    Settings = FWorldGenSettings(); // Use default constructor values
    bIsValid = Settings.ValidateSettings();
    
    if (!bIsValid)
    {
        UE_LOG(LogWorldGen, Error, TEXT("Default settings are invalid! This should never happen."));
    }
}