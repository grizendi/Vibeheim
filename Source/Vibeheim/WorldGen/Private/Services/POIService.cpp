#include "Services/POIService.h"
#include "Services/BiomeService.h"
#include "Services/HeightfieldService.h"
#include "Engine/World.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogPOIService, Log, All);

UPOIService::UPOIService()
{
	BiomeService = nullptr;
	HeightfieldService = nullptr;
	TotalGenerationTime = 0.0f;
	GenerationCount = 0;
	
	// Initialize default settings
	SamplingConfig.GridSize = 4;
	SamplingConfig.CellSize = 16.0f;
	SamplingConfig.MaxAttemptsPerCell = 3;
	SamplingConfig.MinCellSpacing = 8.0f;
	
	ValidationSettings.FlatGroundCheckRadius = 3.0f;
	ValidationSettings.FlatGroundTolerance = 2.0f;
	ValidationSettings.TerrainStampRadius = 5.0f;
	ValidationSettings.TerrainStampStrength = 0.8f;
}

bool UPOIService::Initialize(const FWorldGenConfig& Settings)
{
	WorldGenSettings = Settings;
	PersistenceDirectory = FPaths::ProjectSavedDir() / TEXT("WorldGen") / TEXT("POI");
	
	// Ensure persistence directory exists
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*PersistenceDirectory))
	{
		PlatformFile.CreateDirectoryTree(*PersistenceDirectory);
	}
	
	UE_LOG(LogPOIService, Log, TEXT("POI Service initialized with seed %d"), Settings.Seed);
	return true;
}

