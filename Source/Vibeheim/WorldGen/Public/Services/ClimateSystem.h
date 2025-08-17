#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/WorldGenTypes.h"
#include "ClimateSystem.generated.h"

/**
 * Climate data structure containing temperature and moisture values
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FClimateData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climate")
	float Temperature = 0.0f; // Celsius

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climate")
	float Moisture = 0.0f; // 0.0 to 1.0

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climate")
	float RingBias = 0.0f; // Distance-based bias from world center

	FClimateData() = default;
	FClimateData(float InTemperature, float InMoisture, float InRingBias)
		: Temperature(InTemperature), Moisture(InMoisture), RingBias(InRingBias) {}
};

/**
 * Climate system settings for temperature and moisture generation
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FClimateSettings
{
	GENERATED_BODY()

	// Temperature settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temperature")
	float BaseTemperature = 15.0f; // Base temperature in Celsius

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temperature")
	float LatitudinalRange = 30.0f; // Temperature variation from equator to poles

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temperature")
	float AltitudeLapseRate = 6.5f; // Temperature drop per 1000m altitude (Celsius/km)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temperature")
	float TemperatureNoiseScale = 0.001f; // Scale for temperature noise

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temperature")
	float TemperatureNoiseAmplitude = 5.0f; // Amplitude of temperature noise

	// Moisture settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moisture")
	float BaseMoisture = 0.5f; // Base moisture level (0.0 to 1.0)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moisture")
	float CoastInfluenceDistance = 2000.0f; // Distance from coast that affects moisture

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moisture")
	float MoistureNoiseScale = 0.0015f; // Scale for moisture noise

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moisture")
	float MoistureNoiseAmplitude = 0.3f; // Amplitude of moisture noise

	// Ring system settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rings")
	float WorldCenterX = 0.0f; // World center X coordinate

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rings")
	float WorldCenterY = 0.0f; // World center Y coordinate

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rings")
	float RingInfluenceRadius = 5000.0f; // Maximum distance for ring influence

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rings")
	float RingBiasStrength = 1.0f; // Strength of ring bias effect

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rings")
	float RingFalloffExponent = 2.0f; // Falloff curve for ring influence
};

/**
 * Climate system responsible for generating temperature, moisture, and ring bias data
 * Used for biome determination and environmental effects
 */
UCLASS(BlueprintType)
class VIBEHEIM_API UClimateSystem : public UObject
{
	GENERATED_BODY()

public:
	UClimateSystem();

	/**
	 * Initialize the climate system with settings
	 */
	UFUNCTION(BlueprintCallable, Category = "Climate")
	void Initialize(const FClimateSettings& InSettings, uint64 InSeed);

	/**
	 * Calculate climate data for a specific world position
	 */
	UFUNCTION(BlueprintCallable, Category = "Climate")
	FClimateData CalculateClimate(FVector2D WorldPosition, float Altitude = 0.0f) const;

	/**
	 * Calculate temperature at a specific world position
	 */
	UFUNCTION(BlueprintCallable, Category = "Climate")
	float CalculateTemperature(FVector2D WorldPosition, float Altitude = 0.0f) const;

	/**
	 * Calculate moisture at a specific world position
	 */
	UFUNCTION(BlueprintCallable, Category = "Climate")
	float CalculateMoisture(FVector2D WorldPosition) const;

	/**
	 * Calculate ring bias based on distance from world center
	 */
	UFUNCTION(BlueprintCallable, Category = "Climate")
	float CalculateRingBias(FVector2D WorldPosition) const;

	/**
	 * Generate climate data for an entire tile
	 */
	UFUNCTION(BlueprintCallable, Category = "Climate")
	TArray<FClimateData> GenerateTileClimateData(FTileCoord TileCoord, const TArray<float>& HeightData) const;

	/**
	 * Export climate data as PNG for debugging
	 */
	UFUNCTION(BlueprintCallable, Category = "Climate")
	bool ExportClimatePNG(FTileCoord TileCoord, const TArray<float>& HeightData, const FString& OutputPath) const;

	/**
	 * Get current climate settings
	 */
	UFUNCTION(BlueprintCallable, Category = "Climate")
	const FClimateSettings& GetClimateSettings() const { return Settings; }

	/**
	 * Update climate settings at runtime
	 */
	UFUNCTION(BlueprintCallable, Category = "Climate")
	void UpdateClimateSettings(const FClimateSettings& NewSettings);

private:
	UPROPERTY()
	FClimateSettings Settings;

	UPROPERTY()
	uint64 Seed;

	/**
	 * Calculate latitude-based temperature variation
	 */
	float CalculateLatitudinalTemperature(float WorldY) const;

	/**
	 * Calculate altitude-based temperature lapse
	 */
	float CalculateAltitudeLapse(float Altitude) const;

	/**
	 * Calculate distance to nearest coast (simplified)
	 */
	float CalculateCoastDistance(FVector2D WorldPosition) const;

	/**
	 * Generate deterministic noise for climate variation
	 */
	float GenerateClimateNoise(FVector2D WorldPosition, float Scale, uint32 NoiseType) const;

	/**
	 * Hash function for deterministic noise generation
	 */
	uint32 HashPosition(FVector2D Position, uint32 NoiseType) const;
};