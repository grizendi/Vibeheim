#pragma once

#include "CoreMinimal.h"
#include "Data/WorldGenTypes.h"
#include "UObject/NoExportTypes.h"
#include "VHMTerrainRendering/VHMTypes.h"
#include "WorldGenSettings.generated.h"

class FJsonObject;

// Forward declarations
class UVHMTerrainRenderer;
class UWorldGenSettingsAsset;
class UBiomeDefinitionsAsset;

/**
 * Settings manager for world generation configuration
 * Handles loading, validation, and persistence of world generation settings
 */
UCLASS(BlueprintType, Config = Game)
class VIBEHEIM_API UWorldGenSettings : public UObject {
  GENERATED_BODY()

public:
  UWorldGenSettings();

  // Core settings data
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "WorldGen")
  FWorldGenConfig Settings;

  // VHM terrain rendering settings
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "VHM")
  TOptional<FVHMSettings> VHMSettings;

  // VHM terrain renderer instance
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VHM")
  UVHMTerrainRenderer *VHMTerrainRenderer;

  /**
   * Load settings from JSON configuration file
   * @param ConfigPath Path to the JSON configuration file (relative to project
   * root)
   * @return True if settings were loaded successfully
   */
  UFUNCTION(BlueprintCallable, Category = "WorldGen")
  bool LoadFromJSON(
      const FString &ConfigPath = TEXT("Config/WorldGenSettings.json"));

  /**
   * Save current settings to JSON configuration file
   * @param ConfigPath Path to save the JSON configuration file
   * @return True if settings were saved successfully
   */
  UFUNCTION(BlueprintCallable, Category = "WorldGen")
  bool SaveToJSON(
      const FString &ConfigPath = TEXT("Config/WorldGenSettings.json")) const;

  /**
   * Apply settings from selected Data Assets (preferred over JSON)
   * @param SettingsAsset World generation settings asset
   * @param BiomeAsset Biome definitions asset (optional; used for feature
   * gating)
   * @param OutWarnings Any non-fatal warnings generated during application
   * @return True if any settings were applied from assets
   */
  UFUNCTION(BlueprintCallable, Category = "WorldGen|Assets")
  bool ApplyFromAssets(const UWorldGenSettingsAsset *SettingsAsset,
                       const UBiomeDefinitionsAsset *BiomeAsset,
                       TArray<FString> &OutWarnings);

  /**
   * Validate current settings and fix any invalid values
   * @param OutValidationErrors Array to store validation error messages
   * @return True if all settings are valid (or were successfully corrected)
   */
  UFUNCTION(BlueprintCallable, Category = "WorldGen")
  bool ValidateSettings(TArray<FString> &OutValidationErrors);

  /**
   * Reset settings to default values
   */
  UFUNCTION(BlueprintCallable, Category = "WorldGen")
  void ResetToDefaults();

  /**
   * Get the singleton instance of the settings manager
   */
  UFUNCTION(BlueprintCallable, Category = "WorldGen")
  static UWorldGenSettings *GetWorldGenSettings();

  /** Returns true when streaming systems should run at runtime (non-baked). */
  UFUNCTION(BlueprintCallable, Category = "WorldGen")
  bool IsRuntimeGenerationEnabled() const;

  /**
   * Get the world generation configuration
   */
  UFUNCTION(BlueprintCallable, Category = "WorldGen")
  const FWorldGenConfig &GetWorldGenConfig() const { return Settings; }

  /**
   * Apply locked coordinate system values (cannot be changed)
   */
  UFUNCTION(BlueprintCallable, Category = "WorldGen")
  void ApplyLockedValues();
  // Validation helpers
  void ClampSettingValue(float &Value, float MinValue, float MaxValue,
                         const FString &SettingName,
                         TArray<FString> &OutErrors);
  void ClampSettingValue(int32 &Value, int32 MinValue, int32 MaxValue,
                         const FString &SettingName,
                         TArray<FString> &OutErrors);

  // Singleton instance
  static UWorldGenSettings *Instance;

public:
  /** Optional soft references to selected assets (mirrors AWorldGenManager) */
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WorldGen|Assets")
  TSoftObjectPtr<UWorldGenSettingsAsset> SelectedSettingsAsset;

  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WorldGen|Assets")
  TSoftObjectPtr<UBiomeDefinitionsAsset> SelectedBiomeDefinitionsAsset;

  /** Applied Data Asset configurations (stored for services to consume; no
   * reflection needed) */
  TOptional<FMacroWorldConfig> MacroWorldConfig;
  TOptional<FStreamingBudgetsConfig> StreamingBudgetsConfig;
  TOptional<FRiverSystemConfig> RiverSystemConfig;
  TOptional<FWaterSystemConfig> WaterSystemConfig;

private:
  bool ParseJSONObject(const TSharedPtr<FJsonObject> &JsonObject);
  TSharedPtr<FJsonObject> CreateJSONObject() const;
  bool ValidateHeightfieldSettings(TArray<FString> &OutErrors);
  bool ValidateStreamingSettings(TArray<FString> &OutErrors);
  bool ValidatePCGSettings(TArray<FString> &OutErrors);
  bool ValidatePerformanceSettings(TArray<FString> &OutErrors);
  bool ValidateVHMSettings(TArray<FString> &OutErrors);
};
