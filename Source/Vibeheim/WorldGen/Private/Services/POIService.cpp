#include "Services/POIService.h"
#include "Services/BiomeService.h"
#include "Services/HeightfieldService.h"
#include "Engine/World.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "Data/SerializationShims.h"

DEFINE_LOG_CATEGORY_STATIC(LogPOIService, Log, All);

namespace
{
	static constexpr uint32 POIPersistenceMagic = 0x504F4944; // 'POID'
	static constexpr int32 POIPersistenceVersion = 2;
}

UPOIService::UPOIService()
{
	BiomeService = nullptr;
	HeightfieldService = nullptr;
	TotalGenerationTime = 0.0f;
	GenerationCount = 0;
	ReservationCellSizeMeters = 256.0f;
	
	// Initialize default settings
	SamplingConfig.GridSize = 4;
	SamplingConfig.CellSize = 16.0f;
	SamplingConfig.MaxAttemptsPerCell = 3;
	SamplingConfig.MinCellSpacing = 8.0f;
	
	ValidationSettings.FlatGroundCheckRadius = 2.0f;
	ValidationSettings.FlatGroundTolerance = 2.5f;
	ValidationSettings.TerrainStampRadius = 5.0f;
	ValidationSettings.TerrainStampStrength = 0.8f;
}

bool UPOIService::Initialize(const FWorldGenConfig& Settings)
{
	WorldGenSettings = Settings;
	PersistenceDirectory = FPaths::ProjectSavedDir() / TEXT("WorldGen") / TEXT("POI");
	ActiveReservations.Empty();
	ReservationSpatialIndex.Empty();
	RemovedPOIs.Empty();
	ReservationCellSizeMeters = FMath::Max(WorldGenSettings.TileSizeMeters * 0.5f, 64.0f);
	
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
	TSet<FString> NewlySpawnedUniqueNames;
	
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

	// Determine minimal spacing used for blue-noise sampling within the tile
	float BaseSpacingMeters = TNumericLimits<float>::Max();
	for (const FPOISpawnRule& Rule : POIRules)
	{
		BaseSpacingMeters = FMath::Min(BaseSpacingMeters, GetReservationRadiusMeters(Rule));
	}
	if (!FMath::IsFinite(BaseSpacingMeters) || BaseSpacingMeters <= KINDA_SMALL_NUMBER)
	{
		BaseSpacingMeters = FMath::Max(SamplingConfig.MinCellSpacing, 4.0f);
	}
	else
	{
		BaseSpacingMeters = FMath::Max(BaseSpacingMeters, SamplingConfig.MinCellSpacing);
	}
	
	// Generate stratified sampling points
	TArray<FVector2D> SamplePoints = GenerateBlueNoiseSamples(TileCoord, BaseSpacingMeters, WorldGenSettings.Seed);
	if (SamplePoints.Num() == 0)
	{
		// Fall back to stratified sampling if blue-noise generation failed
		SamplePoints = GenerateStratifiedSamplePoints(TileCoord, WorldGenSettings.Seed);
	}
	
	// Attempt to place POIs at sample points
	for (int32 i = 0; i < SamplePoints.Num(); ++i)
	{
		FVector2D SamplePoint = SamplePoints[i];
		FVector WorldLocation = FVector(SamplePoint.X, SamplePoint.Y, 0.0f);
		
		// Try each POI rule for this sample point
		for (const FPOISpawnRule& Rule : POIRules)
		{
			// Enforce uniqueness constraints before doing expensive checks
			if (Rule.bUniquePerWorld)
			{
				if (NewlySpawnedUniqueNames.Contains(Rule.POIName) || HasExistingPOIByName(Rule.POIName))
				{
					continue;
				}
			}

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
			
			// Enforce world-level reservation rules (blue-noise spacing)
			if (!CheckPOIDistanceRequirements(WorldLocation, Rule, GeneratedPOIs))
			{
				continue;
			}

			// Check distance requirements
			float ReservationRadius = GetReservationRadiusMeters(Rule);
			// Create POI data
			FPOIData NewPOI;
			NewPOI.POIName = Rule.POIName;
			NewPOI.Location = WorldLocation;
			NewPOI.Location.Z = GetHeightAtTileLocation(WorldToTileLocal(WorldLocation, TileCoord), HeightData, TileCoord);
			NewPOI.POIBlueprint = Rule.POIBlueprint;
			NewPOI.OriginBiome = BiomeType;
			
			GeneratedPOIs.Add(NewPOI);
			AllPOIs.Add(NewPOI.POIId, NewPOI);
			RemovedPOIs.Remove(NewPOI.POIId);
			ReserveLocationForPOI(NewPOI, ReservationRadius, Rule);
			if (Rule.bUniquePerWorld)
			{
				NewlySpawnedUniqueNames.Add(Rule.POIName);
			}
			
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
	const float MinDistance = FMath::Max(Rule.MinDistanceFromOthers, 0.0f);

	for (const FPOIData& ExistingPOI : ExistingPOIs)
	{
		float Distance = FVector::Dist2D(Location, ExistingPOI.Location);
		if (Distance < MinDistance)
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
					if (Distance < MinDistance)
					{
						return false;
					}
				}
			}
		}
	}

	// Check against world-level reservations for global spacing
	FGuid ConflictId;
	if (!CanReserveLocation(Location, GetReservationRadiusMeters(Rule), Rule, FGuid(), ConflictId))
	{
		return false;
	}

	return true;
}

