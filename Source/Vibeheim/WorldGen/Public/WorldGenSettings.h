#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/WorldGenTypes.h"
#include "WorldGenSettings.generated.h"

/**
 * Settings manager for world generation configuration
 * Handles loading, validation, and persistence of world generation settings
 */
UCLASS(BlueprintType, Config=Game)
class VIBEHEIM_API UWorldGenSettings : public UObject
{
	GENERATED_BODY()

public:
	UWorldGenSettings();

	// Core settings data
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "WorldGen")
	FWorldGenConfig Settings;

	/**
	 * Load settings from JSON configuration file
	 * @param ConfigPath Path to the JSON configuration file (relative to project root)
	 * @return True if settings were loaded successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldGen")
	bool LoadFromJSON(const FString& ConfigPath = TEXT("Config/WorldGenSettings.json"));

	/**
	 * Save current settings to JSON configuration file
	 * @param ConfigPath Path to save the JSON configuration file
	 * @return True if settings were saved successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldGen")
	bool SaveToJSON(const FString& ConfigPath = TEXT("Config/WorldGenSettings.json")) const;

	/**
	 * Validate current settings and fix any invalid values
	 * @param OutValidationErrors Array to store validation error messages
	 * @return True if all settings are valid (or were successfully corrected)
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldGen")
	bool ValidateSettings(TArray<FString>& OutValidationErrors);

	/**
	 * Reset settings to default values
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldGen")
	void ResetToDefaults();

	/**
	 * Get the singleton instance of the settings manager
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldGen")
	static UWorldGenSettings* GetWorldGenSettings();

	/**
	 * Apply locked coordinate system values (cannot be changed)
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldGen")
	void ApplyLockedValues();

	// Validation functions for specific setting categories
	bool ValidateHeightfieldSettings(TArray<FString>& OutErrors);
	bool ValidateStreamingSettings(TArray<FString>& OutErrors);
	bool ValidatePCGSettings(TArray<FString>& OutErrors);
	bool ValidatePerformanceSettings(TArray<FString>& OutErrors);

private:
	// Internal JSON parsing helpers
	bool ParseJSONObject(const TSharedPtr<FJsonObject>& JsonObject);
	TSharedPtr<FJsonObject> CreateJSONObject() const;
	
	// Validation helpers
	void ClampSettingValue(float& Value, float MinValue, float MaxValue, const FString& SettingName, TArray<FString>& OutErrors);
	void ClampSettingValue(int32& Value, int32 MinValue, int32 MaxValue, const FString& SettingName, TArray<FString>& OutErrors);

	// Singleton instance
	static UWorldGenSettings* Instance;
};