TArray<FPOIData> UPOIService::GenerateTilePOIs(FTileCoord TileCoord, EBiomeType BiomeType, const TArray<float>& HeightData)
{
	double StartTime = FPlatformTime::Seconds();
	TArray<FPOIData> GeneratedPOIs;
	
	// Skip if no biome service available
	if (!BiomeService)
	{
		UE_LOG(LogPOIService, Warning, TEXT("BiomeService not set, cannot generate POIs for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
		return GeneratedPOIs;
	}
	
	// Get biome-specific POI rules
	TArray<FPOISpawnRule> POIRules = GetBiomePOIRules(BiomeType);
	if (POIRules.Num() == 0)
	{
		return GeneratedPOIs;
	}
	
	// Generate stratified sampling points
	TArray<FVector2D> SamplePoints = GenerateStratifiedSamplePoints(TileCoord, WorldGenSettings.Seed);
	
	// Attempt to place POIs at sample points
	for (int32 i = 0; i < SamplePoints.Num(); ++i)
	{
		FVector2D SamplePoint = SamplePoints[i];
		FVector WorldLocation = FVector(SamplePoint.X, SamplePoint.Y, 0.0f);
		
		// Try each POI rule for this sample point
		for (const FPOISpawnRule& Rule : POIRules)
		{
			// Check spawn chance
			float RandomValue = GenerateSeededRandom(WorldGenSettings.Seed, TileCoord, i);
			if (RandomValue > Rule.SpawnChance)
			{
				continue;
			}
			
			// Validate placement
			if (!ValidatePOIPlacement(WorldLocation, Rule, HeightData, TileCoord))
			{
				continue;
			}
			
			// Check distance requirements
			if (!CheckPOIDistanceRequirements(WorldLocation, Rule, GeneratedPOIs))
			{
				continue;
			}
			
			// Create POI data
			FPOIData NewPOI;
			NewPOI.POIName = Rule.POIName;
			NewPOI.Location = WorldLocation;
			NewPOI.Location.Z = GetHeightAtTileLocation(WorldToTileLocal(WorldLocation, TileCoord), HeightData, TileCoord);
			NewPOI.POIBlueprint = Rule.POIBlueprint;
			NewPOI.OriginBiome = BiomeType;
			
			GeneratedPOIs.Add(NewPOI);
			AllPOIs.Add(NewPOI.POIId, NewPOI);
			
			// Only place one POI per sample point
			break;
		}
	}
	
	// Cache generated POIs for this tile
	TilePOIs.Add(TileCoord, FPOITileData(GeneratedPOIs));
	
	// Update performance stats
	float GenerationTime = (FPlatformTime::Seconds() - StartTime) * 1000.0f;
	UpdatePerformanceStats(GenerationTime);
	
	UE_LOG(LogPOIService, Log, TEXT("Generated %d POIs for tile (%d, %d) in %.2fms"), 
		GeneratedPOIs.Num(), TileCoord.X, TileCoord.Y, GenerationTime);
	
	return GeneratedPOIs;
}

bool UPOIService::ValidatePOIPlacement(FVector Location, const FPOISpawnRule& Rule, const TArray<float>& HeightData, FTileCoord TileCoord)
{
	FVector2D LocalPos = WorldToTileLocal(Location, TileCoord);
	
	// Check slope requirements
	if (!CheckSlopeRequirements(Location, Rule.SlopeLimit, HeightData, TileCoord))
	{
		return false;
	}
	
	// Check flat ground requirements if needed
	if (Rule.bRequiresFlatGround && !ValidateFlatGround(Location, HeightData, TileCoord))
	{
		return false;
	}
	
	return true;
}

bool UPOIService::CheckPOIDistanceRequirements(FVector Location, const FPOISpawnRule& Rule, const TArray<FPOIData>& ExistingPOIs)
{
	for (const FPOIData& ExistingPOI : ExistingPOIs)
	{
		float Distance = FVector::Dist2D(Location, ExistingPOI.Location);
		if (Distance < Rule.MinDistanceFromOthers)
		{
			return false;
		}
	}
	
	// Also check against POIs from neighboring tiles
	FTileCoord CurrentTile = FTileCoord::FromWorldPosition(Location, WorldGenSettings.TileSizeMeters);
	for (int32 X = -1; X <= 1; ++X)
	{
		for (int32 Y = -1; Y <= 1; ++Y)
		{
			FTileCoord NeighborTile(CurrentTile.X + X, CurrentTile.Y + Y);
			if (const FPOITileData* NeighborTileData = TilePOIs.Find(NeighborTile))
			{
				for (const FPOIData& NeighborPOI : NeighborTileData->POIs)
				{
					float Distance = FVector::Dist2D(Location, NeighborPOI.Location);
					if (Distance < Rule.MinDistanceFromOthers)
					{
						return false;
					}
				}
			}
		}
	}
	
	return true;
}

bool UPOIService::ApplyTerrainStamp(FVector Location, float Radius, TArray<float>& HeightData, FTileCoord TileCoord)
{
	FVector2D LocalPos = WorldToTileLocal(Location, TileCoord);
	float TargetHeight = GetHeightAtTileLocation(LocalPos, HeightData, TileCoord);
	
	ApplyFlatteningStamp(LocalPos, Radius, ValidationSettings.TerrainStampStrength, HeightData, TileCoord);
	
	UE_LOG(LogPOIService, Verbose, TEXT("Applied terrain stamp at (%f, %f) with radius %f"), 
		Location.X, Location.Y, Radius);
	
	return true;
}

TArray<FPOIData> UPOIService::GetPOIsInArea(FVector Center, float Radius)
{
	TArray<FPOIData> POIsInArea;
	
	for (const auto& POIPair : AllPOIs)
	{
		const FPOIData& POI = POIPair.Value;
		float Distance = FVector::Dist2D(Center, POI.Location);
		if (Distance <= Radius)
		{
			POIsInArea.Add(POI);
		}
	}
	
	return POIsInArea;
}

void UPOIService::SetBiomeService(UBiomeService* InBiomeService)
{
	BiomeService = InBiomeService;
	UE_LOG(LogPOIService, Log, TEXT("BiomeService set for POI generation"));
}

void UPOIService::SetHeightfieldService(UHeightfieldService* InHeightfieldService)
{
	HeightfieldService = InHeightfieldService;
	UE_LOG(LogPOIService, Log, TEXT("HeightfieldService set for POI generation"));
}

void UPOIService::UpdateSamplingConfig(const FStratifiedSamplingConfig& NewConfig)
{
	SamplingConfig = NewConfig;
	UE_LOG(LogPOIService, Log, TEXT("Updated stratified sampling config: GridSize=%d, CellSize=%f"), 
		NewConfig.GridSize, NewConfig.CellSize);
}

void UPOIService::UpdateValidationSettings(const FPOIValidationSettings& NewSettings)
{
	ValidationSettings = NewSettings;
	UE_LOG(LogPOIService, Log, TEXT("Updated POI validation settings: FlatGroundRadius=%f, Tolerance=%f"), 
		NewSettings.FlatGroundCheckRadius, NewSettings.FlatGroundTolerance);
}

TArray<FVector2D> UPOIService::GenerateStratifiedSamplePoints(FTileCoord TileCoord, int32 Seed) const
{
	TArray<FVector2D> SamplePoints;
	
	FVector TileWorldPos = TileCoord.ToWorldPosition(WorldGenSettings.TileSizeMeters);
	float TileSize = WorldGenSettings.TileSizeMeters;
	float CellSize = SamplingConfig.CellSize;
	int32 GridSize = SamplingConfig.GridSize;
	
	// Generate stratified samples in 4x4 grid
	for (int32 GridX = 0; GridX < GridSize; ++GridX)
	{
		for (int32 GridY = 0; GridY < GridSize; ++GridY)
		{
			// Calculate cell bounds
			float CellStartX = TileWorldPos.X - TileSize * 0.5f + GridX * CellSize;
			float CellStartY = TileWorldPos.Y - TileSize * 0.5f + GridY * CellSize;
			
			// Generate random point within cell
			int32 CellIndex = GridY * GridSize + GridX;
			uint32 Hash = HashTilePosition(TileCoord, CellIndex, Seed);
			
			float RandomX = (Hash & 0xFFFF) / 65535.0f;
			float RandomY = ((Hash >> 16) & 0xFFFF) / 65535.0f;
			
			FVector2D SamplePoint(
				CellStartX + RandomX * CellSize,
				CellStartY + RandomY * CellSize
			);
			
			SamplePoints.Add(SamplePoint);
		}
	}
	
	return SamplePoints;
}

TArray<FPOISpawnRule> UPOIService::GetBiomePOIRules(EBiomeType BiomeType) const
{
	TArray<FPOISpawnRule> POIRules;
	
	if (!BiomeService)
	{
		return POIRules;
	}
	
	FBiomeDefinition BiomeDefinition;
	if (BiomeService->GetBiomeDefinition(BiomeType, BiomeDefinition))
	{
		POIRules = BiomeDefinition.POIRules;
	}
	
	return POIRules;
}

bool UPOIService::CheckSlopeRequirements(FVector Location, float SlopeLimit, const TArray<float>& HeightData, FTileCoord TileCoord) const
{
	FVector2D LocalPos = WorldToTileLocal(Location, TileCoord);
	float Slope = CalculateSlopeAtLocation(LocalPos, HeightData, TileCoord);
	
	return Slope <= SlopeLimit;
}

bool UPOIService::ValidateFlatGround(FVector Location, const TArray<float>& HeightData, FTileCoord TileCoord) const
{
	FVector2D LocalPos = WorldToTileLocal(Location, TileCoord);
	float CheckRadius = ValidationSettings.FlatGroundCheckRadius;
	float Tolerance = ValidationSettings.FlatGroundTolerance;
	
	// Sample heights in 3x3 grid around location
	TArray<float> HeightSamples;
	for (int32 X = -1; X <= 1; ++X)
	{
		for (int32 Y = -1; Y <= 1; ++Y)
		{
			FVector2D SamplePos = LocalPos + FVector2D(X * CheckRadius, Y * CheckRadius);
			float Height = GetHeightAtTileLocation(SamplePos, HeightData, TileCoord);
			HeightSamples.Add(Height);
		}
	}
	
	// Check height variation
	float MinHeight = FMath::Min(HeightSamples);
	float MaxHeight = FMath::Max(HeightSamples);
	float HeightVariation = MaxHeight - MinHeight;
	
	return HeightVariation <= Tolerance;
}

float UPOIService::GetHeightAtTileLocation(FVector2D LocalPosition, const TArray<float>& HeightData, FTileCoord TileCoord) const
{
	// Convert local position to heightfield indices
	float TileSize = WorldGenSettings.TileSizeMeters;
	int32 Resolution = FMath::Sqrt(static_cast<float>(HeightData.Num()));
	
	// Normalize to [0,1] range within tile
	float NormX = (LocalPosition.X + TileSize * 0.5f) / TileSize;
	float NormY = (LocalPosition.Y + TileSize * 0.5f) / TileSize;
	
	// Clamp to valid range
	NormX = FMath::Clamp(NormX, 0.0f, 1.0f);
	NormY = FMath::Clamp(NormY, 0.0f, 1.0f);
	
	// Convert to heightfield indices
	float FloatX = NormX * (Resolution - 1);
	float FloatY = NormY * (Resolution - 1);
	
	int32 X0 = FMath::FloorToInt(FloatX);
	int32 Y0 = FMath::FloorToInt(FloatY);
	int32 X1 = FMath::Min(X0 + 1, Resolution - 1);
	int32 Y1 = FMath::Min(Y0 + 1, Resolution - 1);
	
	// Bilinear interpolation
	float FracX = FloatX - X0;
	float FracY = FloatY - Y0;
	
	float H00 = HeightData[Y0 * Resolution + X0];
	float H10 = HeightData[Y0 * Resolution + X1];
	float H01 = HeightData[Y1 * Resolution + X0];
	float H11 = HeightData[Y1 * Resolution + X1];
	
	float H0 = FMath::Lerp(H00, H10, FracX);
	float H1 = FMath::Lerp(H01, H11, FracX);
	
	return FMath::Lerp(H0, H1, FracY);
}

float UPOIService::CalculateSlopeAtLocation(FVector2D LocalPosition, const TArray<float>& HeightData, FTileCoord TileCoord) const
{
	float SampleSpacing = WorldGenSettings.SampleSpacingMeters;
	
	// Sample heights at neighboring points
	float HeightCenter = GetHeightAtTileLocation(LocalPosition, HeightData, TileCoord);
	float HeightLeft = GetHeightAtTileLocation(LocalPosition + FVector2D(-SampleSpacing, 0), HeightData, TileCoord);
	float HeightRight = GetHeightAtTileLocation(LocalPosition + FVector2D(SampleSpacing, 0), HeightData, TileCoord);
	float HeightUp = GetHeightAtTileLocation(LocalPosition + FVector2D(0, SampleSpacing), HeightData, TileCoord);
	float HeightDown = GetHeightAtTileLocation(LocalPosition + FVector2D(0, -SampleSpacing), HeightData, TileCoord);
	
	// Calculate gradients
	float GradientX = (HeightRight - HeightLeft) / (2.0f * SampleSpacing);
	float GradientY = (HeightUp - HeightDown) / (2.0f * SampleSpacing);
	
	// Calculate slope angle in degrees
	float SlopeRadians = FMath::Atan(FMath::Sqrt(GradientX * GradientX + GradientY * GradientY));
	return FMath::RadiansToDegrees(SlopeRadians);
}

float UPOIService::GenerateSeededRandom(int32 Seed, FTileCoord TileCoord, int32 SampleIndex) const
{
	uint32 Hash = HashTilePosition(TileCoord, SampleIndex, Seed);
	return (Hash & 0xFFFFFF) / 16777215.0f; // 24-bit precision
}

void UPOIService::ApplyFlatteningStamp(FVector2D Center, float Radius, float Strength, TArray<float>& HeightData, FTileCoord TileCoord) const
{
	int32 Resolution = FMath::Sqrt(static_cast<float>(HeightData.Num()));
	float TileSize = WorldGenSettings.TileSizeMeters;
	float TargetHeight = GetHeightAtTileLocation(Center, HeightData, TileCoord);
	
	// Apply flattening in circular area
	for (int32 Y = 0; Y < Resolution; ++Y)
	{
		for (int32 X = 0; X < Resolution; ++X)
		{
			// Convert heightfield indices to local position
			FVector2D LocalPos(
				(X / float(Resolution - 1) - 0.5f) * TileSize,
				(Y / float(Resolution - 1) - 0.5f) * TileSize
			);
			
			float Distance = FVector2D::Distance(LocalPos, Center);
			if (Distance <= Radius)
			{
				// Calculate falloff
				float Falloff = 1.0f - (Distance / Radius);
				Falloff = FMath::SmoothStep(0.0f, 1.0f, Falloff);
				
				// Apply flattening
				int32 Index = Y * Resolution + X;
				float CurrentHeight = HeightData[Index];
				HeightData[Index] = FMath::Lerp(CurrentHeight, TargetHeight, Strength * Falloff);
			}
		}
	}
}

// Utility and persistence functions
FVector2D UPOIService::WorldToTileLocal(FVector WorldPosition, FTileCoord TileCoord) const
{
	FVector TileCenter = TileCoord.ToWorldPosition(WorldGenSettings.TileSizeMeters);
	return FVector2D(WorldPosition.X - TileCenter.X, WorldPosition.Y - TileCenter.Y);
}

FVector UPOIService::TileLocalToWorld(FVector2D LocalPosition, FTileCoord TileCoord) const
{
	FVector TileCenter = TileCoord.ToWorldPosition(WorldGenSettings.TileSizeMeters);
	return FVector(TileCenter.X + LocalPosition.X, TileCenter.Y + LocalPosition.Y, 0.0f);
}

uint32 UPOIService::HashTilePosition(FTileCoord TileCoord, int32 SampleIndex, int32 Seed) const
{
	uint32 Hash = 0;
	Hash = HashCombine(Hash, GetTypeHash(TileCoord.X));
	Hash = HashCombine(Hash, GetTypeHash(TileCoord.Y));
	Hash = HashCombine(Hash, GetTypeHash(SampleIndex));
	Hash = HashCombine(Hash, GetTypeHash(Seed));
	return Hash;
}

void UPOIService::UpdatePerformanceStats(float GenerationTimeMs)
{
	GenerationTimes.Add(GenerationTimeMs);
	TotalGenerationTime += GenerationTimeMs;
	GenerationCount++;
	
	// Keep only last 100 samples for rolling average
	if (GenerationTimes.Num() > 100)
	{
		TotalGenerationTime -= GenerationTimes[0];
		GenerationTimes.RemoveAt(0);
	}
}

void UPOIService::GetPerformanceStats(float& OutAverageGenerationTimeMs, int32& OutTotalPOIs)
{
	OutAverageGenerationTimeMs = GenerationCount > 0 ? TotalGenerationTime / GenerationCount : 0.0f;
	OutTotalPOIs = AllPOIs.Num();
}

TArray<FPOIData> UPOIService::GetTilePOIs(FTileCoord TileCoord) const
{
	if (const FPOITileData* TileData = TilePOIs.Find(TileCoord))
	{
		return TileData->POIs;
	}
	return TArray<FPOIData>();
}

bool UPOIService::RemovePOI(const FGuid& POIId)
{
	if (FPOIData* POI = AllPOIs.Find(POIId))
	{
		// Find and remove from tile cache
		FTileCoord TileCoord = FTileCoord::FromWorldPosition(POI->Location, WorldGenSettings.TileSizeMeters);
		if (FPOITileData* TileData = TilePOIs.Find(TileCoord))
		{
			TileData->POIs.RemoveAll([POIId](const FPOIData& POI) { return POI.POIId == POIId; });
		}
		
		AllPOIs.Remove(POIId);
		UE_LOG(LogPOIService, Log, TEXT("Removed POI %s"), *POIId.ToString());
		return true;
	}
	return false;
}

bool UPOIService::AddCustomPOI(const FPOIData& POIData)
{
	FTileCoord TileCoord = FTileCoord::FromWorldPosition(POIData.Location, WorldGenSettings.TileSizeMeters);
	
	AllPOIs.Add(POIData.POIId, POIData);
	TilePOIs.FindOrAdd(TileCoord).POIs.Add(POIData);
	
	UE_LOG(LogPOIService, Log, TEXT("Added custom POI %s at (%f, %f)"), 
		*POIData.POIName, POIData.Location.X, POIData.Location.Y);
	return true;
}

// Persistence functions
bool UPOIService::SavePOIData()
{
	for (const auto& TilePair : TilePOIs)
	{
		FTileCoord TileCoord = TilePair.Key;
		const TArray<FPOIData>& POIs = TilePair.Value.POIs;
		
		if (POIs.Num() == 0)
		{
			continue;
		}
		
		FString FilePath = GetPOIPersistencePath(TileCoord);
		TArray<uint8> SerializedData;
		
		if (SerializePOIData(POIs, SerializedData))
		{
			if (!FFileHelper::SaveArrayToFile(SerializedData, *FilePath))
			{
				UE_LOG(LogPOIService, Error, TEXT("Failed to save POI data for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
				return false;
			}
		}
	}
	
	UE_LOG(LogPOIService, Log, TEXT("Saved POI data for %d tiles"), TilePOIs.Num());
	return true;
}

bool UPOIService::LoadPOIData()
{
	// Clear existing data
	TilePOIs.Empty();
	AllPOIs.Empty();
	
	// Load all .poi files from persistence directory
	TArray<FString> POIFiles;
	IFileManager::Get().FindFiles(POIFiles, *(PersistenceDirectory / TEXT("*.poi")), true, false);
	
	for (const FString& FileName : POIFiles)
	{
		FString FilePath = PersistenceDirectory / FileName;
		TArray<uint8> FileData;
		
		if (FFileHelper::LoadFileToArray(FileData, *FilePath))
		{
			TArray<FPOIData> LoadedPOIs;
			if (DeserializePOIData(FileData, LoadedPOIs))
			{
				// Extract tile coordinate from filename
				FString BaseName = FPaths::GetBaseFilename(FileName);
				TArray<FString> Coords;
				BaseName.ParseIntoArray(Coords, TEXT("_"));
				
				if (Coords.Num() >= 2)
				{
					FTileCoord TileCoord(FCString::Atoi(*Coords[0]), FCString::Atoi(*Coords[1]));
					TilePOIs.Add(TileCoord, FPOITileData(LoadedPOIs));
					
					// Add to global POI map
					for (const FPOIData& POI : LoadedPOIs)
					{
						AllPOIs.Add(POI.POIId, POI);
					}
				}
			}
		}
	}
	
	UE_LOG(LogPOIService, Log, TEXT("Loaded POI data for %d tiles, %d total POIs"), TilePOIs.Num(), AllPOIs.Num());
	return true;
}

FString UPOIService::GetPOIPersistencePath(FTileCoord TileCoord) const
{
	return PersistenceDirectory / FString::Printf(TEXT("%d_%d.poi"), TileCoord.X, TileCoord.Y);
}

bool UPOIService::SerializePOIData(const TArray<FPOIData>& POIs, TArray<uint8>& OutData) const
{
	FMemoryWriter Writer(OutData);
	
	int32 POICount = POIs.Num();
	Writer << POICount;
	
	for (const FPOIData& POI : POIs)
	{
		FPOIData MutablePOI = POI;
		MutablePOI.Serialize(Writer);
	}
	
	return true;
}

bool UPOIService::DeserializePOIData(const TArray<uint8>& InData, TArray<FPOIData>& OutPOIs) const
{
	FMemoryReader Reader(InData);
	
	int32 POICount = 0;
	Reader << POICount;
	
	OutPOIs.Reserve(POICount);
	
	for (int32 i = 0; i < POICount; ++i)
	{
		FPOIData POI;
		POI.Serialize(Reader);
		OutPOIs.Add(POI);
	}
	
	return true;
}