bool UPOIService::ApplyTerrainStamp(FVector Location, float Radius, TArray<float>& HeightData, FTileCoord TileCoord, const FPOITerrainStampSettings& StampSettings)
{
	const float EffectiveRadius = (StampSettings.RadiusMeters > KINDA_SMALL_NUMBER) ? StampSettings.RadiusMeters : Radius;
	if (EffectiveRadius <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogPOIService, Warning, TEXT("ApplyTerrainStamp skipped due to non-positive radius (%.2f)"), EffectiveRadius);
		return false;
	}

	// Determine operation and strength defaults
	EPOITerrainStampMode Operation = StampSettings.Operation == EPOITerrainStampMode::None
		? EPOITerrainStampMode::Flatten
		: StampSettings.Operation;

	float Strength = StampSettings.Strength;
	if (Strength <= KINDA_SMALL_NUMBER)
	{
		Strength = ValidationSettings.TerrainStampStrength;
	}
	Strength = FMath::Clamp(Strength, 0.0f, 1.0f);

	FVector2D LocalPos = WorldToTileLocal(Location, TileCoord);

	switch (Operation)
	{
	case EPOITerrainStampMode::Flatten:
		ApplyFlatteningStamp(LocalPos, EffectiveRadius, Strength, HeightData, TileCoord);
		break;
	case EPOITerrainStampMode::Raise:
		ApplyRaiseStamp(LocalPos, EffectiveRadius, Strength, StampSettings.RaiseHeightMeters, HeightData, TileCoord);
		break;
	case EPOITerrainStampMode::Smooth:
		ApplySmoothStamp(LocalPos, EffectiveRadius, FMath::Max(StampSettings.SmoothIterations, 1), HeightData, TileCoord);
		break;
	default:
		UE_LOG(LogPOIService, Warning, TEXT("ApplyTerrainStamp received unsupported operation (%d)"), static_cast<int32>(Operation));
		return false;
	}

	// Record modification with heightfield service to persist across tiles/world loads
	if (HeightfieldService)
	{
		EHeightfieldOperation HeightOp = EHeightfieldOperation::Flatten;
		float PersistenceStrength = Strength;

		switch (Operation)
		{
		case EPOITerrainStampMode::Flatten:
			HeightOp = EHeightfieldOperation::Flatten;
			break;
		case EPOITerrainStampMode::Raise:
			HeightOp = EHeightfieldOperation::Add;
			PersistenceStrength = StampSettings.RaiseHeightMeters * Strength;
			break;
		case EPOITerrainStampMode::Smooth:
			HeightOp = EHeightfieldOperation::Smooth;
			break;
		default:
			break;
		}

		if (PersistenceStrength > KINDA_SMALL_NUMBER)
		{
			HeightfieldService->ModifyHeightfield(Location, EffectiveRadius, PersistenceStrength, HeightOp);
		}
	}

	UE_LOG(LogPOIService, Verbose, TEXT("Applied %s terrain stamp at (%.1f, %.1f, %.1f) with radius %.1f (strength %.2f)"),
		*UEnum::GetDisplayValueAsText(Operation).ToString(), Location.X, Location.Y, Location.Z, EffectiveRadius, Strength);

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

