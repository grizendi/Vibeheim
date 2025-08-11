#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/WorldGenSettings.h"
#include "WorldGenConfigManager.generated.h"

/**
 * Manages loading, validation, and access to world generation configuration
 */
UCLASS(BlueprintType)
class VIBEHEIM_API UWorldGenConfigManager : public UObject
{
    GENERATED_BODY()

public:
    UWorldGenConfigManager();

    /**
     * Load configuration from the default Config/WorldGenSettings.json file
     * @return True if configuration was loaded and validated successfully
     */
    UFUNCTION(BlueprintCallable, Category = "WorldGen Config")
    bool LoadDefaultConfiguration();

    /**
     * Load configuration from a specific file path
     * @param FilePath Path to the JSON configuration file
     * @return True if configuration was loaded and validated successfully
     */
    UFUNCTION(BlueprintCallable, Category = "WorldGen Config")
    bool LoadConfiguration(const FString& FilePath);

    /**
     * Save current configuration to the default Config/WorldGenSettings.json file
     * @return True if configuration was saved successfully
     */
    UFUNCTION(BlueprintCallable, Category = "WorldGen Config")
    bool SaveDefaultConfiguration() const;

    /**
     * Save current configuration to a specific file path
     * @param FilePath Path where to save the JSON configuration file
     * @return True if configuration was saved successfully
     */
    UFUNCTION(BlueprintCallable, Category = "WorldGen Config")
    bool SaveConfiguration(const FString& FilePath) const;

    /**
     * Get the current world generation settings
     * @return Reference to the current settings
     */
    UFUNCTION(BlueprintCallable, Category = "WorldGen Config")
    const FWorldGenSettings& GetSettings() const { return Settings; }

    /**
     * Update the current world generation settings
     * @param NewSettings The new settings to apply
     * @return True if the new settings are valid and were applied
     */
    UFUNCTION(BlueprintCallable, Category = "WorldGen Config")
    bool UpdateSettings(const FWorldGenSettings& NewSettings);

    /**
     * Reset settings to default values
     */
    UFUNCTION(BlueprintCallable, Category = "WorldGen Config")
    void ResetToDefaults();

    /**
     * Check if the configuration manager has valid settings loaded
     * @return True if valid settings are loaded
     */
    UFUNCTION(BlueprintCallable, Category = "WorldGen Config")
    bool IsConfigurationValid() const { return bIsValid; }

    /**
     * Get the default configuration file path
     * @return Path to the default configuration file
     */
    static FString GetDefaultConfigPath();

private:
    /** Current world generation settings */
    UPROPERTY()
    FWorldGenSettings Settings;

    /** Whether the current settings are valid */
    UPROPERTY()
    bool bIsValid = false;

    /** Initialize with default settings */
    void InitializeDefaults();
};