TArray<FVector2D> UPOIService::GenerateBlueNoiseSamples(FTileCoord TileCoord, float MinDistanceMeters, int32 Seed) const
{
	TArray<FVector2D> SamplePoints;

	const float TileSize = WorldGenSettings.TileSizeMeters;
	const FVector TileWorldPos = TileCoord.ToWorldPosition(TileSize);
	const float HalfSize = TileSize * 0.5f;

	// Seed deterministic random stream per tile
	uint32 SeedHash = HashTilePosition(TileCoord, 0, Seed);
	FRandomStream Random(SeedHash);

	const int32 DesiredSamples = FMath::Max(SamplingConfig.GridSize * SamplingConfig.GridSize, 4);
	const int32 MaxAttempts = DesiredSamples * SamplingConfig.MaxAttemptsPerCell * 4;
	const float MinDistanceSq = FMath::Square(FMath::Max(MinDistanceMeters, 1.0f));

	int32 Attempts = 0;
	while (Attempts < MaxAttempts && SamplePoints.Num() < DesiredSamples)
	{
		Attempts++;

		float OffsetX = Random.FRandRange(-HalfSize, HalfSize);
		float OffsetY = Random.FRandRange(-HalfSize, HalfSize);
		FVector2D Candidate(TileWorldPos.X + OffsetX, TileWorldPos.Y + OffsetY);

		bool bValid = true;
		for (const FVector2D& Existing : SamplePoints)
		{
			if (FVector2D::DistSquared(Candidate, Existing) < MinDistanceSq)
			{
				bValid = false;
				break;
			}
		}

		if (bValid)
		{
			SamplePoints.Add(Candidate);
		}
	}

	// Ensure deterministic ordering for downstream consumers
	if (SamplePoints.Num() > 1)
	{
		SamplePoints.Sort([](const FVector2D& A, const FVector2D& B)
		{
			if (A.X == B.X)
			{
				return A.Y < B.Y;
			}
			return A.X < B.X;
		});
	}

	return SamplePoints;
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
	FVector2D LocationXY(Location.X, Location.Y);
	float CheckRadius = ValidationSettings.FlatGroundCheckRadius;
	float BaseTolerance = ValidationSettings.FlatGroundTolerance;
	
	// Use terrain height as baseline, not Location.Z parameter
	float CenterH = SampleHeightAt(LocationXY, HeightData, TileCoord);
	
	// Sample neighborhood heights and compare against terrain baseline
	float MinH = CenterH;
	float MaxH = CenterH;
	
	for (int32 X = -1; X <= 1; ++X)
	{
		for (int32 Y = -1; Y <= 1; ++Y)
		{
			FVector2D NeighborXY = LocationXY + FVector2D(X * CheckRadius, Y * CheckRadius);
			float H = SampleHeightAt(NeighborXY, HeightData, TileCoord);
			MinH = FMath::Min(MinH, H);
			MaxH = FMath::Max(MaxH, H);
		}
	}
	
	// Calculate height range (all heights from terrain, no subtraction from Location.Z)
	float Range = MaxH - MinH;
	
	// Calculate slope-aware tolerance
	float LocalSlope = CalculateSlopeAtLocation(WorldToTileLocal(Location, TileCoord), HeightData, TileCoord);
	float SlopeLimit = 30.0f; // Default slope limit for POI placement
	float ExpectedDelta = FMath::Tan(FMath::DegreesToRadians(SlopeLimit)) * (CheckRadius * 2.0f);
	float SlopeAwareTolerance = FMath::Max(BaseTolerance, ExpectedDelta * 0.5f);
	
	bool bIsFlat = Range <= SlopeAwareTolerance;
	
	// Add diagnostic logging with both baselines for debugging
	UE_LOG(LogPOIService, Warning, TEXT("CenterH=%.2f, LocationZ=%.2f, Range=%.2f, Tol=%.2f, Result=%s"),
		CenterH, Location.Z, Range, SlopeAwareTolerance, bIsFlat ? TEXT("PASS") : TEXT("FAIL"));
	
	return bIsFlat;
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

void UPOIService::ApplyRaiseStamp(FVector2D Center, float Radius, float Strength, float RaiseHeightMeters, TArray<float>& HeightData, FTileCoord TileCoord) const
{
	if (RaiseHeightMeters == 0.0f || Strength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const int32 Resolution = FMath::Sqrt(static_cast<float>(HeightData.Num()));
	const float TileSize = WorldGenSettings.TileSizeMeters;
	const float TargetHeight = GetHeightAtTileLocation(Center, HeightData, TileCoord) + RaiseHeightMeters;

	for (int32 Y = 0; Y < Resolution; ++Y)
	{
		for (int32 X = 0; X < Resolution; ++X)
		{
			FVector2D LocalPos(
				(X / float(Resolution - 1) - 0.5f) * TileSize,
				(Y / float(Resolution - 1) - 0.5f) * TileSize
			);

			float Distance = FVector2D::Distance(LocalPos, Center);
			if (Distance > Radius)
			{
				continue;
			}

			float Falloff = 1.0f - (Distance / Radius);
			Falloff = FMath::SmoothStep(0.0f, 1.0f, Falloff);

			const int32 Index = Y * Resolution + X;
			const float CurrentHeight = HeightData[Index];
			HeightData[Index] = FMath::Lerp(CurrentHeight, TargetHeight, Strength * Falloff);
		}
	}
}

void UPOIService::ApplySmoothStamp(FVector2D Center, float Radius, int32 Iterations, TArray<float>& HeightData, FTileCoord TileCoord) const
{
	if (Iterations <= 0)
	{
		return;
	}

	const int32 Resolution = FMath::Sqrt(static_cast<float>(HeightData.Num()));
	const float TileSize = WorldGenSettings.TileSizeMeters;

	TArray<float> WorkingHeights = HeightData;
	TArray<float> TempHeights = HeightData;

	for (int32 Iteration = 0; Iteration < Iterations; ++Iteration)
	{
		TempHeights = WorkingHeights;

		for (int32 Y = 0; Y < Resolution; ++Y)
		{
			for (int32 X = 0; X < Resolution; ++X)
			{
				FVector2D LocalPos(
					(X / float(Resolution - 1) - 0.5f) * TileSize,
					(Y / float(Resolution - 1) - 0.5f) * TileSize
				);

				float Distance = FVector2D::Distance(LocalPos, Center);
				if (Distance > Radius)
				{
					continue;
				}

				float Falloff = 1.0f - (Distance / Radius);
				Falloff = FMath::SmoothStep(0.0f, 1.0f, Falloff);

				float Accumulator = 0.0f;
				int32 NeighborCount = 0;

				for (int32 DY = -1; DY <= 1; ++DY)
				{
					for (int32 DX = -1; DX <= 1; ++DX)
					{
						const int32 NX = X + DX;
						const int32 NY = Y + DY;
						if (NX < 0 || NX >= Resolution || NY < 0 || NY >= Resolution)
						{
							continue;
						}

						const int32 NeighborIndex = NY * Resolution + NX;
						Accumulator += WorkingHeights[NeighborIndex];
						NeighborCount++;
					}
				}

				if (NeighborCount > 0)
				{
					const float Average = Accumulator / NeighborCount;
					const int32 Index = Y * Resolution + X;
					const float CurrentHeight = WorkingHeights[Index];
					TempHeights[Index] = FMath::Lerp(CurrentHeight, Average, Falloff * 0.5f);
				}
			}
		}

		WorkingHeights = TempHeights;
	}

	// Commit results back to height data
	for (int32 Y = 0; Y < Resolution; ++Y)
	{
		for (int32 X = 0; X < Resolution; ++X)
		{
			FVector2D LocalPos(
				(X / float(Resolution - 1) - 0.5f) * TileSize,
				(Y / float(Resolution - 1) - 0.5f) * TileSize
			);

			if (FVector2D::Distance(LocalPos, Center) <= Radius)
			{
				const int32 Index = Y * Resolution + X;
				HeightData[Index] = WorkingHeights[Index];
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

float UPOIService::GetReservationRadiusMeters(const FPOISpawnRule& Rule) const
{
	float Radius = Rule.GlobalSpacingOverride > KINDA_SMALL_NUMBER ? Rule.GlobalSpacingOverride : Rule.MinDistanceFromOthers;
	if (Radius <= KINDA_SMALL_NUMBER)
	{
		Radius = SamplingConfig.MinCellSpacing;
	}
	return FMath::Max(Radius, SamplingConfig.MinCellSpacing);
}

bool UPOIService::CanReserveLocation(const FVector& WorldLocation, float RadiusMeters, const FPOISpawnRule& Rule, FGuid IgnoreId, FGuid& OutConflictId) const
{
	OutConflictId.Invalidate();

	if (RadiusMeters <= 0.0f)
	{
		return true;
	}

	TArray<int64> CellsToCheck;
	GatherReservationCells(WorldLocation, RadiusMeters, CellsToCheck);

	for (int64 CellKey : CellsToCheck)
	{
		TArray<FGuid> CandidateIds;
		ReservationSpatialIndex.MultiFind(CellKey, CandidateIds);

		for (const FGuid& CandidateId : CandidateIds)
		{
			if (IgnoreId.IsValid() && CandidateId == IgnoreId)
			{
				continue;
			}

			const FPOIReservation* Reservation = ActiveReservations.Find(CandidateId);
			if (!Reservation)
			{
				continue;
			}

			// Enforce unique-per-world rules
			if ((Rule.bUniquePerWorld || Reservation->bUniqueRule) && Reservation->RuleName.Equals(Rule.POIName))
			{
				OutConflictId = Reservation->POIId;
				return false;
			}

			const float CombinedRadius = RadiusMeters + Reservation->RadiusMeters;
			const float Distance = FVector::Dist2D(WorldLocation, Reservation->Location);
			if (Distance < CombinedRadius)
			{
				OutConflictId = Reservation->POIId;
				return false;
			}
		}
	}

	return true;
}

void UPOIService::ReserveLocationForPOI(const FPOIData& POIData, float RadiusMeters, const FPOISpawnRule& Rule)
{
	if (!POIData.POIId.IsValid())
	{
		return;
	}

	// Remove any stale reservation before re-adding
	ReleaseReservation(POIData.POIId);

	FPOIReservation Reservation;
	Reservation.POIId = POIData.POIId;
	Reservation.Location = POIData.Location;
	Reservation.RadiusMeters = RadiusMeters;
	Reservation.RuleName = Rule.POIName;
	Reservation.OwningTile = FTileCoord::FromWorldPosition(POIData.Location, WorldGenSettings.TileSizeMeters);
	Reservation.bUniqueRule = Rule.bUniquePerWorld;

	ActiveReservations.Add(POIData.POIId, Reservation);

	TArray<int64> Cells;
	GatherReservationCells(POIData.Location, RadiusMeters, Cells);
	for (int64 CellKey : Cells)
	{
		ReservationSpatialIndex.Add(CellKey, POIData.POIId);
	}
}

void UPOIService::ReleaseReservation(const FGuid& POIId)
{
	FPOIReservation Reservation;
	if (!ActiveReservations.RemoveAndCopyValue(POIId, Reservation))
	{
		return;
	}

	TArray<int64> Cells;
	GatherReservationCells(Reservation.Location, Reservation.RadiusMeters, Cells);
	for (int64 CellKey : Cells)
	{
		ReservationSpatialIndex.RemoveSingle(CellKey, POIId);
	}
}

int64 UPOIService::MakeReservationCellKey(int32 CellX, int32 CellY) const
{
	const int64 KeyX = static_cast<int64>(CellX) & 0x00000000FFFFFFFFLL;
	const int64 KeyY = static_cast<int64>(CellY) & 0x00000000FFFFFFFFLL;
	return (KeyX << 32) | KeyY;
}

FIntPoint UPOIService::GetReservationCell(const FVector& WorldLocation, float CellSizeMeters) const
{
	const float SafeCell = FMath::Max(CellSizeMeters, 1.0f);
	const int32 CellX = FMath::FloorToInt(WorldLocation.X / SafeCell);
	const int32 CellY = FMath::FloorToInt(WorldLocation.Y / SafeCell);
	return FIntPoint(CellX, CellY);
}

void UPOIService::GatherReservationCells(const FVector& WorldLocation, float RadiusMeters, TArray<int64>& OutCellKeys) const
{
	OutCellKeys.Reset();

	const float CellSize = FMath::Max(ReservationCellSizeMeters, 1.0f);
	const FIntPoint CenterCell = GetReservationCell(WorldLocation, CellSize);
	const int32 Range = FMath::Max(1, FMath::CeilToInt((RadiusMeters / CellSize) + 1.0f));

	for (int32 DY = -Range; DY <= Range; ++DY)
	{
		for (int32 DX = -Range; DX <= Range; ++DX)
		{
			const int32 CellX = CenterCell.X + DX;
			const int32 CellY = CenterCell.Y + DY;

			const int64 Key = MakeReservationCellKey(CellX, CellY);
			OutCellKeys.AddUnique(Key);
		}
	}
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
		ReleaseReservation(POIId);
		RemovedPOIs.Add(POIId, *POI);

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
	if (!POIData.POIId.IsValid())
	{
		return false;
	}

	FPOISpawnRule TempRule;
	TempRule.POIName = POIData.POIName;
	TempRule.MinDistanceFromOthers = FMath::Max(SamplingConfig.MinCellSpacing, 1.0f);
	TempRule.GlobalSpacingOverride = TempRule.MinDistanceFromOthers;
	TempRule.bUniquePerWorld = false;

	const float ReservationRadius = GetReservationRadiusMeters(TempRule);
	FGuid ConflictId;
	if (!CanReserveLocation(POIData.Location, ReservationRadius, TempRule, FGuid(), ConflictId))
	{
		UE_LOG(LogPOIService, Warning, TEXT("Failed to add custom POI '%s' due to reservation conflict with %s"),
			*POIData.POIName, ConflictId.IsValid() ? *ConflictId.ToString() : TEXT("unknown"));
		return false;
	}

	FTileCoord TileCoord = FTileCoord::FromWorldPosition(POIData.Location, WorldGenSettings.TileSizeMeters);
	
	AllPOIs.Add(POIData.POIId, POIData);
	TilePOIs.FindOrAdd(TileCoord).POIs.Add(POIData);
	RemovedPOIs.Remove(POIData.POIId);
	ReserveLocationForPOI(POIData, ReservationRadius, TempRule);
	
	UE_LOG(LogPOIService, Log, TEXT("Added custom POI %s at (%f, %f)"), 
		*POIData.POIName, POIData.Location.X, POIData.Location.Y);
	return true;
}

FString UPOIService::GetPOIPersistencePath(FTileCoord TileCoord) const
{
	return PersistenceDirectory / FString::Printf(TEXT("%d_%d.poi"), TileCoord.X, TileCoord.Y);
}

bool UPOIService::SavePOIData()
{
	TMap<FTileCoord, TArray<FPOIData>> RemovedByTile;
	for (const TPair<FGuid, FPOIData>& RemovedPair : RemovedPOIs)
	{
		FTileCoord TileCoord = FTileCoord::FromWorldPosition(RemovedPair.Value.Location, WorldGenSettings.TileSizeMeters);
		RemovedByTile.FindOrAdd(TileCoord).Add(RemovedPair.Value);
	}

	TSet<FTileCoord> TilesToPersist;
	for (const TPair<FTileCoord, FPOITileData>& TilePair : TilePOIs)
	{
		TilesToPersist.Add(TilePair.Key);
	}
	for (const TPair<FTileCoord, TArray<FPOIData>>& RemovedPair : RemovedByTile)
	{
		TilesToPersist.Add(RemovedPair.Key);
	}

	IFileManager& FileManager = IFileManager::Get();
	const TArray<FPOIData> EmptyArray;
	int32 SavedTileCount = 0;

	for (const FTileCoord& TileCoord : TilesToPersist)
	{
		const TArray<FPOIData>* ActivePtr = nullptr;
		if (const FPOITileData* TileData = TilePOIs.Find(TileCoord))
		{
			ActivePtr = &TileData->POIs;
		}

		const TArray<FPOIData>* RemovedPtr = RemovedByTile.Find(TileCoord);
		const bool bHasActive = ActivePtr && ActivePtr->Num() > 0;
		const bool bHasRemoved = RemovedPtr && RemovedPtr->Num() > 0;

		const FString FilePath = GetPOIPersistencePath(TileCoord);

		if (!bHasActive && !bHasRemoved)
		{
			if (FileManager.FileExists(*FilePath))
			{
				FileManager.Delete(*FilePath);
			}
			continue;
		}

		TArray<uint8> SerializedData;
		const TArray<FPOIData>& ActiveArray = bHasActive ? *ActivePtr : EmptyArray;
		const TArray<FPOIData>& RemovedArray = bHasRemoved ? *RemovedPtr : EmptyArray;

		if (!SerializePOIData(ActiveArray, RemovedArray, SerializedData))
		{
			UE_LOG(LogPOIService, Error, TEXT("Failed to serialize POI data for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
			return false;
		}

		if (!FFileHelper::SaveArrayToFile(SerializedData, *FilePath))
		{
			UE_LOG(LogPOIService, Error, TEXT("Failed to save POI data for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
			return false;
		}

		++SavedTileCount;
	}

	UE_LOG(LogPOIService, Log, TEXT("Saved POI data for %d tiles (active=%d removed=%d)"),
		SavedTileCount, AllPOIs.Num(), RemovedPOIs.Num());

	return true;
}

bool UPOIService::LoadPOIData()
{
	TilePOIs.Empty();
	AllPOIs.Empty();
	RemovedPOIs.Empty();
	ActiveReservations.Empty();
	ReservationSpatialIndex.Empty();

	TArray<FString> POIFiles;
	IFileManager::Get().FindFiles(POIFiles, *(PersistenceDirectory / TEXT("*.poi")), true, false);

	int32 LoadedTiles = 0;

	for (const FString& FileName : POIFiles)
	{
		const FString FilePath = PersistenceDirectory / FileName;
		TArray<uint8> FileData;

		if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
		{
			UE_LOG(LogPOIService, Warning, TEXT("Failed to read POI file %s"), *FilePath);
			continue;
		}

		TArray<FPOIData> ActivePOIs;
		TArray<FPOIData> RemovedTilePOIs;
		int32 FileVersion = 0;

		if (!DeserializePOIData(FileData, ActivePOIs, RemovedTilePOIs, FileVersion))
		{
			UE_LOG(LogPOIService, Warning, TEXT("Failed to deserialize POI file %s"), *FilePath);
			continue;
		}

		FString BaseName = FPaths::GetBaseFilename(FileName);
		TArray<FString> Coords;
		BaseName.ParseIntoArray(Coords, TEXT("_"));
		if (Coords.Num() < 2)
		{
			UE_LOG(LogPOIService, Warning, TEXT("Invalid POI filename format: %s"), *FileName);
			continue;
		}

		FTileCoord TileCoord(FCString::Atoi(*Coords[0]), FCString::Atoi(*Coords[1]));
		FPOITileData& TileData = TilePOIs.FindOrAdd(TileCoord);
		TileData.POIs = ActivePOIs;

		const bool bLegacyData = FileVersion < POIPersistenceVersion;

		for (FPOIData& POI : TileData.POIs)
		{
			FPOIData& StoredPOI = AllPOIs.FindOrAdd(POI.POIId, POI);
			RemovedPOIs.Remove(POI.POIId);

			TOptional<FPOISpawnRule> Rule = ResolveSpawnRuleForPOI(StoredPOI);
			FPOISpawnRule ReservationRule;
			if (Rule.IsSet())
			{
				ReservationRule = Rule.GetValue();
			}
			else
			{
				ReservationRule.POIName = StoredPOI.POIName;
				ReservationRule.MinDistanceFromOthers = FMath::Max(SamplingConfig.MinCellSpacing, 1.0f);
				ReservationRule.GlobalSpacingOverride = ReservationRule.MinDistanceFromOthers;
				ReservationRule.bUniquePerWorld = false;
			}

			ReconcileLoadedPOI(StoredPOI, Rule, bLegacyData, TileCoord);

			const float ReservationRadius = GetReservationRadiusMeters(ReservationRule);
			ReserveLocationForPOI(StoredPOI, ReservationRadius, ReservationRule);

			// Keep tile data in sync with reconciled POI
			POI = StoredPOI;
		}

		for (const FPOIData& RemovedPOI : RemovedTilePOIs)
		{
			RemovedPOIs.Add(RemovedPOI.POIId, RemovedPOI);
			ReleaseReservation(RemovedPOI.POIId);
		}

		++LoadedTiles;
	}

	UE_LOG(LogPOIService, Log, TEXT("Loaded POI data for %d tiles, %d total POIs (removed=%d)"),
		LoadedTiles, AllPOIs.Num(), RemovedPOIs.Num());

	return true;
}

bool UPOIService::SerializePOIData(const TArray<FPOIData>& ActivePOIs, const TArray<FPOIData>& RemovedPOIsForTile, TArray<uint8>& OutData) const
{
	FMemoryWriter Writer(OutData);

	uint32 Magic = POIPersistenceMagic;
	int32 Version = POIPersistenceVersion;
	int32 ActiveCount = ActivePOIs.Num();
	int32 RemovedCount = RemovedPOIsForTile.Num();

	Writer << Magic;
	Writer << Version;
	Writer << ActiveCount;

	for (const FPOIData& POI : ActivePOIs)
	{
		FPOIData MutablePOI = POI;
		MutablePOI.Serialize(Writer);
	}

	Writer << RemovedCount;
	for (const FPOIData& Removed : RemovedPOIsForTile)
	{
		FPOIData MutableRemoved = Removed;
		MutableRemoved.Serialize(Writer);
	}

	return true;
}

bool UPOIService::DeserializePOIData(const TArray<uint8>& InData, TArray<FPOIData>& OutActivePOIs, TArray<FPOIData>& OutRemovedPOIs, int32& OutVersion) const
{
	FMemoryReader Reader(InData);

	OutVersion = 0;
	OutActivePOIs.Reset();
	OutRemovedPOIs.Reset();

	if (!Reader.TotalSize())
	{
		return false;
	}

	uint32 Magic = 0;
	Reader << Magic;

	if (Magic == POIPersistenceMagic)
	{
		Reader << OutVersion;

		if (OutVersion > POIPersistenceVersion)
		{
			UE_LOG(LogPOIService, Warning, TEXT("POI data version %d is newer than supported version %d"), OutVersion, POIPersistenceVersion);
		}

		int32 ActiveCount = 0;
		Reader << ActiveCount;
		OutActivePOIs.Reserve(ActiveCount);
		for (int32 Index = 0; Index < ActiveCount; ++Index)
		{
			FPOIData POI;
			POI.Serialize(Reader);
			OutActivePOIs.Add(POI);
		}

		int32 RemovedCount = 0;
		Reader << RemovedCount;
		OutRemovedPOIs.Reserve(RemovedCount);
		for (int32 Index = 0; Index < RemovedCount; ++Index)
		{
			FPOIData RemovedPOI;
			RemovedPOI.Serialize(Reader);
			OutRemovedPOIs.Add(RemovedPOI);
		}
	}
	else
	{
		// Legacy format: first int is count, no header or removed entries
		Reader.Seek(0);
		int32 ActiveCount = 0;
		Reader << ActiveCount;
		OutActivePOIs.Reserve(ActiveCount);
		for (int32 Index = 0; Index < ActiveCount; ++Index)
		{
			FPOIData POI;
			POI.Serialize(Reader);
			OutActivePOIs.Add(POI);
		}

		OutRemovedPOIs.Reset();
		OutVersion = 0;
	}

	return true;
}

float UPOIService::SampleHeightAt(FVector2D WorldXY, const TArray<float>& HeightData, FTileCoord TileCoord) const
{
	// Consistent coordinate conversion (cm → sample index)
	const float SampleSpacing = WorldGenSettings.SampleSpacingMeters * 100.0f; // Convert to cm
	const float TileSize = WorldGenSettings.TileSizeMeters * 100.0f; // Convert to cm
	const int32 GridSize = FMath::Sqrt(static_cast<float>(HeightData.Num()));
	
	// Calculate tile origin in world coordinates (cm)
	FVector TileWorldPos = TileCoord.ToWorldPosition(WorldGenSettings.TileSizeMeters);
	FVector2D TileOrigin(TileWorldPos.X * 100.0f - TileSize * 0.5f, TileWorldPos.Y * 100.0f - TileSize * 0.5f);
	
	// Convert world position to local tile coordinates
	FVector2D LocalPos = (WorldXY * 100.0f - TileOrigin) / SampleSpacing;
	
	// Clamp to valid sample range
	float Fx = FMath::Clamp(LocalPos.X, 0.0f, GridSize - 1.0f);
	float Fy = FMath::Clamp(LocalPos.Y, 0.0f, GridSize - 1.0f);
	
	// Get integer indices
	int32 Ix = FMath::FloorToInt(Fx);
	int32 Iy = FMath::FloorToInt(Fy);
	
	// Bilinear interpolation for smoother sampling
	if (Ix < GridSize - 1 && Iy < GridSize - 1)
	{
		float FracX = Fx - Ix;
		float FracY = Fy - Iy;
		
		float H00 = HeightData[Iy * GridSize + Ix];
		float H10 = HeightData[Iy * GridSize + (Ix + 1)];
		float H01 = HeightData[(Iy + 1) * GridSize + Ix];
		float H11 = HeightData[(Iy + 1) * GridSize + (Ix + 1)];
		
		float H0 = FMath::Lerp(H00, H10, FracX);
		float H1 = FMath::Lerp(H01, H11, FracX);
		
		return FMath::Lerp(H0, H1, FracY);
	}
	else
	{
		// Edge case: use nearest neighbor
		int32 Index = Iy * GridSize + Ix;
		return HeightData.IsValidIndex(Index) ? HeightData[Index] : 0.0f;
	}
}

bool UPOIService::ValidatePlacementConstraints(FVector Location, const TArray<float>& HeightData, FTileCoord TileCoord)
{
	FVector2D LocationXY(Location.X, Location.Y);
	
	// Calculate slope at the location
	FVector2D LocalPos = WorldToTileLocal(Location, TileCoord);
	float Slope = CalculateSlopeAtLocation(LocalPos, HeightData, TileCoord);
	
	// Default constraints (can be made configurable later)
	const float MaxAllowedSlope = 30.0f; // degrees
	const float MinAltitude = -1000.0f; // cm
	const float MaxAltitude = 10000.0f; // cm
	
	// Get terrain height at location
	float TerrainHeight = SampleHeightAt(LocationXY, HeightData, TileCoord);
	
	// Validate slope constraint
	bool bValidSlope = Slope <= MaxAllowedSlope;
	
	// Validate altitude constraint
	bool bValidAltitude = (TerrainHeight >= MinAltitude) && (TerrainHeight <= MaxAltitude);
	
	// Validate flat ground constraint using updated validation logic
	bool bValidFlatGround = ValidateFlatGround(Location, HeightData, TileCoord);
	
	// Add detailed diagnostic logging
	UE_LOG(LogPOIService, Warning, TEXT("POI validation at (%.1f,%.1f,%.1f):"), 
		Location.X, Location.Y, Location.Z);
	UE_LOG(LogPOIService, Warning, TEXT("  Terrain Height: %.2f cm"), TerrainHeight);
	UE_LOG(LogPOIService, Warning, TEXT("  Slope: %.2f° (max=%.2f°) -> %s"), 
		Slope, MaxAllowedSlope, bValidSlope ? TEXT("PASS") : TEXT("FAIL"));
	UE_LOG(LogPOIService, Warning, TEXT("  Altitude: %.2f cm (range=[%.1f,%.1f]) -> %s"),
		TerrainHeight, MinAltitude, MaxAltitude, bValidAltitude ? TEXT("PASS") : TEXT("FAIL"));
	UE_LOG(LogPOIService, Warning, TEXT("  Flat Ground: %s"), bValidFlatGround ? TEXT("PASS") : TEXT("FAIL"));
	
	bool bOverallValid = bValidSlope && bValidAltitude && bValidFlatGround;
	UE_LOG(LogPOIService, Warning, TEXT("  Overall Result: %s"), bOverallValid ? TEXT("VALID") : TEXT("INVALID"));
	
	return bOverallValid;
}
bool UPOIService::HasExistingPOIByName(const FString& Name) const
{
	if (Name.IsEmpty())
	{
		return false;
	}

	for (const TPair<FGuid, FPOIData>& Pair : AllPOIs)
	{
		if (Pair.Value.POIName.Equals(Name, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

TOptional<FPOISpawnRule> UPOIService::ResolveSpawnRuleForPOI(const FPOIData& POIData) const
{
	if (!BiomeService || POIData.OriginBiome == EBiomeType::None)
	{
		return TOptional<FPOISpawnRule>();
	}

	TArray<FPOISpawnRule> Rules = GetBiomePOIRules(POIData.OriginBiome);
	for (const FPOISpawnRule& Rule : Rules)
	{
		if (Rule.POIName.Equals(POIData.POIName, ESearchCase::IgnoreCase))
		{
			return Rule;
		}
	}

	return TOptional<FPOISpawnRule>();
}

void UPOIService::ReconcileLoadedPOI(FPOIData& POIData, const TOptional<FPOISpawnRule>& Rule, bool bLegacyData, const FTileCoord& OwningTile)
{
	if (HeightfieldService)
	{
		const FVector2D WorldXY(POIData.Location.X, POIData.Location.Y);
		const float TerrainHeight = HeightfieldService->SampleHeightWorldXY(WorldXY);
		if (!FMath::IsNearlyEqual(POIData.Location.Z, TerrainHeight, 1.0f))
		{
			POIData.Location.Z = TerrainHeight;
		}
	}

	if (FPOITileData* TileData = TilePOIs.Find(OwningTile))
	{
		for (FPOIData& TilePOI : TileData->POIs)
		{
			if (TilePOI.POIId == POIData.POIId)
			{
				TilePOI = POIData;
				break;
			}
		}
	}

	if (FPOIData* StoredEntry = AllPOIs.Find(POIData.POIId))
	{
		*StoredEntry = POIData;
	}
	else
	{
		AllPOIs.Add(POIData.POIId, POIData);
	}

	if (!HeightfieldService || !Rule.IsSet() || !bLegacyData)
	{
		return;
	}

	const FPOITerrainStampSettings& Stamp = Rule->TerrainStampSettings;
	if (Stamp.Operation == EPOITerrainStampMode::None)
	{
		return;
	}

	const float EffectiveRadius = Stamp.RadiusMeters > 0.0f ? Stamp.RadiusMeters : ValidationSettings.TerrainStampRadius;
	float EffectiveStrength = Stamp.Strength > 0.0f ? Stamp.Strength : ValidationSettings.TerrainStampStrength;

	switch (Stamp.Operation)
	{
	case EPOITerrainStampMode::Flatten:
		HeightfieldService->ModifyHeightfield(POIData.Location, EffectiveRadius, EffectiveStrength, EHeightfieldOperation::Flatten);
		break;
	case EPOITerrainStampMode::Raise:
		HeightfieldService->ModifyHeightfield(POIData.Location, EffectiveRadius, Stamp.RaiseHeightMeters * EffectiveStrength, EHeightfieldOperation::Add);
		break;
	case EPOITerrainStampMode::Smooth:
		HeightfieldService->ModifyHeightfield(POIData.Location, EffectiveRadius, EffectiveStrength, EHeightfieldOperation::Smooth);
		break;
	default:
		break;
	}
}
